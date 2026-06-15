// Copyright © 2014 CCP ehf.

#include "StdAfx.h"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <inttypes.h>

#include "BlueOS.h"
#include "IBluePersist.h"
#include "IBlueCallbackMan.h"
#include "BlueMemoryTracker.h"
#include "BlueResMan.h"
#include "BlueMemStream.h"
#include "BlueObjectRecycler.h"
#include "MotherLode.h"
#include "blueloginmemory.h"
#include "BluePaths.h"
#include "BlueResFile.h"
#include "BlueSocketLogger.h"
#include <BlueStatistics.h>
#include "crypto.h"
#include "errormessage.h"

#if BLUE_WITH_PYTHON
#include "ITaskletTimer.h"
#include "BluePython.h"
#endif

#ifdef _WIN32
#include "win32.h"
#include <shellapi.h>
#endif

#if CCP_STACKLESS
#include "BitPacker.h"
#include "BlueNet.h"
#include "BluePyCpp.h"
#endif

#include <sstream> // for message creation
#include <algorithm>
#include <iomanip>
#include "BlueTimeoutHandler.h"
#include <Scheduler.h>

static CcpLogChannel_t s_chOS = CCP_LOG_DEFINE_CHANNEL( "OS" );
static CcpLogChannel_t s_chErr = CCP_LOG_DEFINE_CHANNEL( "ERR" );

IBlueOS* BeOS = nullptr;
BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "os", BeOS );

IBlueOS* BlueGetBeOS()
{
	return BeOS;
}

CCP_STATS_DECLARE( logInfo,			"Blue/logInfo",			false,	CST_COUNTER_LOW, "Count of info logs" );
CCP_STATS_DECLARE( logNotice,		"Blue/logNotice",		false,	CST_COUNTER_LOW, "Count of notice logs" );
CCP_STATS_DECLARE( logWarn,			"Blue/logWarn",			false,	CST_COUNTER_LOW, "Count of warning logs" );
CCP_STATS_DECLARE( logErr,			"Blue/logErr",			false,	CST_COUNTER_LOW, "Count of error logs" );

#if CCP_STACKLESS

const int BNT_SIMCLOCK_SYNC_INIT = BlueNet::BlueNetKeyFromName( "SimClock::SycInit" );
const int BNT_SIMCLOCK_SYNC_UPDATE = BlueNet::BlueNetKeyFromName( "SimClock::SycUpdate" );
const int BNT_SIMCLOCK_SYNC_DETACH = BlueNet::BlueNetKeyFromName( "SimClock::SycDetach" );

#endif

//--------------------------------------------------------------------
// Class registration info
static bool GetBeOS(const Be::IID& riid, void** ppv )
{
	return BeOS->QueryInterface(riid, ppv);
}

// Use the decentralized class registration macros
BLUE_DEFINE_NO_REGISTER( BlueOS );
BLUE_REGISTER_CLASS_EX( BlueOS, GetBeOS, Be::ClassRegistration::DISABLE_PYTHON_CONSTRUCTION );

//--------------------------------------------------------------------
static bool HeapScrewed()
{
#ifdef _WIN32
	HANDLE dummy[1];
	DWORD numheaps = GetProcessHeaps( 1, dummy );
	HANDLE* heaps = new HANDLE[numheaps+10];
	DWORD got = GetProcessHeaps( numheaps, heaps );

	if( got <= numheaps )
	{
		for( DWORD i = 0; i < got; i++ )
		{
			if( !HeapValidate( heaps[i], 0, NULL ) )
			{
				delete[] heaps;
				return true;
			}
		}
	}

	delete[] heaps;
#endif

	return false;
}

#ifdef _WIN32

// This is not nice
HANDLE gBreakSleep;
HANDLE gBreakCarbonIo;
static unsigned int sNextScheduledIOWakeup = 0; // when the watchdog will wakeup
static bool sPendingWakeup = false; // is there a wakeup event outstanding?
#endif

// A class to format and create a windows error message.
class BlueOsError
{
public:
	BlueOsError( IBlueOS::OsErrorType errorcode );
	~BlueOsError();

	operator const char* () const {return message?message:"Unknown";}
private:
	void Format( unsigned long code );
	char *message;
};

#ifdef _WIN32

BlueOsError::BlueOsError( IBlueOS::OsErrorType error )
{
    message = 0;
    DWORD res = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        0,
		error,
        0, //MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
        (LPTSTR)&message,
        0,
        0);
    if (!res) {
        if (message)
            LocalFree(message);
        message = 0;
    } else {
        // remove trailing newlines
        size_t i = strlen(message);
        while(i-- > 0 && (message[i] == '\n' || message[i]=='\r'))
            message[i] = 0;
    }
}

BlueOsError::~BlueOsError()
{
	if (message)
    {
		LocalFree( message );
    }
}

#else

BlueOsError::BlueOsError( IBlueOS::OsErrorType error )
{
    char buffer[128];
    if( strerror_r( error, buffer, sizeof( buffer ) ) == 0 )
    {
        message = strdup( buffer );
    }
    else
    {
        message = nullptr;
    }
}

BlueOsError::~BlueOsError()
{
    if (message)
    {
        free( message );
    }
}

#endif

#if BLUE_WITH_PYTHON
struct TaskletSwitch
{
	const char* mName;
	PyObject* mContext;
};

static TaskletSwitch TASKLETS[] =
{
	{"BeOS::System", NULL},
	{"BeOS::Python", NULL},
	{"BeOS::Host App", NULL},
	{"Idle Thread", NULL},
	{"BeOS::SleepEx", NULL},
};

static const int BLUETASKLET = 0;
static const int PYTHONTASKLET = 1;
static const int HOSTTASKLET = 2;
static const int IDLETASKLET = 3;
static const int SLEEPTASKLET = 4;

//use this because the TASKLETS thing may be cleared at the end.
class SafeAutoTasklet : public AutoTasklet
{
public:
	SafeAutoTasklet(ITaskletTimer *timer, PyObject *context, bool active = true, TASKLETFLAGS flags = NONE) :
		AutoTasklet(timer, context, active && context != 0, flags)
	{}
};

#endif

static CBlueOS beOs;

//////////////////////////////////////////////////////////////////////
//
// Public member functions
//
//////////////////////////////////////////////////////////////////////

BlueOS::BlueOS() :
	mIndispensableTerminationSteps( "BlueOS/mIndispensableTerminationSteps" ),
	mErrorLog( "BlueOS/mErrorLog" ),
	mErrorLogMutex( "BlueOS", "mErrorLogMutex" ),
	mTickers( "BlueOS/mTickers" ),
	m_frameTimeWatchdog( "FrameTime" ),
	m_frameTimeTimeout( 0 ),
	mAdvanceTimeInPump( true ),
	mTimeStampIdx( -1 ),
	mLanguageID(L"EN")
{
	mPID = CcpGetCurrentProcessId();
	m_startupTime = CcpGetTickCount();

	BeOS = this;  //Set global IBlueOS pointer to this static object

	// init BeInfo struct
	mStructSize = sizeof (BeInfo);

	//Set the time
	SetTime();
	mRealTime = GetActualTime();
	mSimTime = mRealTime;
	mSimDilation = 1.0;
	mDilationSyncBaseWallclockTime = mRealTime;
	mDilationSyncBaseSimTime = mSimTime;
	mDilationSyncFactor = 1.0;
	mDilationSyncMaster = 0;

	mDynamicSimDilationEnabled = false;
	mSimClockLockedToRealClock = true;
	mLastOverloadedTime = mRealTime;
	mLastUnderloadedTime = mRealTime;
	// This bunch are reasonable defaults.  It's expected and encouraged for games to override these defaults to their choosing.
	mMinSimDilation = .1;
	mMaxSimDilation = 1;
	mDilationOverloadThreshold = 10 * 1000 * 10000; // 10 seconds
	mDilationUnderloadThreshold = 2 * 1000 * 10000; // 2 seconds
	mDilationOverloadAdjustment = .8254; // .1x factor with 12 applications - go from 1 to .1 in 2 minutes with 10sec intervals
	mDilationUnderloadAdjustment = 1.059254; // 10x factor with 40 applications - go from .1 to 1 in 80 seconds with 2sec intervals

	mExitTime = mExternalTime = 0;

#if CCP_STACKLESS
    mTimeSyncAdjust = 0;
    mTimeSyncAdjustFactor = .02;
#endif

	mTimer.Reset();
	mTimeWarp = 1.0; //obsolete
	mTimeAdjusted = 0; //obsolete

	mDebugLevel = 0;

	mBuildno = -1;

	// New order
	mInsidePump = false;

	mMiniDump = false;

#if BLUE_WITH_PYTHON
	mFrameClock = 0;
#endif

	mUseRDTSC = false;

	InitSlug();
	InitVariableTicking();

	CcpStatistics::Init();
}


