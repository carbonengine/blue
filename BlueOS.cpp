#include "StdAfx.h"

#include "BlueOS.h"
#include "include/IBluePersist.h"
#include "include/IBlueCallbackMan.h"
#include "include/TransGaming.h"
#include "BlueMemoryTracker.h"
#include "BlueResMan.h"
#include "BlueMemStream.h"
#include "BlueObjectRecycler.h"
#include "MotherLode.h"
#include "BlueLogInMemory.h"
#include "BluePaths.h"
#include "Stuffer.h"
#include "BlueResFile.h"

#if BLUE_WITH_PYTHON
#include "include/ITaskletTimer.h"
#include "BluePython.h"
#endif

#ifdef _WIN32
#include "crypto.h"
#include "Logger/Logger.h"
#endif

#if CCP_STACKLESS
#include "include/BitPacker.h"
#include "include/BlueNet.h"
#include "include/BlueNetTypes.h"
#include "IOLoop.h"
#include <stacklessio_api.h>
#include "CcpUtils/PyCpp.h"
#endif

#include <sstream> // for message creation
#include <algorithm>
#include "BlueTimeoutHandler.h"

static CcpLogChannel_t s_chOS = CCP_LOG_DEFINE_CHANNEL( "OS" );
static CcpLogChannel_t s_chErr = CCP_LOG_DEFINE_CHANNEL( "ERR" );

IBlueOS* BeOS = nullptr;
BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "os", BeOS );

CCP_STATS_DECLARE( logInfo,			"Blue/logInfo",			false,	CST_COUNTER_LOW, "Count of info logs" );
CCP_STATS_DECLARE( logNotice,		"Blue/logNotice",		false,	CST_COUNTER_LOW, "Count of notice logs" );
CCP_STATS_DECLARE( logWarn,			"Blue/logWarn",			false,	CST_COUNTER_LOW, "Count of warning logs" );
CCP_STATS_DECLARE( logErr,			"Blue/logErr",			false,	CST_COUNTER_LOW, "Count of error logs" );

// the new sleep mode
bool g_useNewSleepMode = false;

// CarbonIO wake up mode
bool g_carbonIoFastWakeup = false;

bool g_carbonIoManualWakeup = true;

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

// A class to format and create a windows error message.
class BlueWinError
{
public:
	BlueWinError();	//gets code from GetLastError()
	BlueWinError(unsigned long errorcode);	//(DWORD)you supply the code
	~BlueWinError();

	operator const char* () const {return message?message:"Unknown";}
private:
	void Format(unsigned long code);
	char *message;
};