BlueOS::~BlueOS()
{
#if BLUE_WITH_PYTHON
	Py_XDECREF(mFrameClock);
#endif
}


BlueOS::Ticker::Ticker( IBlueEvents *cb, void* cookie ) :
	mCb( cb ),
	mCookie( (const char*)cookie )
{
}


void BlueOS::RegisterForTicks( IBlueEvents *cb, void* cookie )
{
	Ticker t(cb, cookie);
	TickIt it = std::find( mTickers.begin(), mTickers.end(), t );
	if( it == mTickers.end() )
	{
		mTickers.push_back(t);
	}
}


void BlueOS::UnregisterForTicks( IBlueEvents *cb, void* cookie )
{
	// We have to remove the ticker, to relinquish our reference to it.
	// This isn't bad for the ordering of tickers, it appears to be the
	// order of initial tickers that matters.  Things like ballparks can
	// go and be reinserted at the end.
	TickIt it = std::find( mTickers.begin(), mTickers.end(), Ticker(cb, cookie) );
	if( it != mTickers.end() )
	{
		mTickers.erase(it);
	}
}


void _Py_FatalErrorFunc( const char* _func, const char* msg )
{
	CCP_LOGERR_CH( s_chOS, "Py_FatalError: %s", msg );
	fprintf( stderr, "Fatal Python error: %s\n", msg );
#ifdef _WIN32
	RaiseException( 0xE0000011, EXCEPTION_NONCONTINUABLE, 0, NULL );
	TerminateProcess( GetCurrentProcess(), -3 ); // shouldn't get here
#else
	kill( getpid(), SIGKILL );
	__builtin_unreachable();
#endif
}

//--------------------------------------------------------------------
// Frame rate is capped at ~100 max frames per second (10ms per frame).
// If there is time remaining in the frame, then sleep for the available
// duration. Otherwise, yield to give other threads an opportunity to run
// before continuing execution.
void BlueOS::Sleep()
{
	auto now = std::chrono::steady_clock::now();
	// We don't want to sleep for less than 1 millisecond, since switching out of and back
	// into a large thread can be quite heavy, so this could be a waste of resources.
	if( now < mNextFrame )
	{
		std::this_thread::sleep_until( mNextFrame );
	}
	else
	{
		std::this_thread::yield();
	}
	// Set the next frame to 1 millisecond before the actual desired value.
	// This is because on Windows, the granularity of timers used for sleep has been set to 1ms in exefile.
	// It doesn't get any more precise than that on Windows, so we may well oversleep by up to 1ms,
	// even if there is nothing else for the processor to do than run our thread.
	mNextFrame = std::chrono::steady_clock::now() + std::chrono::milliseconds( m_desiredFrameTimeMilliseconds - 1 );
}

//------------------------------------------------------------------------------
// Variable DeltaT Support
//
// This section contains a first-pass at support for a variable-deltaT ticking system (for Incarna etc).
// Until this has stabilised, it is supported *alongside* the previous fixed-delta "catchup ticks" system.
//
// To make it easier to analyse these changes in isolation, I have tried to group them into contiguous
// blocks of code. However, in future, I would expect to split them up a bit to reduce bloating of Blue.
//

// NB: "CST_TIME" stats are interpreted as being in units of seconds, with an apparent rendering-scale
//     cap of 0.300 (i.e. 300ms). I don't want the capping, so I'm using the "CST_COUNTER_HIGH" type
//     instead, with units of milliseconds.
//
CCP_STATS_DECLARE( STAT_actualDeltaT, "Blue/actualDeltaT", true, CST_COUNTER_HIGH,
				  "Actual Delta-Time value" );
CCP_STATS_DECLARE( STAT_smoothedDeltaT, "Blue/smoothedDeltaT", true, CST_COUNTER_HIGH,
				  "Smoothed Delta-Time value" );
CCP_STATS_DECLARE( STAT_usedDeltaT, "Blue/usedDeltaT", true, CST_COUNTER_HIGH,
				  "Used Delta-Time value" );

// Copied from their original definitions in "GameWorld\StdAfx.h"
// TODO: I know this is ugly, but I don't want to complicate the dependency situation right now
//       Will try to clean this up later. 2011/03/15.
//
#define ONE_MILLISECOND 10000
#define ONE_MILLISECOND_F 10000.0f
#define ONE_SECOND (ONE_MILLISECOND * 1000)
#define ONE_SECOND_F (ONE_MILLISECOND_F * 1000)


void BlueOS::InitVariableTicking()
{
	// If we aren't using a measured delta, use this nominal value instead
	mNominalDeltaTSec = 0.033f; // i.e. GAMEWORLD_S_PER_TICK;

	const bool START_WITH_CONSTANT_TICKS = false; //true;

	if( START_WITH_CONSTANT_TICKS )
	{
		// The original behaviour is equivalent to this...
		mUseNominalDeltaT = true;
	}
	else
	{
		// Full variable-tick support is enabled by this...
		mUseNominalDeltaT = false;
	}

	// The smoothed delta introduces artefacts of its own, which aren't always desirable.
	mUseSmoothedDeltaT = false;

	mTimeScaler = 1.0f;
}

void BlueOS::InitSlug()
{
	mSlugTimeMinMs = 0.0f;
	mSlugTimeMaxMs = 0.0f;
	mSlugTimeCurrentMs = 0.0f;
	mSlugTimeDeltaMs = 1.0f;
	mIsSlugTimeIncreasing = true;
}

void BlueOS::UpdateSlugTime( float deltaTime )
{
	if( mIsSlugTimeIncreasing )
	{
		mSlugTimeCurrentMs += mSlugTimeDeltaMs * deltaTime * 1000;
		if( mSlugTimeCurrentMs > mSlugTimeMaxMs )
		{
			mSlugTimeCurrentMs = mSlugTimeMaxMs;
			mIsSlugTimeIncreasing = false;
		}
	}
	else
	{
		mSlugTimeCurrentMs -= mSlugTimeDeltaMs * deltaTime * 1000;
		if( mSlugTimeCurrentMs < mSlugTimeMinMs )
		{
			mSlugTimeCurrentMs = mSlugTimeMinMs;
			mIsSlugTimeIncreasing = true;
		}
	}
}

void BlueOS::DoSlug( float deltaTime )
{
	UpdateSlugTime( deltaTime );
	CcpThreadSleep( int( mSlugTimeCurrentMs ) );
}

void BlueOS::ComputeTimeValues(Be::Time* ptrActualTime, float* ptrDeltaT_sec)
{
	CCP_STATS_ZONE( "BlueOS/ComputeTimeValues" );


	// The so-called "ActualTime" is a fairly raw wallclock-time from the system,
	// but it seems to be quite noisy.
	//
	Be::Time actualTime, actualDeltaT;

	static Be::Time sPrevActualTime = GetActualTime(); // for first-time init

	actualTime = GetActualTime();

	actualDeltaT = actualTime - sPrevActualTime;
	if( actualDeltaT < 0 )
	{
		// NB: "actualTime" doesn't always go forwards...
		//     e.g. it can be pulled backwards when "Synchronizing clocks"
		CCP_LOG( "Clamping negative actualDeltaT of %f secs to zero", actualDeltaT/ONE_SECOND_F );
		actualDeltaT = 0;
	}
	CCP_STATS_SET( STAT_actualDeltaT, actualDeltaT / ONE_MILLISECOND_F );
	sPrevActualTime = actualTime;


	// The so-called "SmoothedTime" is a filtered version of the raw wallclock-time,
	// but the filtering creates some artefacts of it's own e.g. a latent response
	// to genuine changes in the raw delta.
	//
	Be::Time smoothedTime, smoothedDeltaT;

	static Be::Time sPrevSmoothedTime = GetSmoothedTime(); // for first-time init

	smoothedTime = GetSmoothedTime();

	smoothedDeltaT = smoothedTime - sPrevSmoothedTime;

#if CCP_STACKLESS
    // See if we have a sync nudge to apply - ignoring sub-millisecond shifts
    if( mTimeSyncAdjust > ONE_MILLISECOND || mTimeSyncAdjust < -ONE_MILLISECOND )
    {
        Be::Time maxMagnitude = Be::Time(smoothedDeltaT * mTimeSyncAdjustFactor);
        if( maxMagnitude < 0 )
        {
            maxMagnitude = -maxMagnitude;
        }

        // Clamp our adjustment to the max magnitude allowed
        Be::Time tickAdjust = std::min( maxMagnitude, std::max( -maxMagnitude, mTimeSyncAdjust ) );

        // Apply the sucker
        smoothedTime += tickAdjust;
        smoothedDeltaT += tickAdjust;
        mUTCAdj += tickAdjust;
        mTimeSyncAdjust -= tickAdjust;
    }
#endif

	if( smoothedDeltaT < 0 )
	{
		// NB: The smoothing algorithm doesn't guarantee always positive deltas!
		//     (this it true even if/when the underlying clock *is* increasing)
		CCP_LOG( "Clamping negative smoothedDeltaT of %f secs to zero", smoothedDeltaT/ONE_SECOND_F );
		smoothedDeltaT = 0;
	}
	CCP_STATS_SET( STAT_smoothedDeltaT, smoothedDeltaT / ONE_MILLISECOND_F );
	sPrevSmoothedTime = smoothedTime;

	// Before any of these changes were made, the "smoothedTime" was used as the system's
	// absolute "mTime". I don't want to risk changing that, so I'll leave it like that.

	// Ken: time to risk it!
	// BrianB 9Nov2011:  That didn't work out very well, basing real time off of actualTime caused serious camera problem.  See http://defects/issue.asp?ISID=65940
	mRealTime = smoothedTime;

	if( mSimClockLockedToRealClock )
	{
		mSimTime = mRealTime;  // We are locked.  Don't even bother with anything else.
	}
	else
	{
		while( !mPendingDilationEvents.empty() )
		{
			if( mRealTime > mPendingDilationEvents.top().mNextDilationEventWallclockTime )
			{
				BlueOS::PendingDilationEvent newEvent = mPendingDilationEvents.top();
				mPendingDilationEvents.pop();

				CCP_LOG_CH( s_chOS, "TIDI New sync base - %f, %" PRId64, newEvent.mNextDilationFactor, mRealTime);
				// The event now becomes our current sync base
				mSimDilation = newEvent.mNextDilationFactor;
				mDilationSyncFactor = newEvent.mNextDilationFactor;
				mDilationSyncBaseSimTime = newEvent.mNextDilationEventSimTime;
				mDilationSyncBaseWallclockTime = newEvent.mNextDilationEventWallclockTime;
			}
			else
			{
				// The next event is in the future, break the loop so we carry on
				break;
			}
		}

		if( mDynamicSimDilationEnabled )
		{
			// The server knows exactly where it should be
			mSimTime = Be::Time(mDilationSyncBaseSimTime + (mRealTime - mDilationSyncBaseWallclockTime) * mSimDilation);
		}
		else
		{
			if( mDilationSyncMaster != 0 )
			{
				// Find the time we should be based on what our master has told us
				Be::Time desiredSimTime = Be::Time(mDilationSyncBaseSimTime + (mRealTime - mDilationSyncBaseWallclockTime) * mDilationSyncFactor);
				Be::Time error = Be::Time(mSimTime + smoothedDeltaT * mSimDilation) - desiredSimTime;

				// Aim to have a quarter of the error corrected in one real second
				mSimDilation = mDilationSyncFactor - (error/4) / ONE_SECOND_F;
				mSimDilation = std::max(0.0, mSimDilation);  // Don't allow negative time.
				mSimDilation = std::min(mSimDilation, mDilationSyncFactor * 2);  // Don't allow going more than twice as fast as we're supposed to.
			}

			mSimTime += Be::Time(smoothedDeltaT * mSimDilation);
		}
	}

	// Now, we must decide which delta value to actually use, and convert it into seconds...
	float deltaT_sec;

	if( mUseNominalDeltaT )
	{
		deltaT_sec = mNominalDeltaTSec;
	}
	else if( mUseSmoothedDeltaT )
	{
		deltaT_sec = smoothedDeltaT / ONE_SECOND_F;
	}
	else
	{
		deltaT_sec = actualDeltaT / ONE_SECOND_F;
	}

	deltaT_sec *= mTimeScaler; // Test behaviour by scaling

	if( deltaT_sec < 0 )
	{
		// Negative deltas are bad, of course, and will cause PhysX to choke (badly).
		// By this point, I should have already prevented them though, so this is
		// effectively an "assertion"...
		CCP_LOGERR( "ERROR: deltaT_sec < 0 --- value is %f", deltaT_sec );
	}

	// NB: I'm putting these timer stats into a millisecond scale, and must multiply accordingly
	CCP_STATS_SET( STAT_usedDeltaT, deltaT_sec * 1000 );


	// Set the formal outputs of this function
	*ptrActualTime = actualTime;
	*ptrDeltaT_sec = deltaT_sec;
}

#if !__APPLE__
void BlueOS::PumpOS()
{
    PumpOSInternal();
}
#endif

void BlueOS::PumpOSInternal()
{
	CCP_STATS_ZONE( "BlueOS/PumpOS" );
	if( BeCrashes )
	{
		char timeStr[64];
		float interval = float( double( CcpGetTickCount() - m_startupTime ) / 1000 );
		sprintf_s( timeStr, "%.1f", interval );

		BeCrashes->SetCrashKeyValue( "timeSinceStartup", timeStr );
	}

	if( CcpIsDebuggerPresent() )
	{
		m_frameTimeWatchdog.Stop();
	}

	m_frameTimeWatchdog.Tick();

#if BLUE_WITH_PYTHON
	PyOS->PyFlushError("PumpOS::start");
#endif

#if CCP_STACKLESS
	//if no other tasklets are running, need to periodically reset timeslice
	PyOS->GetTaskletTimer()->TimesliceReset();
#endif

	if( mInsidePump )
	{
		return; //avoid reentrancy
	}

	mInsidePump = true;

	if( mDebugLevel > 0 && HeapScrewed() )
	{
		CCP_LOGERR_CH( s_chOS, "Heap corrupt - I'm at the top of the blue pump");
	}

#if CCP_STACKLESS

	SafeAutoTasklet _at(PyOS->GetTaskletTimer(), TASKLETS[BLUETASKLET].mContext);
	{
		CCP_STATS_ZONE( "BlueOS/PumpOS/DoSleep" );
		Sleep();
	}

	BeNet->DeliverCPackets(); // dispatch any waiting callbacks
#endif


	// Variable-DeltaT Support...

	Be::Time actualTime = 0;
	float deltaT_sec = 0.0f;

	if( mAdvanceTimeInPump )
	{
		// Compute the DeltaT for this tick (and "actualTime" for legacy purposes)
		ComputeTimeValues(&actualTime, &deltaT_sec);
	}

	if( mExitTime )
	{
		mExternalTime = actualTime-mExitTime; //the time spent outside BlueOS
	}

	// The "Slug" is a deliberately crude diagnostic feature which introduces a (potentially variable) delay into
	// the mainloop. This helps to test our frame-rate compensation. Added by Patrick Kerr.
	//
	DoSlug( deltaT_sec );

#if BLUE_WITH_PYTHON
	// Switch to python as soon as possible, since we woke up to service python tasklets.
	{
		SafeAutoTasklet _at2(PyOS->GetTaskletTimer(),TASKLETS[PYTHONTASKLET].mContext);
		PyOS->PumpPython(false);
		PyOS->PyFlushError("PumpOS::end PumpPython");
	}
#endif

	EvaluateTimeDilation();

	TickTickers();

	// Mark timestamp in rotating timestamps array
	m_pumpTicksTotal++;
	mTimeStampIdx = (mTimeStampIdx+1) % nTimeStamps;
	mTimeStamps[mTimeStampIdx] = actualTime;

	// Compute averate fps, over mFpsRefreshRate steps
	int steps = (int)(mFps * mFpsRefreshRate * 1e-7);
	if( steps >= nTimeStamps )
	{
		steps = nTimeStamps-1;
	}
	else if( steps < 1 )
	{
		steps = 1;
	}
	int last = mTimeStampIdx - steps;
	if( last < 0 )
	{
		last += nTimeStamps;
	}
	double denominator = (actualTime - mTimeStamps[last]) * 1e-7;
	mFps = denominator ? steps/denominator : 0.0;


	if( mDebugLevel > 0 && HeapScrewed() )
	{
		CCP_LOGERR_CH( s_chOS, "Heap corrupt - I'm at the bottom of the blue pump");
	}

	BeRecycler->Update( GetCurrentFrameTime() );
	BeClasses->ProcessPendingDeletes();

	BeMemoryTracker->Update();

	CaptureLogCountsToStats();

	if( g_statistics )
	{
		g_statistics->Update();
	}

#if BLUE_WITH_PYTHON
	PyOS->PyFlushError("PumpOS::end");
#endif
	mExitTime = GetActualTime();
	mInsidePump = false;
}

//////////////////////////////////////////////////////////////////////
//
// IBlueOS interface methods
//
//////////////////////////////////////////////////////////////////////