//BlueWinError class members
BlueWinError::BlueWinError()
{
	Format(GetLastError());
}
BlueWinError::BlueWinError(DWORD error)
{
	Format(error);
}
BlueWinError::~BlueWinError()
{
	if (message)
		LocalFree(message);
}
void BlueWinError::Format(DWORD errcode)
{
	message = 0;
	DWORD res = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		0,
		errcode,
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


#endif // _WIN32

#if CCP_STACKLESS

//------------------------------------------------------------------------------
static void OnIOTaskletScheduled( bool force )
{
	if( g_carbonIoFastWakeup )
	{
		SetEvent(gBreakCarbonIo);
	}
	else
	{
		unsigned int current = GetTickCount();
		if( (current > sNextScheduledIOWakeup) || force )
		{
			sNextScheduledIOWakeup = current + BeNet->GetMinScheduledIOInterval();
			sPendingWakeup = false;
			SetEvent( gBreakSleep ); // tends to cause an immediate context-switch, do it last
		}
		else
		{
			sPendingWakeup = true;
		}
	}
}

//------------------------------------------------------------------------------
static void IOWakeupWatchdogThread( void *arg )
{
	// periodically wake up and check to see if a pending wakeup was
	// issued but never serviced, this will happen only when IO is unbusy.
	for(;;)
	{
		if ( sPendingWakeup )
		{
			unsigned int current = GetTickCount();
			if ( current > sNextScheduledIOWakeup )
			{
				SetEvent( gBreakSleep );
				sPendingWakeup = false;
			}
		}

		Sleep( BeNet->GetWatchdogInterval() );
	}
}


//------------------------------------------------------------------------------
int runPyMain( std::vector<std::wstring> &argv )
{/*
	This is so that we can have blue behave as a standard python interpreter.
 */
	// We want vanilla python behaviour.
	// Every argument after /py gets forwarded to Py_Main	
	unsigned int nPythonArgs;
	unsigned int vanillaIndex = 0;
	char** pythonArguments;
	// PyMain actually fucks with the arguments
	// so we need to backup the pointer to clean them up
	char** backupPythonArguments; 
		
	// At what index is the vanilla marker
	for( unsigned int i = 0; i < argv.size(); i++ )
	{
		const std::wstring &arg = argv[i];
		if( arg == L"/py")
		{
			vanillaIndex = i;
			break;
		}
	}

	// Create the parameter list for the main python function
	nPythonArgs = (unsigned int)argv.size() - vanillaIndex;

	// Convert all the unicode strings to ascii
	// We are going to be very careful about allocating memory and reporting
	// anything that goes wrong.
	pythonArguments = (char**)CCP_MALLOC("RunPyMain: Argument Array",  nPythonArgs*sizeof(char*) );
	if( pythonArguments == NULL )
	{
		CCP_LOGERR( "runPyMain() -> Couldn't allocate memory for the python arguments" );
		return 1;
	}

	backupPythonArguments = (char**)CCP_MALLOC("RunPyMain: Argument Array",  nPythonArgs*sizeof(char*) );
	if( backupPythonArguments == NULL )
	{
		CCP_LOGERR( "runPyMain() -> Couldn't allocate memory for the backup python arguments" );
		return 1;
	}

	unsigned int index = 0;
	unsigned int counter = 0;
	do 
	{
		unsigned int slen = (unsigned int)argv[index].size() + 1; // Plus one for the null terminator
		pythonArguments[counter] = (char*)CCP_MALLOC( "RunPyMain: Array Element",  slen*sizeof(char) );
		if( pythonArguments[counter] == NULL )
		{
			CCP_LOGERR( "runPyMain() -> Couldn't allocate memory for one of the python parameters" );			
			return 1;
		}
		strncpy_s( pythonArguments[counter], slen, CW2A(argv[index].c_str()), _TRUNCATE );
		++counter;
		index = vanillaIndex + counter;
	} while ( index < argv.size() );

	// backup the pointers
	for( unsigned int i = 0; i < nPythonArgs; i++ )
	{
		backupPythonArguments[i] = pythonArguments[i];
	}

	// Success,... now lets hope the arguments make sense
	int result = Py_Main( nPythonArgs, pythonArguments );

	// Cleanup	
	for( unsigned int i = 0; i < nPythonArgs; i++ )
	{
		CCP_FREE( backupPythonArguments[i] );
	}
	CCP_FREE( pythonArguments );
	CCP_FREE( backupPythonArguments );
	return result;
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
	mCatchUpTickerHeap( "BlueOS/mCatchUpTickerHeap" ),
	m_frameTimeWatchdog( "FrameTime" ),
	m_frameTimeTimeout( 0 ),
	mAdvanceTimeInPump( true ),
	mTimeStampIdx( -1 )
{
	mPID = CcpGetCurrentProcessId();

	BeOS = this;  //Set global IBlueOS pointer to this static object

	mLastKeyValue = NULL;
	strcpy_s(mLanguageID, sizeof(mLanguageID), "EN");
	m_cachedLanguageId = CA2W( mLanguageID );

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
	mNextScheduledEvent = 0;
	mInsidePump = false;

	mResolveRotRefs = true;
	mMiniDump = false;

#if CCP_STACKLESS
	gBreakSleep = CreateEvent(NULL, FALSE, FALSE, NULL);
	gBreakCarbonIo = CreateEvent(NULL, FALSE, FALSE, NULL);

	CioSetOnTaskletScheduledCallback( OnIOTaskletScheduled );
	_beginthread( IOWakeupWatchdogThread, 0, 0 );

#endif

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
	CCP_FREE(mLastKeyValue);

	CloseHandle(gBreakSleep);
}


BlueOS::Ticker::Ticker( IBlueEvents *cb, void* cookie ) :
	mCb( cb ),
	mCookie( (const char*)cookie )
{
}


BlueOS::CatchupTicker::CatchupTicker(
	ICatchupTicks *cb, 
	void* cookie, 
	Be::Time tickTime, 
	Be::Time perFrameBudget
) :
	mCb(cb),
	mCookie((const char*)cookie),
	mTimeBetweenTicks(tickTime),
	mPerFrameTimeBudget(perFrameBudget),
	mNeedsPostFrameTick(false)
{
}


bool BlueOS::CatchupTicker::operator<(const CatchupTicker& other) const
{
	if( mCb == other.mCb )
	{
		return mCookie < other.mCookie;
	}
	else
	{
		return mCb < other.mCb;
	}
}


bool BlueOS::CatchupTicker::operator == (const CatchupTicker& other) const
{
	return mCb == other.mCb && mCookie == other.mCookie;
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


void BlueOS::RegisterForCatchupTicks(
	ICatchupTicks *cb, 
	void* cookie, 
	Be::Time tickMS, 
	Be::Time perFrameBudget )
{
	// Check if its already in here? If so then return
	for( unsigned int i = 0; i < mCatchUpTickerHeap.size(); i++ )
	{
		if( mCatchUpTickerHeap[i]->mCb == cb && mCatchUpTickerHeap[i]->mCookie == cookie)
		{
			return;
		}
	}

	CatchupTicker *ticker = CCP_NEW("BlueOS::RegisterForCatchupTicks::CatchupTicker")
		CatchupTicker(cb, cookie, tickMS*10000, perFrameBudget);

	ticker->mNextTickTime = mRealTime + ticker->mTimeBetweenTicks;
	mCatchUpTickerHeap.push_back(ticker);
	push_heap( mCatchUpTickerHeap.begin(), mCatchUpTickerHeap.end(), CatchUpTickerComparator() );
}


void BlueOS::UnregisterForCatchupTicks( ICatchupTicks *cb, void* cookie )
{
	for( unsigned int i = 0; i < mCatchUpTickerHeap.size(); i++ )
	{
		if( mCatchUpTickerHeap[i]->mCb == cb && mCatchUpTickerHeap[i]->mCookie == cookie )
		{
			// Swap the back to this position, pop back, and reheapify
			CCP_DELETE mCatchUpTickerHeap[i];
			mCatchUpTickerHeap[i] = mCatchUpTickerHeap[mCatchUpTickerHeap.size()-1];
			mCatchUpTickerHeap.pop_back();
			make_heap(mCatchUpTickerHeap.begin(), mCatchUpTickerHeap.end(), CatchUpTickerComparator());
			return;
		}
	}
}

void Py_FatalError( char *msg )
{
	CCP_LOGERR_CH( s_chOS, "Py_FatalError: %s", msg );
	fprintf( stderr, "Fatal Python error: %s\n", msg );
#ifdef _WIN32
	RaiseException( 0xE0000011, EXCEPTION_NONCONTINUABLE, 0, NULL );
	TerminateProcess( GetCurrentProcess(), -3 ); // shouldn't get here
#else
	exit(1);
#endif
}

//--------------------------------------------------------------------
// This function handles the intricacies of sleeping for a particular
// time. In particular, it will resume an interrupted sleep if it finds
// that there is, in fact, no IO to be done.
void BlueOS::DoSleep()
{
#if CCP_STACKLESS

	if( mNextScheduledEvent <= 0 )
	{
		// Quick return if there is no desire to sleep
		ResetEvent( gBreakSleep );
		return;
	}

	if ( g_useNewSleepMode )
	{
		SafeAutoTasklet _at2(PyOS->GetTaskletTimer(),TASKLETS[IDLETASKLET].mContext, true); 
		SleepWithIo sleeper;
		bool retry = false;
		do {
			//orgsleeptime may change between calls.  Nifty.
			sleeper.Sleep(mNextScheduledEvent, retry);
			if ( retry )
			{
				m_ioRunsTotal++;
			}
		} while ( retry );
	}
	else
	{
		// Mark this as an idle timer
		SafeAutoTasklet _at2(PyOS->GetTaskletTimer(),TASKLETS[IDLETASKLET].mContext, true, IDLE); 

		HANDLE handles[3];
		handles[0] = PyStacklessIoGetWakeupEventHandle();
		handles[1] = gBreakSleep;
		handles[2] = gBreakCarbonIo;
		__int64 startTime, frequency=0;

		int orgsleeptime = mNextScheduledEvent;
		int sleeptime = orgsleeptime;
		QueryPerformanceCounter((LARGE_INTEGER*)&startTime);
		while( sleeptime > 0 )
		{
			DWORD result;
			{
				Ccp::PyAllowThreads _allow; //allow python threads during the sleep
				result = MsgWaitForMultipleObjectsEx(
					_countof(handles), handles,
					sleeptime, QS_ALLEVENTS, MWMO_ALERTABLE);
			}

			// We appear to have been woken by stacklessIO.  But this may be an old state, and StackelssIO
			// may have been serviced in the mean time.  therefore, we must check if servicing it is still
			// necessary.
			if( result == WAIT_OBJECT_0 )
			{
				PyStacklessIoStatus_t s;
				s.struct_size = sizeof(s);
				PyStacklessIoGetStatus(&s);
				if( s.nNonRunnable || s.nRunnable )
				{
					break; //There is stuff to be done
				}
				else
				{
					//[yawn,] no need to panic.
					__int64 nowTime;
					QueryPerformanceCounter((LARGE_INTEGER*)&nowTime);
					if( !frequency )
					{
						QueryPerformanceFrequency((LARGE_INTEGER*)&frequency);
					}
					double time_spent = (double)(nowTime-startTime)/(double)frequency;
					sleeptime = orgsleeptime - (int)(time_spent*1000.0); //milliseconds
				}
			}
			else if (result == WAIT_OBJECT_0 + 2)
			{
				//CarbonIo signal
				// Depending on this flag, now have carbonIO wake up its tasklets.  Perhaps it
				// didn't already do so (because it merely scheduled a pending call) so this forces
				// the issue.
				// This is the right thing to do, but it is switchable for testing purposes.
				if( g_carbonIoManualWakeup )
				{
					CioWakeupTasklets();
				}
				if( PyStackless_GetRunCount() > 1 )
				{
					break; //stuff to be done
				}
			}
			else
			{
				break; //interrupted or timed out
			}
		}
	}
	ResetEvent( gBreakSleep );

#endif
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
// patrick.kerr@ccpgames.com, 2011/03/15

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


// UGLY: Using a global to support convenient access to the variable delta, without having to change
//       lots of function signatures. Eventually, it should be possible to remove this, but it makes
//       life easier to retain it for now.
#ifdef _WIN32
__declspec(dllexport) 
#endif
float g_deltaT_sec = 0.0f;


void BlueOS::InitVariableTicking()
{
	// If we aren't using a measured delta, use this nominal value instead
	mNominalDeltaTSec = 0.033f; // i.e. GAMEWORLD_S_PER_TICK;

	const bool START_WITH_CONSTANT_TICKS = false; //true;

	if( START_WITH_CONSTANT_TICKS )
	{
		// The original behaviour is equivalent to this...
		mUseSimpleCatchupLoop = false;
		mUseNominalDeltaT = true;
	}
	else
	{
		// Full variable-tick support is enabled by this...
		mUseSimpleCatchupLoop = true;
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

void BlueOS::UpdateSlugTime()
{
	if( mIsSlugTimeIncreasing )
	{
		mSlugTimeCurrentMs += mSlugTimeDeltaMs;
		if( mSlugTimeCurrentMs > mSlugTimeMaxMs )
		{
			mSlugTimeCurrentMs = mSlugTimeMaxMs;
			mIsSlugTimeIncreasing = false;
		}
	}
	else
	{
		mSlugTimeCurrentMs -= mSlugTimeDeltaMs;
		if( mSlugTimeCurrentMs < mSlugTimeMinMs )
		{
			mSlugTimeCurrentMs = mSlugTimeMinMs;
			mIsSlugTimeIncreasing = true;
		}
	}
}

void BlueOS::DoSlug()
{
	UpdateSlugTime();
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
    if( abs(mTimeSyncAdjust) > ONE_MILLISECOND )
    {
        Be::Time maxMagnitude = abs( Be::Time(smoothedDeltaT * mTimeSyncAdjustFactor) );

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

				CCP_LOG_CH( s_chOS, "TIDI New sync base - %f, %I64d", newEvent.mNextDilationFactor, mRealTime);
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

	// Update the global version of deltaT (which is just a temporary convenience for now)
	g_deltaT_sec = deltaT_sec;

	// NB: I'm putting these timer stats into a millisecond scale, and must multiply accordingly
	CCP_STATS_SET( STAT_usedDeltaT, g_deltaT_sec * 1000 );


	// Set the formal outputs of this function
	*ptrActualTime = actualTime;
	*ptrDeltaT_sec = deltaT_sec;
}

void BlueOS::RunSimpleCatchupLoop(float deltaT_sec)
{
	// In future, if we remove CatchupTicks, I would expect to replace them with something like this...
	/*
	for (VariableTickIt ticker = mVariableTickers.begin();
		 ticker != mVariableTickers.end(); ticker++)
	{
		CCP_STATS_ZONE( "BlueOS/PumpOS/VariableTickers" );

		const char* taskname = ticker->mCookie;
		// Switch to appropriate ticker
		AutoTasklet _at2(PyOS->GetTaskletTimer(), taskname ? taskname : "(null ticker?)");		
		ticker->mCb->OnTick(mTime, deltaT_sec, (void*)taskname);

        PyOS->PyFlushError(taskname);

		if (mDebugLevel > 0 && HeapScrewed())
			CCP_LOGERR_CH( s_osChannel, "Heap corrupt after ticking %s", taskname);
	}
	*/

	// While we still have CatchupTickers, use this simple iteration to treat them as VariableTickers
	for( CatchupTickerVector::iterator iter = mCatchUpTickerHeap.begin();
		iter != mCatchUpTickerHeap.end(); ++iter )
	{
		CCP_STATS_ZONE( "BlueOS/PumpOS/VariableTickers" );

		CatchupTicker* ticker = *iter;

		const char* taskname = ticker->mCookie;
		// Switch to appropriate ticker
#if BLUE_WITH_PYTHON
		AutoTasklet _at2(PyOS->GetTaskletTimer(), taskname ? taskname : "(null ticker?)");		
#endif
		ticker->mCb->OnTick(mRealTime, deltaT_sec, (void*)taskname);
		ticker->mCb->OnPostFrameTick(mRealTime, (void *)taskname);

#if BLUE_WITH_PYTHON
		PyOS->PyFlushError(taskname);
#endif

		if( mDebugLevel > 0 && HeapScrewed() )
		{
			CCP_LOGERR_CH( s_chOS, "Heap corrupt after ticking %s", taskname);
		}
	}
}

void BlueOS::PumpOS()
{
	CCP_STATS_ZONE( "BlueOS/PumpOS" );

#ifdef _WIN32
	if( IsDebuggerPresent() )
#endif
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

	// Get value for languageID for ResFile worker threads
	// TODO: remove this clunky hack
	strcpy_s(mLanguageID, sizeof(mLanguageID), KeyVal("languageID", "_cachercalling", false));
	m_cachedLanguageId = CA2W( mLanguageID );

#if CCP_STACKLESS

	SafeAutoTasklet _at(PyOS->GetTaskletTimer(), TASKLETS[BLUETASKLET].mContext);

	// Sleep until we need to wake up
	bool doSleep;
	if (mOverrideFG)
	{
		doSleep = mOverrideFG < 0;
	}
	else
	{
		doSleep = mSleepTime != 0;
	}

	if (doSleep)
	{
		//CCP_STATS_ZONE( "BlueOS/PumpOS/DoSleep" );
		// Defining this directly to be able to mark this as an "idle" telemetry zone
		tmZone( g_telemetryContext, TMZF_IDLE, "BlueOS/PumpOS/DoSleep" );

		DoSleep();
		mNextScheduledEvent = 10000; // 10 secs. to next event
	}

	// alert the IO watchdog that events have been delivered recently
	sNextScheduledIOWakeup = GetTickCount() + MIN_SCHEDULED_IO_INTERVAL;

	BeNet->DeliverCPackets(); // dispatch any waiting callbacks
#endif


	// Variable-DeltaT Support...

	// The "Slug" is a deliberately crude diagnostic feature which introduces a (potentially variable) delay into
	// the mainloop. This helps to test our frame-rate compensation. Added by Patrick Kerr.
	//
	DoSlug();

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

	if( mUseSimpleCatchupLoop )
	{
		RunSimpleCatchupLoop(deltaT_sec);
	}
	else
	{
		RunCatchUpTicks(deltaT_sec);
	}

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

bool BlueOS::Startup( short interfaceVersion, int pyOptimizeFlag )
{
#if _WIN32
#if BLUE_WITH_PYTHON
	//start up crypto
	if( !InitVerificationCtxt() )
	{ 
		SetError(BEDEF, Clsid(), "BlueOS::Startup(): InitVerificationContext failed");
		return false;
	}
#endif

	mOsInfo.dwOSVersionInfoSize = sizeof mOsInfo;
	GetVersionEx(&mOsInfo);
#endif

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
	BeClasses->CreateInstance(GetBluePyOSClsid(), GetIBluePyOSIID(), (void**)&PyOS);
	if( !PyOS )
	{
		goto FAIL;
	}

	PyOS->mOptimizeFlag = pyOptimizeFlag;
	if( !PyOS->Startup() )
	{
		goto FAIL;
	}

	for (int i = 0; i < sizeof TASKLETS / sizeof TASKLETS[0]; i++)
	{
		TASKLETS[i].mContext = PyString_InternFromString(TASKLETS[i].mName);
	}

	if( PyOS->IsPackaged() )
	{
		BeResMan->SetSubstituteBlackForRed( true );
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

	// Now, enter stackless and continue running from there.  This allows stackless to initialize
	// the main tasklet.
	if( !PyOS->IsPackaged() )
	{
		// See if /py is on the command line
		std::vector<std::wstring> argv = GetStartupArgs();

		// Should we run the standard interpreter loop
		for( size_t i = 1; i < argv.size(); ++i )
		{
			const std::wstring &arg = argv[i];
			if( arg == L"/py" )
			{
				if( runPyMain( argv ) )
				{
					return false;
				}
				return true;
			}
		}
	}

	PyObject* me = BlueWrapObjectForPython( this );
	if( me )
	{
		PyObject* ret = PyStackless_CallMethod_Main(me, "StacklessMain", NULL);
		Py_DECREF(me);
		if (!ret)
		{
			PyOS->PyError();
			return false;
		}
		else
		{
			Py_DECREF(ret);
		}
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

	// See if /telemetryServer is on the command line
	std::vector<std::wstring> argv = GetStartupArgs();

	for( size_t i = 1; i < argv.size(); ++i )
	{
		const std::wstring &arg = argv[i];
		if( arg.find( L"/telemetryServer=" ) == 0 )
		{
			std::wstring server = argv[i].substr(17);
			CW2A aServer( server.c_str() );
			const int ARENA_SIZE = 8*1024*1024;
			g_statistics->StartTelemetry( aServer, ARENA_SIZE );
		}
	}

	// StartTelemetry triggers a start on the next Update, so do
	// an update here.
	if( g_statistics )
	{
		g_statistics->Update();
	}

	// Set the main tasklet as block trapped.
	{
		BluePy current(PyStackless_GetCurrent());
		if (!current)
			return NULL;
		PyTasklet_SetBlockTrap((PyTaskletObject*)(PyObject*)current, 1);
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
#if CCP_STACKLESS
	fprintf(stderr, "Terminating process by request - returning %d", retCode );
#endif
	CCP_LOGERR_CH( s_chOS, "Terminating process by request - returning %d", retCode );

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

#ifdef _WIN32
	// This should ensure that the in-memory logs are flushed
	// ensuring that any exceptions right before the terminate are still sent out!
	// This only handles the in memory logging, not the other handlers
	Log__Flush();

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
	Log__Flush();

	// If we ever get here something has gone horribly wrong - induce a crash so
	// we learn about this via the crash dumps.
	int* crashPointer = nullptr;
	*crashPointer = 42;
#elif defined(__ORBIS__)
	// TODO: kill it!
#else
    kill( getpid(), SIGKILL );
#endif
}

void BlueOS::SetTime(Be::Time time)
{
	if( !time )
	{
#ifdef _WIN32
		//Get the time, as precisely as we can (it's updated very rarely)
		FILETIME ft0, ft1;
		GetSystemTimeAsFileTime(&ft0);
		for(;;)
		{
			GetSystemTimeAsFileTime(&ft1);
			if( memcmp(&ft0, &ft1, sizeof(ft0)) )
			{
				break; //it just changed!  okay, we are all set.
			}
		}
		LARGE_INTEGER li;
		li.LowPart = ft1.dwLowDateTime;
		li.HighPart = ft1.dwHighDateTime;
		time = li.QuadPart;
#elif defined(__ORBIS__)
		SceKernelTimespec ts;
		sceKernelClockGettime( SCE_KERNEL_CLOCK_MONOTONIC, &ts );
		time = (uint64_t)ts.tv_sec * 10000000LL + (uint64_t)ts.tv_nsec / 100;
#else
        struct timeval tv;
        gettimeofday( &tv, nullptr );
        time = tv.tv_sec;
        time *= 1000000;
        time += tv.tv_usec;
        time *= 10;
#endif
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


Be::Time BlueOS::GetActualTime() const
{	
	return mUTCAdj + const_cast<SuperTime &>(mWallclock).Get(); // Adjust for server sync
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


void BlueOS::NextScheduledEvent(int millisec)
{
#if CCP_STACKLESS
	if( millisec <= 0 )
	{
		SetEvent(gBreakSleep); //If we are sleeping (this could be an async window message we're in, wake up!)
		mNextScheduledEvent = -1;
	}
	else if( millisec < mNextScheduledEvent )
	{
		mNextScheduledEvent = millisec;
	}
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
	err.mWinError = 0;
	if( error == BE32 )
	{
#ifdef _WIN32
		err.mWinError = GetLastError();
		BlueWinError winerr(err.mWinError);
		CCP_LOGERR( winerr );
#endif
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

		CCP_LOGERR( buff );
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
#ifdef _WIN32
			// win 32 code
			BlueWinError winerr(err->mWinError);
			stream << '{' << err->mWinError << ':' << (const char*)winerr << '}';
#endif
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
		strncpy_s( *errorstring, len - 1, error.c_str(), _TRUNCATE);
	}

}


//--------------------------------------------------------------------
// Config file
//--------------------------------------------------------------------


//--------------------------------------------------------------------
// Config.ini access function.
//--------------------------------------------------------------------
const char* BlueOS::KeyVal( const char* key, const char* value, bool setValue )
{
#if BLUE_WITH_PYTHON
	bool isLanguageID = strcmp(key, "languageID") == 0;
	bool isCacherCalling = strcmp(value, "_cachercalling") == 0;
	if( isLanguageID && !isCacherCalling )
	{
		// Use "cached" copy.
		return mLanguageID;
	}

	//Delegate all this to the python builtin "prefs"
	PyObject* prefs = PyDict_GetItemString(PyEval_GetBuiltins(), "prefs");
	if( !prefs )
	{
		return "";
	}

	if( setValue )
	{
		PyObject* pyvalue = PyString_FromString(value);

		if( PyObject_SetAttrString(prefs, (char*)key, pyvalue) == -1 )
		{
			PyOS->PyError();
		}

		Py_DECREF(pyvalue);
		return value;
	}
	else
	{
		BluePy pyvalue(PyObject_GetAttrString(prefs, (char*)key));
		if( pyvalue )
		{
			BluePy string(PyObject_Str(pyvalue));
			CCP_FREE(mLastKeyValue);
			if( string )
			{
				mLastKeyValue = CCP_STRDUP( "BlueOS::KeyVal", PyString_AS_STRING(string.o));
			}
			else
			{
				PyErr_Clear();
				mLastKeyValue = CCP_STRDUP( "BlueOS::KeyVal empty", "");
			}
			return mLastKeyValue;
		}
		else
		{
			PyErr_Clear();
			if( isLanguageID && isCacherCalling )
			{
				return "";
			}
			else
			{
				return value;
			}
		}

	}
#else
	return "";
#endif
}



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

	return PyInt_FromLong((long)diff);
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

	return PyInt_FromLong((long)diff);
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

#ifdef _WIN32
	SYSTEMTIME st;

	if (!FileTimeToSystemTime((FILETIME*)&time, &st))
	{
		SetError(BE32, Clsid(), "Couldn't convert time.");
		return nullptr;
	}

	PyObject* list = PyList_New(8);

	if( !list )
	{
		return nullptr;
	}

	PyList_SET_ITEM(list, 0, PyInt_FromLong(st.wYear));
	PyList_SET_ITEM(list, 1, PyInt_FromLong(st.wMonth));
	PyList_SET_ITEM(list, 2, PyInt_FromLong(st.wDayOfWeek));
	PyList_SET_ITEM(list, 3, PyInt_FromLong(st.wDay));
	PyList_SET_ITEM(list, 4, PyInt_FromLong(st.wHour));
	PyList_SET_ITEM(list, 5, PyInt_FromLong(st.wMinute));
	PyList_SET_ITEM(list, 6, PyInt_FromLong(st.wSecond));
	PyList_SET_ITEM(list, 7, PyInt_FromLong(st.wMilliseconds));

	return list;

#else
	// TODO: Implement for non-Win32
	return nullptr;

#endif
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

#ifdef _WIN32
	SYSTEMTIME st;
	st.wYear = i[0];
	st.wMonth = i[1];
	st.wDay = i[2];
	st.wHour = i[3];
	st.wMinute = i[4];
	st.wSecond = i[5];
	st.wMilliseconds = i[6];

	Be::Time ft;

	if (!SystemTimeToFileTime(&st, (FILETIME*)&ft))
	{
		SetError(BE32, Clsid(), "Couldn't convert time.");
		return nullptr;
	}

	return PyLong_FromLongLong(ft);

#else

	// TODO: Implement for non-Win32
	return nullptr;

#endif

}


PyObject* BlueOS::PyTimeDiffAsParts( PyObject* args )
{
#ifdef _WIN32
	PyObject* t1;
	PyObject* t2 = NULL;

	if( !PyArg_ParseTuple(args, "O!|O!", &PyLong_Type, &t1, &PyLong_Type, &t2) )
	{
		return NULL;
	}

	Be::Time time1 = PyLong_AsLongLong(t1);
	if (time1 == -1 && PyErr_Occurred()) return 0;
	Be::Time time2;
	if (t2)
	{
		time2 = PyLong_AsLongLong(t2);
		if( time2 == -1 && PyErr_Occurred() )
		{
			return 0;
		}
	}
	else
	{
		time2 = mRealTime;  // TDTODO
	}

	Be::Time diff = time2 - time1;

	SYSTEMTIME sdiff;	
	if( !FileTimeToSystemTime((FILETIME*)&diff, &sdiff) )
	{
		SetError(BE32, Clsid(), "Couldn't convert time.");
		return nullptr;
	}
	Be::Time n = 0;
	SYSTEMTIME nullTime;
	if( !FileTimeToSystemTime((FILETIME*)&n, &nullTime) )
	{
		SetError(BE32, Clsid(), "Couldn't convert time.");
		return nullptr;
	}

	PyObject* list = PyList_New(7);

	if( !list )
	{
		return NULL;
	}

	PyList_SET_ITEM(list, 0, PyInt_FromLong( sdiff.wYear - nullTime.wYear));
	PyList_SET_ITEM(list, 1, PyInt_FromLong( sdiff.wMonth - nullTime.wMonth));	
	PyList_SET_ITEM(list, 2, PyInt_FromLong( sdiff.wDay - nullTime.wDay));
	PyList_SET_ITEM(list, 3, PyInt_FromLong( sdiff.wHour - nullTime.wHour));
	PyList_SET_ITEM(list, 4, PyInt_FromLong( sdiff.wMinute - nullTime.wMinute));
	PyList_SET_ITEM(list, 5, PyInt_FromLong( sdiff.wSecond - nullTime.wSecond));
	PyList_SET_ITEM(list, 6, PyInt_FromLong( sdiff.wMilliseconds - nullTime.wMilliseconds));

	return list;

#else
	
	// TODO: Implement for non-Win32
	return nullptr;

#endif
}

PyObject* BlueOS::PyFormatUTC( PyObject* args )
{
#ifdef _WIN32

	PyObject* t = NULL;
	long localtime = 1;

	if( !PyArg_ParseTuple(args, "|O!i", &PyLong_Type, &t, &localtime) )
	{
		return NULL;
	}

	Be::Time time;
	if( t )
	{
		time = PyLong_AsLongLong(t);
		if( time == -1 && PyErr_Occurred() )
		{
			return 0;
		}
	}
	else
	{
		time = mRealTime;  // TDTODO
	}

	if (localtime)
	{
		FileTimeToLocalFileTime((FILETIME*)&time, (FILETIME*)&time);
	}

	SYSTEMTIME st;
	FileTimeToSystemTime((FILETIME*)&time, &st);

	char tmp[1024];

	PyObject* list = PyList_New(3);

	if( !list )
	{
		return NULL;
	}

	if( !GetDateFormat(LOCALE_INVARIANT , DATE_SHORTDATE, &st, NULL, tmp, sizeof (tmp)) )
	{
		strcpy_s(tmp, "[error]");
	}

	PyList_SET_ITEM(list, 0, PyString_FromString(tmp));

	if( !GetDateFormat(LOCALE_INVARIANT , DATE_LONGDATE, &st, NULL, tmp, sizeof (tmp)) )
	{
		strcpy_s(tmp, "[error]");
	}

	PyList_SET_ITEM(list, 1, PyString_FromString(tmp));

	if( !GetTimeFormat(LOCALE_SYSTEM_DEFAULT ,
		LOCALE_NOUSEROVERRIDE | TIME_NOTIMEMARKER | TIME_FORCE24HOURFORMAT,
		&st, NULL, tmp, sizeof (tmp)))
	{
		strcpy_s(tmp, "[error]");
	}

	PyList_SET_ITEM(list, 2, PyString_FromString(tmp));

	return list;

#else

	// TODO: Implement for non-Win32
	return nullptr;

#endif
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
		CCP_LOG_CH( s_chOS, "TIDI Init recv from %I64u", fromID);
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
			CCP_LOGWARN_CH( s_chOS, "TIDI Event tossed because of master - %I64d %I64d", fromID, mDilationSyncMaster);
			return;
		}

		// If it's older than our current sync, it's stale and needs to go away
		if( nextEventWallclock < mDilationSyncBaseWallclockTime )
		{
			CCP_LOGWARN_CH( s_chOS, "TIDI Event tossed because of time - %I64d %I64d", nextEventWallclock, mDilationSyncBaseWallclockTime);
			return;
		}

		// Store the update so the next clock loop can take it into account
		BlueOS::PendingDilationEvent newEvent;
		newEvent.mNextDilationEventSimTime = nextEventSim;
		newEvent.mNextDilationEventWallclockTime = nextEventWallclock;
		newEvent.mNextDilationFactor = nextDilationFactor;
		mPendingDilationEvents.push(newEvent);
		CCP_LOG_CH( s_chOS, "TIDI Event recv - %f %I64d", newEvent.mNextDilationFactor, newEvent.mNextDilationEventWallclockTime);
	}
	else
	{
		// Detach - if it's from our master, then we have no master.
		if( fromID != mDilationSyncMaster )
		{
			CCP_LOGWARN_CH( s_chOS, "TIDI Detach tossed because of master - %I64d %I64d", fromID, mDilationSyncMaster);
			return;
		}
		CCP_LOG_CH( s_chOS, "TIDI Detach recv from %I64u", fromID);

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
	int taskletQueueTickStart, taskletQueueTickEnd;
	float frameTime, maxFrameTime;
	PyOS->GetSchedulerStats(taskletQueueTickStart, taskletQueueTickEnd, frameTime, maxFrameTime);

	if( taskletQueueTickEnd > 0 )
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
#ifdef _WIN32
PyObject* BlueOS::PyHeapCompact(PyObject* args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;

	_heapmin();

	HANDLE dummy[1];
	DWORD numheaps = GetProcessHeaps(1, dummy);
	HANDLE* heaps = new HANDLE[numheaps+10];
	DWORD got = GetProcessHeaps(numheaps, heaps);

	if (got <= numheaps)
	{
		for (DWORD i = 0; i < got; i++)
		{
			HeapCompact(heaps[i], 0);
		}
	}

	delete[] heaps;

	Py_INCREF(Py_None);
	return Py_None;
}

PyObject* BlueOS::PyGlobalMemoryStatus(PyObject* args)
{
	if( !PyArg_ParseTuple(args, "") )
	{
		return NULL;
	}

	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);

	return Py_BuildValue(
		"[ssssssss][iKKKKKKK]",
		"memoryLoad",
		"totalPhys", "availPhys",
		"totalPageFile", "availPageFile", 
		"totalVirtual", "availVirtual",
		"availExtendedVirtual",
		status.dwMemoryLoad,
		status.ullTotalPhys, status.ullAvailPhys,
		status.ullTotalPageFile, status.ullAvailPageFile,
		status.ullTotalVirtual, status.ullAvailVirtual,
		status.ullAvailExtendedVirtual
		);
}
#endif

PyObject* BlueOS::PySetAppTitle(PyObject* args)
{
#ifdef _WIN32
	const char* title;

	if( !PyArg_ParseTuple(args, "s", &title) )
	{
		return NULL;
	}

	SetConsoleTitle(title);

	Py_INCREF(Py_None);
	return Py_None;
#else

	// TODO: Implement for non-Win32
	return nullptr;

#endif
}

PyObject* BlueOS::PyGetAppTitle(PyObject* args)
{
#ifdef _WIN32
	if( !PyArg_ParseTuple(args, "") )
	{
		return NULL;
	}

	const int titleSize = 512;
	char title[ titleSize ];

	GetConsoleTitle( title, titleSize);

	return PyString_FromString( title );
#else

	// TODO: Implement for non-Win32
	return nullptr;

#endif
}

#ifdef _WIN32

// For ShellExecute
#include <shellapi.h>

PyObject* BlueOS::PyApplyPatch(PyObject* args)
{
	PyObject *fileO, *paramO;
	PyObject *fileU, *paramU;
	const wchar_t* patchFile;
	const wchar_t* parameter;

	if( !PyArg_ParseTuple(args, "OO", &fileO, &paramO) )
	{
		return NULL;
	}

	fileU = PyUnicode_FromObject(fileO);
	if (!fileU) return 0;
	paramU = PyUnicode_FromObject(paramO);
	if( !paramU )
	{
		Py_DECREF(fileU);
		return 0;
	}

	patchFile = PyUnicode_AsUnicode(fileU);
	parameter = PyUnicode_AsUnicode(paramU);

	CCP_LOG_CH( s_chOS, "ApplyPatch: patchFile=%S parameter=%S",patchFile, parameter);

	ShellExecuteW(NULL, NULL, patchFile, parameter, NULL, SW_SHOWNORMAL);

	Py_DECREF(fileU);
	Py_DECREF(paramU);
	::TerminateProcess(::GetCurrentProcess(), 0);

	// this will never happen, put it here to please the compiler.
	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* BlueOS::PyShellExecute(PyObject* args)
{
	PyObject *fn, *params=0;
	if( !PyArg_ParseTuple(args, "O!|O!", &PyBaseString_Type, &fn, &PyBaseString_Type, &params) )
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

	std::wstring file(PyUnicode_AsUnicode(fn));
	Py_DECREF(fn);
	bool http = file.find(L"http://") == 0 || file.find(L"https://") == 0;

	if (!http)
	{
		//not a http request. rewrite the file
		file = BePaths->ResolvePathW( file.c_str() );
	}

	//since IE7, urls cannot be opened directly from multithreaded applications.  Instead, we must have
	//rundll do it for us. But under Transgaming we must preserve the old behaviour, since it doesn't
	//support rundll32.exe.

	SHELLEXECUTEINFOW ei;
	memset(&ei, 0, sizeof(ei));
	ei.cbSize = sizeof(ei);
	std::wstring tmp;
	if( !http || IsTransgaming() )
	{
		ei.lpFile = file.c_str();
		if( params )
		{
			ei.lpParameters = PyUnicode_AsUnicode(params);
		}
	}
	else
	{
		//have rundll handle http if not running under TransGaming
		ei.lpVerb = L"open";
		ei.lpFile = L"rundll32.exe";
		tmp = L"url.dll,FileProtocolHandler ";
		tmp += file.c_str();
		ei.lpParameters = tmp.c_str();
	}
	ei.nShow = SW_SHOWNORMAL;
	BOOL OK = ShellExecuteExW(&ei);

	if( !OK )
	{
		CW2A filename(file.c_str());
		return PyErr_SetFromWindowsErrWithFilename(GetLastError(), (char*)filename);
	}

	Py_INCREF(Py_None);
	return Py_None;
}

#endif

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
}

const std::vector<std::wstring>& BlueOS::GetStartupArgs() const
{
	return m_startupArgs;
}

const wchar_t* BlueOS::GetLanguageId()
{
	return m_cachedLanguageId.c_str();
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

void BlueOS::RunCatchUpTicks( float deltaT_sec )
{
	// Run all of our catchup ticks if we have any
	if( !mCatchUpTickerHeap.empty() )
	{
		Be::Time startTickTime, timeAfterTick, elapsedTime;

		//Track our out of budget ticks here
		CatchupTickerList outOfBudgetTickers("BlueOS::PumpOS::outOfBudgetTickers");  

		// While we still have ticks to process
		startTickTime = GetActualTime();
		while( !mCatchUpTickerHeap.empty() && (mCatchUpTickerHeap.front()->mNextTickTime <= mRealTime) )
		{
			CCP_STATS_ZONE( "BlueOS/PumpOS/CatchupTickers" );

			CatchupTicker *ticker = mCatchUpTickerHeap.front();

			const char* taskname = ticker->mCookie;

			// Switch to appropriate ticker
#if BLUE_WITH_PYTHON
			AutoTasklet _at2(PyOS->GetTaskletTimer(), taskname ? taskname : "(null ticker?)");
#endif
			ticker->mCb->OnTick(mRealTime, deltaT_sec, (void *)ticker->mCookie);
			ticker->mNeedsPostFrameTick = true;

			if( mDebugLevel > 0 && HeapScrewed() )
			{
				CCP_LOGERR_CH( s_chOS, "Heap corrupt after ticking %s", taskname);
			}

			// Determine the amount of elapsed time
			timeAfterTick = GetActualTime();
			elapsedTime = timeAfterTick - startTickTime;

			// Remove the tick off the current heap we just ran
			pop_heap( mCatchUpTickerHeap.begin(), mCatchUpTickerHeap.end(), CatchUpTickerComparator() );
			mCatchUpTickerHeap.pop_back();

			// Setup tick to do next tick
			ticker->mNextTickTime += ticker->mTimeBetweenTicks;

			// Is this tick not going to occur this frame? 
			if( ticker->mNextTickTime>mRealTime )
			{
				// Reset the budget for the ticks
				ticker->mBudgetLeft = ticker->mPerFrameTimeBudget;

				// Push the tick onto the heap
				mCatchUpTickerHeap.push_back(ticker);
				push_heap(mCatchUpTickerHeap.begin(), mCatchUpTickerHeap.end(), CatchUpTickerComparator());
			}
			else if( ticker->mBudgetLeft>elapsedTime ) // Check the budget
			{
				// Decrement the time spent from the budget
				ticker->mBudgetLeft -= elapsedTime;

				// Push the tick onto the heap
				mCatchUpTickerHeap.push_back(ticker);
				push_heap(mCatchUpTickerHeap.begin(), mCatchUpTickerHeap.end(), CatchUpTickerComparator());
			}
			else
			{
				// Ran out of the budget for this catch up tick
				ticker->mBudgetLeft = ticker->mPerFrameTimeBudget;
				outOfBudgetTickers.push_back(ticker);
			}

			// Time after the tick becomes the start time for the next tick
			startTickTime = timeAfterTick;
		}

		// Readd back to the heap all the ticks that ran out of budget
		for( CatchupTickerList::iterator iter = outOfBudgetTickers.begin(); iter != outOfBudgetTickers.end(); ++iter )
		{
			// Push the tick onto the heap
			mCatchUpTickerHeap.push_back(*iter);
			push_heap(mCatchUpTickerHeap.begin(), mCatchUpTickerHeap.end(), CatchUpTickerComparator());
		}

		// Call all the post frame ticks on the catchup ticks that ran this frame
		for( CatchupTickerVector::iterator iter=mCatchUpTickerHeap.begin(); iter!=mCatchUpTickerHeap.end(); ++iter )
		{
			CatchupTicker *ticker = *iter;
			if( ticker->mNeedsPostFrameTick )
			{
				ticker->mCb->OnPostFrameTick(mRealTime, (void *)ticker->mCookie);
				ticker->mNeedsPostFrameTick = false;
			}
		}

		// Mark the next scheduled event based on the front catch tick
		Be::Time timeTillNextTick = mCatchUpTickerHeap.front()->mNextTickTime - mRealTime;

		// Set next time till tick in ms (doesn't matter if we are less than 0 since gets handled by NextScheduledEvent)
		NextScheduledEvent((int)(timeTillNextTick/10000));
	}
}

void BlueOS::TickTickers()
{
	//Tick tickers
	for( TickIt i = mTickers.begin(); i != mTickers.end(); ++i )
	{
		const char* taskname = i->mCookie;
		// Switch to appropriate ticker
#if BLUE_WITH_PYTHON
		AutoTasklet _at2(PyOS->GetTaskletTimer(), taskname ? taskname : "(null ticker?)");		
#endif
		i->mCb->OnTick(mRealTime, mSimTime, (void*)taskname);

#if BLUE_WITH_PYTHON
		PyOS->PyFlushError(taskname);
#endif

		if( mDebugLevel > 0 && HeapScrewed() )
		{
			CCP_LOGERR_CH( s_chOS, "Heap corrupt after ticking %s", taskname);
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

#ifdef OptimizeOff
#pragma optimize("", on)
#endif