bool BlueOS::Startup( int pyOptimizeFlag )
{
	//start up crypto
	if( !InitCrypto() )
	{
		SetError( BEDEF, Clsid(), "BlueOS::Startup(): InitCrypto failed" );
		return false;
	}

	// pump yielding
	mSleepTime = 1;
	mOverrideFG = 0;

	// framerate counters
	m_pumpTicksTotal = 0;
	m_ioRunsTotal = 0;
	mTimeStampIdx = -1;
	mFps = 0.0;
	mFpsRefreshRate = 10000000;
	mLockFramerate = 0.0;  //TODO: not used, but would be

#if BLUE_WITH_PYTHON
	CCP_LOG( "Creating PyOS" );
	BeClasses->CreateInstance(GetBluePyOSClsid(), GetIBluePyOSIID(), (void**)&PyOS);
	if( !PyOS )
	{
		goto FAIL;
	}

	PyOS->SetPackaged( mPackaged );
	PyOS->mOptimizeFlag = pyOptimizeFlag;
	if( !PyOS->Startup() )
	{
		goto FAIL;
	}

	for (int i = 0; i < sizeof TASKLETS / sizeof TASKLETS[0]; i++)
	{
		TASKLETS[i].mContext = PyUnicode_InternFromString(TASKLETS[i].mName);
	}
#endif

	return true;

#if BLUE_WITH_PYTHON
FAIL:
	if( PyOS )
	{
		PyOS->Shutdown(1);
	}

	if( BeMotherLode )
	{
		BeMotherLode->Shutdown();
		BeMotherLode->Unlock();
		BeMotherLode = 0;
	}

	//turn off pyos
	if( PyOS )
	{
		PyOS->Shutdown(2);
		PyOS->Unlock();
		PyOS=0;
	}
#endif
	return false;
}

bool BlueOS::RunStackless()
{
#if CCP_STACKLESS
	PyObject* me = BlueWrapObjectForPython( this );
	if( me )
	{
		//Just call StacklessMain callable
		PyObject* stacklessMainCallable = PyObject_GetAttrString( me, "StacklessMain" );

		if( !PyCallable_Check( stacklessMainCallable ) )
		{
			PyOS->PyError();
			return false;
		}

		// Call "StacklessMain" which contains main game loop, see BlueOS::PyStacklessMain
		PyObject* ret = PyObject_Call( stacklessMainCallable, nullptr, nullptr );

		if( !ret )
		{
			PyOS->PyError();
			return false;
		}

		Py_DECREF( ret );
	}
	else
	{
		PyOS->PyError();
		return false;
	}

	return true;

#else

	return false;

#endif
}

#if BLUE_WITH_PYTHON
PyObject* BlueOS::PyStacklessMain( PyObject* args )
{
#if CCP_STACKLESS
	if( HasStartupArg( L"telemetryServer" ) )
	{
		std::wstring server = GetStartupArgValue( L"telemetryServer" );
		std::string aServer( CW2A( server.c_str() ) );
		// large number because some of our usage of scheduler spawn thousands of tasklets (e.g. sol node and proxy)
		g_statistics->SetTelemetryMaxThreadCount( 32768 );
		g_statistics->StartTelemetry( aServer );
	}

	// StartTelemetry triggers a start on the next Update, so do
	// an update here.
	if( g_statistics )
	{
		g_statistics->Update();
	}

	//autoexec can reside in a .zip lib
	PyObject *module = PyImport_ImportModule( "autoexec" );
	if( !module )
	{
		// PyErr_SetString( PyExc_RuntimeError, "autoexec module not found" );
		return NULL;
	}

	//Now, we really shouldn't run code as a side effect of import, in
	//particular, if it goes into a long running loop, because that will
	//hold the import lock frozen.  Instead, we attempt to run the "run" method
	//of autoexec.
	PyObject *run = PyObject_GetAttrString( module, "run" );
	Py_DECREF( module );
	if ( run )
	{
		PyObject *result = PyObject_CallObject( run, NULL );
		Py_DECREF( run );
		if ( !result )
		{
			return NULL;
		}
		Py_DECREF( result );
	}
	else
	{
		PyErr_Clear(); // no run function supplied.
	}

	bool quit = false;
	while( !quit )
	{
		{
			CCP_STATS_ZONE( "Main loop");
#if _WIN32
			MSG msg;
			while(PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
			{
				if (msg.message ==	WM_QUIT)
				{
					quit = true;
					BlueLogInMemory::GetInstance()->ExecuteSaveLogCallback();
					break;
				}
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
#endif
			BeOS->PumpOS();
		}
	}
#endif

	Py_INCREF(Py_None);
	return Py_None;
}
#endif


void BlueOS::Terminate( int retCode )
{
	CCP_LOG_CH( s_chOS, "Terminating process by request - returning %d\n", retCode );

	for( unsigned int ix = 0; ix < mIndispensableTerminationSteps.size(); ++ix)
	{
		#ifdef _WIN32
			__try
			{
				mIndispensableTerminationSteps[ix]();
			}
			__except( EXCEPTION_EXECUTE_HANDLER )
			{
				CCP_LOGERR_CH( s_chOS, "Exception thrown when executing a shutdown step" );
			}
		#else
			mIndispensableTerminationSteps[ix]();
		#endif
	}

	//this enables us to save out the in memory logger when we close jessica.
	BlueLogInMemory::GetInstance()->ExecuteSaveLogCallback();

	CCP_LOG_CH( s_chOS, "Shutdown callbacks finished, terminating" );

	fflush( stderr );
	fflush( stdout );

	// This should ensure that the in-memory logs are flushed
	// ensuring that any exceptions right before the terminate are still sent out!
	// This only handles the in memory logging, not the other handlers
	FlushSocketLogger();

#if _WIN32
	BOOL result = TerminateProcess( GetCurrentProcess(), retCode );
	if( !result )
	{
		CCP_LOGERR_CH( s_chOS, "TerminateProcess failed: %d", GetLastError() );
	}

	DWORD waitResult = WaitForSingleObject( GetCurrentProcess(), 5000 );
	switch( waitResult )
	{
		case WAIT_OBJECT_0:
			// Process has terminated
			break;

		case WAIT_TIMEOUT:
			CCP_LOGERR_CH( s_chOS, "Waiting for process termination timed out" );
			break;

		case WAIT_FAILED:
			CCP_LOGERR_CH( s_chOS, "Waiting for process termination failed: %d", GetLastError() );
			break;

		default:
			CCP_LOGERR_CH( s_chOS, "Waiting for process termination returned %d", waitResult );
			break;
	}
	FlushSocketLogger();

	// If we ever get here something has gone horribly wrong - induce a crash so
	// we learn about this via the crash dumps.
	CcpCrashOnPurpose();
#else
	// The exit() handler does cleanup which causes a crash on macOS because we are
	// bad citizens and have memory issues which we haven't taken care of yet
	// in the meantime we use _exit which quits without cleanup
	_exit( retCode );
#endif
}

bool BlueOS::IsOnMainTasklet()
{
	return BeResMan->IsOnMainThread();
}

void BlueOS::SetTime(Be::Time time)
{
	if( !time )
	{
        //Get the time, as precisely as we can (it's updated very rarely)
        auto t0 = TimeNow();
        for (;;)
        {
            time = TimeNow();
            if( t0 != time )
            {
                break; //it just changed!  okay, we are all set.
            }
        }
	}
	Be::Time wc = mWallclock.Get();
	mUTCAdj = time - wc;
	Be::Time diff = time - mRealTime;

	mRealTime = time;
	mSimTime += Be::Time(diff * mSimDilation);
#if BLUE_WITH_PYTHON
	if( mFrameClock )
	{
		PyObject *r = PyObject_CallMethod(mFrameClock, (char*)"Rebase", 0);
		if( !r )
		{
			PyOS->PyError();
		}
		Py_XDECREF(r);
	}
#endif
}

// backdoor for synchro.cpp
void SetBlueTime(Be::Time time)
{
	beOs.SetTime(time);
}


Be::Time BlueOS::GetActualTime()
{
	static CcpMutex s_getActualTimeMutex( "GetActualTime", "BeOS" );
	CcpAutoMutex locker( s_getActualTimeMutex );
	return mUTCAdj + mWallclock.Get(); // Adjust for server sync
}

Be::Time BlueOS::GetSmoothedTime()
{
#if BLUE_WITH_PYTHON
	if( !mFrameClock )
#endif
	{
		return GetActualTime();
	}

#if BLUE_WITH_PYTHON
	PyObject *t = PyObject_CallMethod(mFrameClock, const_cast<char*>( "Sample" ), 0);
	if( !t )
	{
		PyOS->PyError();
		return GetActualTime();
	}

	Be::Time time = PyLong_AsLongLong(PyTuple_GET_ITEM(t, 0));
	Py_DECREF(t);

	if( time == -1 && PyErr_Occurred() )
	{
		PyOS->PyError();
		return GetActualTime();
	}

	return time;
#endif
}

void BlueOS::RegisterIndispensableTerminationStep( TerminationCallback* callback )
{
	mIndispensableTerminationSteps.push_back( callback );
}

void BlueOS::SetError( long error,	const Be::Clsid* reporter, const char* format, ... )
{
	CcpAutoMutex lock( mErrorLogMutex );

	if( mTurnOffSetError )
	{
		//wow, creative hack!
		mTurnOffSetError = false;
		return;
	}

	if( error == BEFLUSH )
	{
		//Put out the stuff to the logger!
		char* str;
		FormatError(&str);
		if( str[0] )
		{
			CCP_LOGERR_CH( s_chErr, "%s", str);
		}
		CCP_FREE( str );
	}


	if( error == BEFLUSH || error == BECLEAR )
	{
		for( ErrIt i = mErrorLog.begin(); i != mErrorLog.end(); ++i )
		{
			CCP_DELETE[] (char*)(*i).mDescription;
		}
		mErrorLog.clear();
		return;
	}

	//Emergency vent.  We don't want to fill the memory with errors, oh no.
	if( mErrorLog.size() > 256 )
	{
		CCP_LOGERR_CH( s_chErr, "Autoflushing blue error log:");
		SetError(BEFLUSH, 0, "");
		SetError(BECLEAR, 0, "");
	}

	// Set up the error structure
	IBlueOS::Error err;
	err.mError = error;
	err.mOsError = 0;
	if( error == BE32 )
	{
#ifdef _WIN32
		err.mOsError = GetLastError();
#else
        err.mOsError = errno;
#endif
		BlueOsError winerr( err.mOsError );
		CCP_LOGERR( winerr );
	}

	err.mSource = reporter;

	static Be::Time counter = 0;
	err.mTimestamp = counter; counter += 1;

	// optionally format the string and varargs
	err.mDescription = NULL;
	if( format )
	{
		char buff[8192];
		int buffsize = sizeof(buff)-1;
		va_list va;
		va_start(va, format);
		buff[0]=0;
		buff[buffsize]=0;
		vsnprintf_s(buff, sizeof(buff), _TRUNCATE, format, va);
		va_end(va);

		size_t wrote = strlen(buff);
		char* msg = CCP_NEW("Error/mDescription") char[wrote + 1];
		strcpy_s(msg, wrote+1, buff);
		err.mDescription = msg;

		// Message may contain format specifiers, hence the formatting below
		CCP_LOGERR( "%s", buff );
	}

	mErrorLog.push_back(err);
}


const IBlueOS::Error* BlueOS::GetError( long index )
{
	CcpAutoMutex lock( mErrorLogMutex );

	return index < (long)mErrorLog.size() ? &mErrorLog[index] : NULL;
}


void BlueOS::FormatError( char** errorstring )
{
	CcpAutoMutex lock( mErrorLogMutex );

	if( mErrorLog.empty() )
	{
		*errorstring = (char*)CCP_MALLOC( "errorString", 1 );
		(*errorstring)[0] = '\0';
		return;
	}

	std::ostringstream stream;
	// put a LF in front for cosmetic purposes
	stream << '\n';

	for( BlueOS::ErrorLog::const_reverse_iterator err = mErrorLog.rbegin(); err != mErrorLog.rend(); ++err )
	{
		stream << (int)err->mTimestamp << " : ";

		// prettyprint error code
		if( err->mError == BE32 )
		{
			BlueOsError winerr( err->mOsError );
			stream << '{' << err->mOsError << ':' << (const char*)winerr << '}';
		}
		else if( err->mError != BEDEF )
		{
			stream << "[Unknown]";
		}

		if( err->mError != BEDEF  )
		{
			stream << " in ";
		}

		if( err->mSource )
		{
			stream << err->mSource->GetModule() << '.' << err->mSource->GetName();
		}
		else
		{
			stream << "in unknown source";
		}

		// Now, find each newline in the description and indent accordingly
		const char *descriptionPtr = err->mDescription;
		if( descriptionPtr )
		{
			stream << ": ";
		}
		while( descriptionPtr && *descriptionPtr )
		{
			const char *newline = strchr(descriptionPtr, '\n');
			if( newline )
			{
				const char *next = newline+1;
				// Skip any whitespace after the newline
				while( *next == '\n' || *next == '\r' || *next == ' ' )
				{
					++next;
				}

				stream.write(descriptionPtr, std::streamsize(newline-descriptionPtr));

				if( *next )
				{
					stream << "\n    ";
				}

				descriptionPtr = next;
			}
			else
			{
				stream << descriptionPtr << '\n';
				descriptionPtr = NULL;
			}
		}
	}


	//copy the stuff into the result;
    std::string error = stream.str();
	size_t len = error.size() + 1;

	*errorstring = (char*)CCP_MALLOC( "errorstring", len*sizeof(char) );

	if( *errorstring )
	{
		strncpy_s( *errorstring, len, error.c_str(), _TRUNCATE);
	}

}


//--------------------------------------------------------------------
// Config file
//--------------------------------------------------------------------


BeInfo* BlueOS::GetInfo()
{
	return this;
}



#if BLUE_WITH_PYTHON
// Depricated, please use the GetXTime functions below.
PyObject* BlueOS::PyGetTime(PyObject* args)
{
	Be::Time time;
	int benchmark = 0;

	if (!PyArg_ParseTuple(args, "|i", &benchmark))
		return NULL;

	if (benchmark)
	{
		time = GetActualTime();
	}
	else
	{
		time = mRealTime;
	}

	return PyLong_FromLongLong(time);
}

PyObject* BlueOS::PyGetWallclockTime(PyObject* args)
{
    return PyLong_FromLongLong(mRealTime);
}

PyObject* BlueOS::PyGetWallclockTimeNow(PyObject* args)
{
	Be::Time time = GetActualTime();

	return PyLong_FromLongLong(time);
}

PyObject* BlueOS::PyGetSimTime(PyObject* args)
{
	return PyLong_FromLongLong(mSimTime);
}

PyObject* BlueOS::PyGetCycles(PyObject* args)
{
	PyObject* ret = PyTuple_New(2);

	PyTuple_SET_ITEM(ret, 0, PyLong_FromLongLong(mTimer.GetCycles()));
	PyTuple_SET_ITEM(ret, 1, PyLong_FromLongLong(mTimer.GetFreq()));

	return ret;
}


PyObject* BlueOS::PySetTime(PyObject* args)
{
	PyObject* time;

	if( !PyArg_ParseTuple(args, "O!", &PyLong_Type, &time) )
	{
		return NULL;
	}

	Be::Time t = PyLong_AsLongLong(time);
	if (t == -1 && PyErr_Occurred())
	{
		return 0;
	}

	SetTime(t);

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* BlueOS::PyTimeDiffInMs(PyObject* args)
{
	PyObject* t1;
	PyObject* t2 = NULL;

	if( !PyArg_ParseTuple(args, "O!O!", &PyLong_Type, &t1, &PyLong_Type, &t2) )
	{
		return NULL;
	}

	Be::Time time1 = PyLong_AsLongLong(t1);
	if( time1 == -1 && PyErr_Occurred() )
	{
		return 0;
	}

	Be::Time time2 = PyLong_AsLongLong(t2);
	if( time1 == -1 && PyErr_Occurred() )
	{
		return 0;
	}

	Be::Time diff = time2 - time1;

	// go from 100 nanosec. to millisec.
	diff /= 10000;

	if( diff >= LONG_MAX || diff <= LONG_MIN )
	{
		return PyLong_FromLongLong( diff );
	}

	return PyLong_FromLong((long)diff);
}


PyObject* BlueOS::PyTimeDiffInUs(PyObject* args)
{
	PyObject* t1;
	PyObject* t2 = NULL;

	if( !PyArg_ParseTuple(args, "O!O!", &PyLong_Type, &t1, &PyLong_Type, &t2) )
	{
		return NULL;
	}

	Be::Time time1 = PyLong_AsLongLong(t1);
	if( time1 == -1 && PyErr_Occurred() )
	{
		return 0;
	}

	Be::Time time2 = PyLong_AsLongLong(t2);
	if( time2 == -1 && PyErr_Occurred() )
	{
		return 0;
	}

	Be::Time diff = time2 - time1;

	// go from 100 nanosec. to microsec.
	diff /= 10;

	if( diff >= LONG_MAX || diff <= LONG_MIN )
	{
		return PyLong_FromLongLong( diff );
	}

	return PyLong_FromLong((long)diff);
}


PyObject* BlueOS::PyTimeFromDouble(PyObject* args)
{
	double secs;

	if( !PyArg_ParseTuple(args, "d", &secs) )
	{
		return NULL;
	}

	return PyLong_FromLongLong(TimeFromDouble(secs));
}


PyObject* BlueOS::PyTimeAsDouble(PyObject* args)
{
	PyObject* t = NULL;

	if( !PyArg_ParseTuple(args, "O!", &PyLong_Type, &t) )
	{
		return NULL;
	}

	Be::Time time = PyLong_AsLongLong(t);
	if (time == -1 && PyErr_Occurred()) return 0;

	return PyFloat_FromDouble(TimeAsDouble(time));
}


PyObject* BlueOS::PyTimeAddSec(PyObject* args)
{
	PyObject* time;
	double secs;

	if( !PyArg_ParseTuple(args, "O!d", &PyLong_Type, &time, &secs) )
	{
		return NULL;
	}

	Be::Time ltime = PyLong_AsLongLong(time);
	if( ltime == -1 && PyErr_Occurred() )
	{
		return 0;
	}

	return PyLong_FromLongLong(ltime + TimeFromDouble(secs));
}


PyObject* BlueOS::PyGetTimeParts(PyObject* args)
{
	PyObject* t = NULL;

	if( !PyArg_ParseTuple(args, "O!", &PyLong_Type, &t) )
	{
		return NULL;
	}

	Be::Time time = PyLong_AsLongLong(t);
	if (time == -1 && PyErr_Occurred()) return 0;

	CcpDateTime st;

	if( !TimeAsDateTime( st, time ) )
	{
		SetError(BE32, Clsid(), "Couldn't convert time.");
		return nullptr;
	}

	PyObject* list = PyList_New(8);

	if( !list )
	{
		return nullptr;
	}

	PyList_SET_ITEM(list, 0, PyLong_FromLong(st.year));
	PyList_SET_ITEM(list, 1, PyLong_FromLong(st.month));
	PyList_SET_ITEM(list, 2, PyLong_FromLong(st.dayOfWeek));
	PyList_SET_ITEM(list, 3, PyLong_FromLong(st.day));
	PyList_SET_ITEM(list, 4, PyLong_FromLong(st.hour));
	PyList_SET_ITEM(list, 5, PyLong_FromLong(st.minute));
	PyList_SET_ITEM(list, 6, PyLong_FromLong(st.second));
	PyList_SET_ITEM(list, 7, PyLong_FromLong(st.milliseconds));

	return list;
}


PyObject* BlueOS::PyGetTimeFromParts( PyObject* args )
{
	int i[8];

	if( !PyArg_ParseTuple(
		args,
		"iiiiiii", &i[0], &i[1], &i[2], &i[3], &i[4], &i[5], &i[6]
	))
	{
		return NULL;
	}

	CcpDateTime dt = { 0 };
	dt.year = uint16_t( i[0] );
	dt.month = uint16_t( i[1] );
	dt.day = uint16_t( i[2] );
	dt.hour = uint16_t( i[3] );
	dt.minute = uint16_t( i[4] );
	dt.second = uint16_t( i[5] );
	dt.milliseconds = uint16_t( i[6] );
	Be::Time ft;
    if( !TimeFromDateTime( ft, dt ) )
    {
        SetError(BE32, Clsid(), "Couldn't convert time.");
        return nullptr;
    }

    return PyLong_FromLongLong( ft );
}


PyObject* BlueOS::PyGetCpuTime( PyObject* args )
{
	PyErr_SetString(PyExc_RuntimeError, "GetCpuTime no longer supported");
	return NULL;
}

PyObject* BlueOS::PyEnableSimDilation( PyObject* args )
{
	long long simClockOffset = 0;
	if ( !PyArg_ParseTuple(args, "|L", &simClockOffset) )
	{
		return NULL;
	}

	mSimClockLockedToRealClock = false;
	if( mDynamicSimDilationEnabled == false )
	{ //  Base the sim time some 100 years in the future if we haven't done this before.
		//  REMOVE BEFORE RELEASE
		mSimTime += simClockOffset;
		mDilationSyncBaseSimTime += simClockOffset;
	}
	mDynamicSimDilationEnabled = true;

	Py_INCREF(Py_None);
	return Py_None;
}

PyObject* BlueOS::PyRegisterClientIDForSimTimeUpdates( PyObject* args )
{
#if CCP_STACKLESS

	PyObject* clientIDObj;
	if( !PyArg_ParseTuple(args, "O!", &PyLong_Type, &clientIDObj) )
	{
		return NULL;
	}
	unsigned long long clientID = PyLong_AsLongLong(clientIDObj);

	std::map<unsigned long long, int>::iterator existingEntry;
	existingEntry = mSimDilationSyncClients.find(clientID);
	if( existingEntry != mSimDilationSyncClients.end() )
	{
		// They're already registered with us, up the refcount and bail
		(*existingEntry).second += 1;
		Py_INCREF(Py_None);
		return Py_None;
	}

	// This is a client that we didn't have registered before, create the initial
	// entry in our map and shoot them the init packet
	mSimDilationSyncClients[clientID] = 1;

	// Send this dude the initial update
	char data[128];
	BitPacker packer( data, 128 );
	packer.Pack( mSimTime );
	packer.Pack( mSimDilation );
	packer.Pack( mRealTime );

	int len = packer.Finalize();
	BeNet->SendPacketToClient(clientID, BNT_SIMCLOCK_SYNC_INIT, data, len);
#endif

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* BlueOS::PyUnregisterClientIDForSimTimeUpdates( PyObject* args )
{
#if CCP_STACKLESS

	PyObject* clientIDObj;
	if( !PyArg_ParseTuple(args, "O!", &PyLong_Type, &clientIDObj) )
	{
		return NULL;
	}
	unsigned long long clientID = PyLong_AsLongLong(clientIDObj);

	std::map<unsigned long long, int>::iterator existingEntry;
	existingEntry = mSimDilationSyncClients.find(clientID);
	if( existingEntry == mSimDilationSyncClients.end() )
	{
		CCP_LOGERR_CH( s_chOS, "UnregisterClientID - ID not found: %I64u", clientID);
		return NULL;
	}

	(*existingEntry).second -= 1;
	if( (*existingEntry).second == 0)
	{
		// That's our last reference, erase and detatch them.
		mSimDilationSyncClients.erase(clientID);
		// It doesn't look like BlueNet handles sending an empty payload, so send a 0.
		char data = 0;
		BeNet->SendPacketToClient(clientID, BNT_SIMCLOCK_SYNC_DETACH, &data, 1);
	}

#endif

	Py_INCREF(Py_None);
	return Py_None;
}
#endif

void BlueOS::SendDilationEvent( BlueOS::PendingDilationEvent newEvent )
{
#if CCP_STACKLESS

	// Turn our set of clients into an array of unsigned long longs
	const int numClients = int(mSimDilationSyncClients.size());
	if( numClients == 0 )
	{
		// Noone to tell, bail early.
		return;
	}

	unsigned long long* clientList = new unsigned long long[numClients];

	std::map<unsigned long long, int>::iterator clientIter;
	int i;
	for( clientIter = mSimDilationSyncClients.begin(), i = 0; clientIter != mSimDilationSyncClients.end(); ++clientIter, i++ )
	{
		clientList[i] = (*clientIter).first;
	}

	// Make a nice little buffer of data
	char data[128];
	BitPacker packer( data, 128 );
	packer.Pack( newEvent.mNextDilationEventSimTime );
	packer.Pack( newEvent.mNextDilationFactor );
	packer.Pack( newEvent.mNextDilationEventWallclockTime );

	CCP_LOG_CH( s_chOS, "TIDI Event send - %f", newEvent.mNextDilationFactor);

	int len = packer.Finalize();
	// Shoot it home~
	BeNet->SendPacketToClientList( clientList, numClients, BNT_SIMCLOCK_SYNC_UPDATE, data, len );
	delete[] clientList;

#endif
}

void BlueOS::RegisterForSimTimeRebase( ISimTimeRebaseNotify* cb )
{

	std::vector<ISimTimeRebaseNotify*>::iterator it = std::find( m_simRebaseCallbacks.begin(), m_simRebaseCallbacks.end(), cb );
	if( it == m_simRebaseCallbacks.end() )
	{
		m_simRebaseCallbacks.push_back(cb);
	}
}

void BlueOS::UnregisterForSimTimeRebase( ISimTimeRebaseNotify* cb )
{
	std::vector<ISimTimeRebaseNotify*>::iterator it = std::find( m_simRebaseCallbacks.begin(), m_simRebaseCallbacks.end(), cb );
	if( it != m_simRebaseCallbacks.end() )
	{
		m_simRebaseCallbacks.erase( it );
	}
}

void BlueOS::SimClockPacketCallBack( const unsigned long long fromID, const int blueNetType, const char* data, const int len )
{
#if CCP_STACKLESS

	// We recieved a TiDi packet, time to care about the sim clock
	mSimClockLockedToRealClock = false;

	Be::Time nextEventSim, nextEventWallclock;
	double nextDilationFactor;

	if( blueNetType != BNT_SIMCLOCK_SYNC_DETACH )
	{   // Detach has no data.
		BitPacker unpacker(data, len);
		unpacker.Unpack( nextEventSim );
		unpacker.Unpack( nextDilationFactor );
		unpacker.Unpack( nextEventWallclock );
	}

	if( blueNetType == BNT_SIMCLOCK_SYNC_INIT )
	{
		CCP_LOG_CH( s_chOS, "TIDI Init recv from %" PRIu64, fromID);
		// First up, inform folks that the sim clock is rebasing
		PyObject *args = PyTuple_New( 2 );
		PyTuple_SetItem( args, 0, PyLong_FromUnsignedLongLong( mSimTime ) );
		PyTuple_SetItem( args, 1, PyLong_FromUnsignedLongLong( nextEventSim ) );

		PyOS->PythonEvent( "DoSimClockRebase", args );
		Py_DECREF( args );

		for( auto it = m_simRebaseCallbacks.begin(); it != m_simRebaseCallbacks.end(); it++ )
		{
			(*it)->OnSimClockRebase( mSimTime, nextEventSim );
		}

		PyOS->RebaseSimClock( mSimTime, nextEventSim );

		// Init means we have a new master.  Bash the sim clock into compliance.
		mDilationSyncMaster = fromID;
		mSimTime = nextEventSim;
		mSimDilation = nextDilationFactor;

		mDilationSyncBaseWallclockTime = nextEventWallclock;
		mDilationSyncBaseSimTime = nextEventSim;
		mDilationSyncFactor = nextDilationFactor;
	}
	else if( blueNetType == BNT_SIMCLOCK_SYNC_UPDATE )
	{
		// Update packet.  First check to make sure this is coming from our current master
		if( fromID != mDilationSyncMaster )
		{
			CCP_LOGWARN_CH( s_chOS, "TIDI Event tossed because of master - %" PRId64 "%" PRId64, fromID, mDilationSyncMaster);
			return;
		}

		// If it's older than our current sync, it's stale and needs to go away
		if( nextEventWallclock < mDilationSyncBaseWallclockTime )
		{
			CCP_LOGWARN_CH( s_chOS, "TIDI Event tossed because of time - %" PRId64 " %" PRId64, nextEventWallclock, mDilationSyncBaseWallclockTime);
			return;
		}

		// Store the update so the next clock loop can take it into account
		BlueOS::PendingDilationEvent newEvent;
		newEvent.mNextDilationEventSimTime = nextEventSim;
		newEvent.mNextDilationEventWallclockTime = nextEventWallclock;
		newEvent.mNextDilationFactor = nextDilationFactor;
		mPendingDilationEvents.push(newEvent);
		CCP_LOG_CH( s_chOS, "TIDI Event recv - %f %" PRId64, newEvent.mNextDilationFactor, newEvent.mNextDilationEventWallclockTime);
	}
	else
	{
		// Detach - if it's from our master, then we have no master.
		if( fromID != mDilationSyncMaster )
		{
			CCP_LOGWARN_CH( s_chOS, "TIDI Detach tossed because of master - %" PRIu64 " %" PRId64, fromID, mDilationSyncMaster);
			return;
		}
		CCP_LOG_CH( s_chOS, "TIDI Detach recv from %" PRIu64, fromID);

		mDilationSyncMaster = 0;
		while( !mPendingDilationEvents.empty() )
		{
			mPendingDilationEvents.pop();
		}

		// Reset the advancement of our sim clock to full speed
		mSimDilation = mDilationSyncFactor = 1;
	}

#endif
}

void SimClockPacketCallBackHelper( const unsigned long long fromID, const int blueNetType, const char* data, const int len )
{
	( static_cast<BlueOS*>( BeOS ) )->SimClockPacketCallBack( fromID, blueNetType, data, len );
}


void BlueOS::EvaluateTimeDilation()
{
#if CCP_STACKLESS
	// Needed a post-construction place to set up the packet handler.  This'll do.
	static bool packetsHandled = false;
	if( !packetsHandled )
	{
		BeNet->RegisterCallbackSync( SimClockPacketCallBackHelper, BNT_SIMCLOCK_SYNC_INIT );
		BeNet->RegisterCallbackSync( SimClockPacketCallBackHelper, BNT_SIMCLOCK_SYNC_UPDATE );
		BeNet->RegisterCallbackSync( SimClockPacketCallBackHelper, BNT_SIMCLOCK_SYNC_DETACH );
		packetsHandled = true;
	}
	// If I'm not in control of my own sim clock, there's nothing left to do.
	if( !mDynamicSimDilationEnabled )
	{
		return;
	}

	// Track the last time we were overloaded and the last time we were underloaded
	SchedulerStats& stats = PyOS->GetSchedulerStats( );

	if( stats.numberOfTaskletsInQueuePostTick > 0 )
	{
		// We had tasklets left ready to run at the end of the tick.  We're overloaded.
		mLastOverloadedTime = mRealTime;
	}
	else
	{
		// We completed our queue, we're underloaded (albiet possibly very slightly)
		mLastUnderloadedTime = mRealTime;
	}

	// If we aren't currently waiting for an event to complete, see if we should make one.
	if( mPendingDilationEvents.empty() )
	{
		// We don't have an event pending, decide if I should.
		bool eventCreated = false;
		double desiredDilation = 1;

		// Enforce the bounds
		if( mSimDilation < mMinSimDilation )
		{
			desiredDilation = mMinSimDilation;
			eventCreated = true;
		}
		else if( mSimDilation > mMaxSimDilation )
		{
			desiredDilation = mMaxSimDilation;
			eventCreated = true;
		}
		else if( mLastOverloadedTime > mLastUnderloadedTime )
		{
			// We've been overloaded for a while.  If it's long enough, adjust on down.

			// No sense adjusting down if we're already at the bottom.
			if( mSimDilation > mMinSimDilation)
			{
				Be::Time overloadDuration = mRealTime - mLastUnderloadedTime;
				if( overloadDuration > mDilationOverloadThreshold )
				{
					desiredDilation = std::max(mMinSimDilation, mSimDilation * mDilationOverloadAdjustment);
					eventCreated = true;
				}
			}
		}
		else
		{
			// And the same for under-load
			if( mSimDilation < mMaxSimDilation)
			{
				Be::Time underloadDuration = mRealTime - mLastOverloadedTime;
				if( underloadDuration > mDilationUnderloadThreshold )
				{
					desiredDilation = std::min(mMaxSimDilation, mSimDilation * mDilationUnderloadAdjustment);
					eventCreated = true;
				}
			}
		}

		if( eventCreated )
		{
			BlueOS::PendingDilationEvent newEvent;
			newEvent.mNextDilationFactor = desiredDilation;
			newEvent.mNextDilationEventWallclockTime = mRealTime + 2 * ONE_SECOND;
			newEvent.mNextDilationEventSimTime = Be::Time(mSimTime + (2 * ONE_SECOND_F) * mSimDilation);
			mPendingDilationEvents.push(newEvent);
			SendDilationEvent(newEvent);

			// Reset our times so we get a full evaluation period with the new dilation factor
			mLastOverloadedTime = mLastUnderloadedTime = mRealTime;
		}
	}

#endif
}


#if BLUE_WITH_PYTHON

PyObject* BlueOS::PySetAppTitle(PyObject* args)
{
	const char* title;

	if( !PyArg_ParseTuple(args, "s", &title) )
	{
		return NULL;
	}
#ifdef _WIN32
	SetConsoleTitle(title);
#else
    printf( "\033]2;%s\a", title );
#endif

    Py_INCREF(Py_None);
	return Py_None;
}



PyObject* BlueOS::PyShellExecute(PyObject* args)
{
	PyObject *fn, *params=0;
	if( !PyArg_ParseTuple(args, "O!|O!", &PyUnicode_Type, &fn, &PyUnicode_Type, &params) )
	{
		return NULL;
	}
	fn = PyUnicode_FromObject(fn);
	if( !fn )
	{
		return 0;
	}
	if( params )
	{
		params = PyUnicode_FromObject(params);
		if( !params )
		{
			Py_DECREF(fn);
			return 0;
		}
	}
	auto param = PyUnicode_AsWideCharString( fn, NULL );
	std::wstring file( param );
	PyMem_Free( param );
	Py_DECREF(fn);
	bool http = file.find(L"http://") == 0 || file.find(L"https://") == 0;

	if (!http)
	{
		//not a http request. rewrite the file
		file = BePaths->ResolvePathW( file.c_str() );
	}

#ifdef _WIN32
	//since IE7, urls cannot be opened directly from multithreaded applications.  Instead, we must have
	//rundll do it for us.

	SHELLEXECUTEINFOW ei;
	memset(&ei, 0, sizeof(ei));
	ei.cbSize = sizeof(ei);
	std::wstring tmp;
	wchar_t* wcharParams = nullptr;
	if( !http )
	{
		ei.lpFile = file.c_str();
		if( params )
		{
			wcharParams = PyUnicode_AsWideCharString( params, NULL );
			ei.lpParameters = wcharParams;
		}
	}
	else
	{
		ei.lpVerb = L"open";
		ei.lpFile = L"rundll32.exe";
		tmp = L"url.dll,FileProtocolHandler ";
		tmp += file.c_str();
		ei.lpParameters = tmp.c_str();
	}
	ei.nShow = SW_SHOWNORMAL;
	BOOL OK = ShellExecuteExW(&ei);
	PyMem_Free( wcharParams );

	if( !OK )
	{
		CW2A filename(file.c_str());
		return PyErr_SetFromWindowsErrWithFilename(GetLastError(), (char*)filename);
	}
#else
	std::string file_utf8{WideToUTF8( file )};
    std::string command = "open " + file_utf8;
    system( command.c_str() );
#endif
	Py_INCREF(Py_None);
	return Py_None;
}

PyObject* BlueOS::PyTerminate( PyObject* args )
{
	int retCode = 0;
	if( !PyArg_ParseTuple( args, "|i:Terminate", &retCode ) )
	{
		return NULL;
	}

	Terminate( retCode );
	// will never get here but for style points we still finish it off - in style:
	Py_RETURN_NONE;
}
#endif

void BlueOS::SetStartupArgs( const std::vector<std::wstring>& args )
{
	m_startupArgs = args;

	for( size_t i = 1; i < m_startupArgs.size(); ++i )
	{
		std::wstring argName = m_startupArgs[i];
		std::wstring argValue;

		if( argName[0] == L'/' )
		{
			argName.erase( 0, 1 );
		}

		size_t assignPos = argName.find_first_of( L'=' );
		if( assignPos != std::string::npos )
		{
			argValue = argName.substr( assignPos + 1 );
			argName.erase( assignPos );
		}

		m_startupArgsMap[argName] = argValue;
	}
}

const std::vector<std::wstring>& BlueOS::GetStartupArgs() const
{
	return m_startupArgs;
}

bool BlueOS::HasStartupArg( const std::wstring& arg ) const
{
	if( m_startupArgsMap.find( arg ) != m_startupArgsMap.end() )
	{
		return true;
	}

	return false;
}

std::wstring BlueOS::GetStartupArgValue( const std::wstring& arg ) const
{
	auto it = m_startupArgsMap.find( arg );
	if( it != m_startupArgsMap.end() )
	{
		return it->second;
	}

	return L"";
}

const wchar_t* BlueOS::GetLanguageId()
{
	return mLanguageID.c_str();
}

uint32_t BlueOS::GetFrameTimeTimeout() const
{
	return m_frameTimeTimeout;
}

static BlueTimeoutHandler s_timeoutHandler;

void BlueOS::SetFrameTimeTimeout( uint32_t val )
{
	m_frameTimeTimeout = val;
	s_timeoutHandler.Reset();
	m_frameTimeWatchdog.Start( m_frameTimeTimeout, &s_timeoutHandler );
}

uint32_t BlueOS::GetDesiredFrameTimeMilliseconds() const
{
	return m_desiredFrameTimeMilliseconds;
}

void BlueOS::SetDesiredFrameTimeMilliseconds( uint32_t val )
{
	m_desiredFrameTimeMilliseconds = val;
}

int32_t BlueOS::GetSleepTime() const
{
	PyErr_WarnEx( PyExc_DeprecationWarning, "blue.os.sleeptime no longer serves any purpose" , 1 );
	return mSleepTime;
}

void BlueOS::SetSleepTime( int32_t val )
{
	PyErr_WarnEx( PyExc_DeprecationWarning, "blue.os.sleeptime no longer serves any purpose" , 1 );
	mSleepTime = val;
}


void BlueOS::TickTickers()
{
	// Copy tickers, which can get mutated during ticks
	auto tickers{ mTickers };

	for( const auto& ticker : tickers )
	{
		if( std::find( mTickers.begin(), mTickers.end(), ticker ) == mTickers.end() )
		{
			// ticker was removed during a different tick
			continue;
		}

		const char* taskname = ticker.mCookie;
		// Switch to appropriate ticker
#if BLUE_WITH_PYTHON
		AutoTasklet _at2( PyOS->GetTaskletTimer(), taskname ? taskname : "(null ticker?)" );
#endif
		ticker.mCb->OnTick( mRealTime, mSimTime, (void*)taskname );

#if BLUE_WITH_PYTHON
		PyOS->PyFlushError( taskname );
#endif

		if( mDebugLevel > 0 && HeapScrewed() )
		{
			CCP_LOGERR_CH( s_chOS, "Heap corrupt after ticking %s", taskname );
		}
	}
}

void BlueOS::CaptureLogCountsToStats()
{
	CCP_STATS_ADD( logInfo, CCP::GetLogCounter( CCP::LOGTYPE_INFO ) );
	CCP_STATS_ADD( logNotice, CCP::GetLogCounter( CCP::LOGTYPE_NOTICE ) );
	CCP_STATS_ADD( logWarn, CCP::GetLogCounter( CCP::LOGTYPE_WARN ) );
	CCP_STATS_ADD( logErr, CCP::GetLogCounter( CCP::LOGTYPE_ERR ) );

	CCP::GetLogCounter( CCP::LOGTYPE_INFO ) = 0;
	CCP::GetLogCounter( CCP::LOGTYPE_NOTICE ) = 0;
	CCP::GetLogCounter( CCP::LOGTYPE_WARN ) = 0;
	CCP::GetLogCounter( CCP::LOGTYPE_ERR ) = 0;
}

void BlueOS::ShowErrorMessageBox( const wchar_t* title, const wchar_t* message )
{
#ifdef _WIN32
	MessageBoxW( nullptr, message, title, MB_ICONSTOP );
#elif defined(__APPLE__)
    CFStringEncoding encoding = (CFByteOrderLittleEndian == CFByteOrderGetCurrent()) ? kCFStringEncodingUTF32LE : kCFStringEncodingUTF32BE;

    CFStringRef titleRef = CFStringCreateWithBytes( nullptr, reinterpret_cast<const UInt8*>( title ), wcslen( title ) * sizeof( wchar_t ), encoding, false );
    CFStringRef messageRef = CFStringCreateWithBytes( nullptr, reinterpret_cast<const UInt8*>( message ), wcslen( message ) * sizeof( wchar_t ), encoding, false );

    CFOptionFlags result;  //result code from the message box

    CFUserNotificationDisplayAlert( 0, kCFUserNotificationStopAlertLevel, nullptr, nullptr, nullptr, titleRef, messageRef, nullptr, nullptr, nullptr, &result );

	CFRelease( titleRef );
	CFRelease( messageRef );
#endif
}

void BlueOS::SetMarkupZonesInPython(bool markupZonesInPython)
{
	PyOS->SetMarkupZonesInPython( markupZonesInPython );
}

void BlueOS::ShowMessageBoxForVerificationFailure( const std::string& errmsg )
{
	std::string msg = TranslateErrorMessage( "Your EVE client installation may have modified, damaged or corrupt files.", IDS_VERIFYFAIL_M );
	std::string caption = TranslateErrorMessage( "Verification Failure", IDS_VERIFYFAIL_C );

	msg += "\n\n" + errmsg;
	DisplayErrorMessageBox( caption.c_str(), msg.c_str() );
}

bool BlueOS::VerifyManifestAndGatherDirectives( directives_t& directives )
{
	//We always read our manifest.  We have a null manifest that we try first for kicks.
	if( !BeIsSuccess( VerifyManifestFile( "root:/manifest.dat", directives ) ) )
	{
		// Try again with a different path.
		Be::Result<std::string> result = VerifyManifestFile( "bin:/manifest.dat", directives );
		if( !BeIsSuccess( result ) )
		{
			BeOS->SetError( BEFLUSH );
			ShowMessageBoxForVerificationFailure( result.value );
			return false;
		}
	}

	return true;
}

void BlueOS::ProcessLibDirectives( const directives_t& directives, std::vector<std::wstring>& zips )
{

	mPackaged = false;

	//process lib directives from the file
	for( const std::string& directive : directives )
	{
		if( directive.find( "lib:" ) == 0 )
		{
			mPackaged = true;
			CCP_LOG_CH( s_chOS, "Directive %s", directive.c_str() );
			zips.push_back( BePaths->ResolvePathW( std::wstring( std::begin( directive ) + 4, std::end( directive ) ) ) );
		}
	}
}

bool BlueOS::ConstructPathListFromManifest(std::vector<std::wstring>& pathlist,  bool verifyManifest)
{
	//build pathlist
	if( verifyManifest )
	{
		directives_t directives;

		if( !VerifyManifestAndGatherDirectives( directives ) )
		{
			return false;
		}

		ProcessLibDirectives( directives, pathlist );
	}

	//add other paths
	if( !pathlist.size() )
	{
		BePaths->GetExpandedSearchPaths( "lib", pathlist );
	}

	// BIN path is required.
	BePaths->GetExpandedSearchPaths( "bin", pathlist );

	return true;
}

extern "C" PyObject* CCP_CONCATENATE( PyInit_blue, CCP_BUILD_FLAVOR )( void );

void BlueOS::GetInitTab( std::vector<_inittab>& tabs ) const
{
	tabs.push_back( { g_moduleName, CCP_CONCATENATE( PyInit_blue, CCP_BUILD_FLAVOR ) } );
	tabs.push_back( { nullptr, nullptr } );
}


#ifdef OptimizeOff
#pragma optimize("", on)
#endif
