// Copyright © 2000 CCP ehf.

/* 
	*************************************************************************

	BlueOS.h

	Project:   Blue

	Description:   

		Implementation of IBlueOS interface


	Dependencies:

		Blue

	*************************************************************************
*/

#ifndef _BLUEOS_H_
#define _BLUEOS_H_

#include "IBlueOS.h"
#include "IBluePython.h"
#include "BlueTime.h"
#include "WatchdogThread.h"
#include "manifest.h"

#include <vector>
#include <list>
#include <queue>
#include <set>

const int MAXTIMESAMPLES = 5;
const int nTimeStamps = 150;

BLUE_DECLARE( BlueOS );

class BlueOS : 
	public IBlueOS,
	public BeInfo
{
public:
	BlueOS();
	~BlueOS();
	
	void SetTime(Be::Time time = 0);
	Be::Time GetActualTime() override;
	Be::Time GetCurrentFrameTime() const override { return mSimTime; }

	bool IsOnMainTasklet();

#if BLUE_WITH_PYTHON
	PyObject *mFrameClock;
#endif

	Be::Time GetSmoothedTime();
	
	typedef TrackableStdVector<TerminationCallback*> TerminationCallbacks;
	TerminationCallbacks mIndispensableTerminationSteps;
	void RegisterIndispensableTerminationStep( TerminationCallback* callback ) override;

	// error stuff
	typedef TrackableStdVector<IBlueOS::Error> ErrorLog;
	typedef ErrorLog::iterator ErrIt;
	ErrorLog mErrorLog;
	CcpMutex mErrorLogMutex;

	// Blue timekeeping
	SuperTime mWallclock; //time unit giving us precise time
	Be::Time mUTCAdj;	  //time adjustment from server
#if CCP_STACKLESS
    Be::Time mTimeSyncAdjust; // Time adjustement from the server that we haven't applied yet - to be applied slowly over time
    double mTimeSyncAdjustFactor;
#endif

	// scheduling and event notifications
	BeTimer mTimer;
	bool mUseRDTSC; //if true, use the RDTSC instruction for performance counting

	// Pumping
	bool mInsidePump;

	//  Debugging
	long mDebugLevel;

	// languageID for ResFile worker threads.
	std::wstring mLanguageID;
	
	// "Slug" Support
	float mSlugTimeMinMs;
	float mSlugTimeMaxMs;
	float mSlugTimeCurrentMs;
	float mSlugTimeDeltaMs;
	bool mIsSlugTimeIncreasing;
	void InitSlug();
	void UpdateSlugTime( float deltaTime );
	void DoSlug( float deltaTime );

	// Variable Delta-T Support
	bool mUseNominalDeltaT;
	bool mUseSmoothedDeltaT;
	float mNominalDeltaTSec;
	float mTimeScaler;
	void InitVariableTicking();
	void ComputeTimeValues(Be::Time* ptrActualTime, float* ptrDeltaT_sec);

	struct Ticker
	{
		Ticker(IBlueEvents *cb, void* cookie);
		bool operator == (const Ticker &o) const {return mCb==o.mCb && mCookie==o.mCookie;}
		IBlueEvents* mCb;
		const char* mCookie;
	};

	// We use an array here, because the order of registered tickers matters!
	// no vector, since vector iterators may invalidate when vector grows
	// during recursion.
	typedef TrackableStdList<Ticker> Tickers;
	typedef Tickers::iterator TickIt;
	Tickers mTickers;

	// framerate bookkeeping
	Be::Time mTimeStamps[nTimeStamps];  //for the running fps average
	int mTimeStampIdx;
	// int m_pumpTicksTotal; moved to BeInfo

	//time outside blueOS
	Be::Time mExitTime, mExternalTime;
	
	CcpProcessId_t mPID; // Process ID

	bool IsPackaged() override
	{
		return mPackaged;
	}
	
	// class registration stuff
private:

	bool mPackaged = false;

	uint64_t m_startupTime;
	
	IBlueCallbackManPtr m_callbackManager;

	// Command line arguments
	std::vector<std::wstring> m_startupArgs;
	std::map<std::wstring, std::wstring> m_startupArgsMap;

	// Sim Clock management state
	class PendingDilationEvent
	{
	public:
		Be::Time mNextDilationEventSimTime;	// The simulation time at which the following dilation factor will apply
		Be::Time mNextDilationEventWallclockTime;	// The wallclock time at which the following dilation factor will apply - so this can be used as the new sync basis
		double mNextDilationFactor;	// The pending dilation factor.  Delay is given so that clients can more closely follow changes.

		bool operator<(const PendingDilationEvent& other) const
		{
			// We're going to use this in a priority queue, so reverse order so sooner times come first
			return mNextDilationEventWallclockTime > other.mNextDilationEventWallclockTime;
		}
	};
	std::priority_queue<PendingDilationEvent> mPendingDilationEvents;

	std::vector<ISimTimeRebaseNotify*> m_simRebaseCallbacks;

	Be::Time mDilationSyncBaseWallclockTime;	// The Wallclock time of the last sync event.  Used only if mDynamicSimDilationEnabled is false.
	Be::Time mDilationSyncBaseSimTime;	// The Sim time of the last sync event.  Used only if mDynamicSimDilationEnabled is false.
	unsigned long long mDilationSyncMaster;  // Which nodeID is my master, so I can ignore update packets from anyone else

	bool mSimClockLockedToRealClock;	// This is a killswitch for the sim clock.  Defaulted true, turns false if TiDi is enabled or a TiDi packet is recieved.  If true, mSimTime will be exactly the same as mRealTime
	bool mDynamicSimDilationEnabled;    // True means the mSimDilation will be driven by load, within the constraints below.  This is appropriately true on (some) servers, should always be false on clients.
	double mMinSimDilation;		// The minimum time multiplier.  .1 would mean running at 10% time is the most dilated we can get
	double mMaxSimDilation;		// The maximum time multiplier.  Should almost certainly be 1 unless you're doing silly things.
	std::map<unsigned long long, int> mSimDilationSyncClients;	// A set of client IDs to send sim clock sync updates to, refcounted

	Be::Time mLastOverloadedTime; // When were we last overloaded
	Be::Time mLastUnderloadedTime; // The last time we were underloaded
	Be::Time mDilationOverloadThreshold; // How long we have to be overloaded before we slow things down
	Be::Time mDilationUnderloadThreshold; // ...same for underloaded and speeding up.
	double mDilationOverloadAdjustment; // What to multiply the dilation factor by in order to slow down
	double mDilationUnderloadAdjustment; // Same for speeding up

	void EvaluateTimeDilation();
	void SendDilationEvent(PendingDilationEvent newEvent);
	friend void SimClockPacketCallBackHelper( const unsigned long long fromID, const int blueNetType, const char* data, const int len );
	void SimClockPacketCallBack( const unsigned long long fromID, const int blueNetType, const char* data, const int len );

	WatchdogThread m_frameTimeWatchdog;
	uint32_t m_frameTimeTimeout;
	int32_t m_desiredFrameTimeMilliseconds{10};
	
	uint32_t GetFrameTimeTimeout() const;
	void SetFrameTimeTimeout( uint32_t val );

	uint32_t GetDesiredFrameTimeMilliseconds() const;
	void SetDesiredFrameTimeMilliseconds( uint32_t val );

	int32_t GetSleepTime() const;
	void SetSleepTime( int32_t val );

	// If true (the default) then Pump will advance the time. This can be set to false to disable
	// time advancement - used by butter smooth rendering where fixed time steps are used and time
	// is set explicitly with SetTime.
	bool mAdvanceTimeInPump;


public:

	/////////////////////////////////////////
	// Blue class info

	EXPOSE_TO_BLUE();

	/////////////////////////////////////////
	// Python thunkers

#if BLUE_WITH_PYTHON
	PyObject* PyGetTime( PyObject* args );  // TDTODO - Depricate me!
	PyObject* PyGetWallclockTime( PyObject* args );
	PyObject* PyGetWallclockTimeNow( PyObject* args );
	PyObject* PyGetSimTime( PyObject* args );
	PyObject* PyGetCycles( PyObject* args );
	PyObject* PySetTime( PyObject* args );
	PyObject* PyTimeDiffInMs( PyObject* args );
	PyObject* PyTimeDiffInUs( PyObject* args );

	PyObject* PyTimeFromDouble( PyObject* args );
	PyObject* PyTimeAsDouble( PyObject* args );
	PyObject* PyTimeAddSec( PyObject* args );

	PyObject* PyGetTimeParts( PyObject* args );
	PyObject* PyGetTimeFromParts( PyObject* args );

	PyObject* PyGetCpuTime( PyObject* args );

	PyObject* PyEnableSimDilation( PyObject* args );
	PyObject* PyRegisterClientIDForSimTimeUpdates( PyObject* args );
	PyObject* PyUnregisterClientIDForSimTimeUpdates( PyObject* args );

	PyObject* PySetAppTitle( PyObject* args );
	
	PyObject* PyShellExecute( PyObject* args );

	PyObject* PyTerminate( PyObject* args );

	PyObject* PyStacklessMain( PyObject* args );
#endif

	/////////////////////////////////////////
	// IBlueOS interface

	// Blue OS startup / termination
	bool Startup(
		int pyOptimizeVersion
		) override;

	bool RunStackless() override;

	void Terminate( int retCode ) override;
	
	// Scheduling and such...
	void RegisterForTicks(
		IBlueEvents *cb,
		void* cookie
		) override;

	void UnregisterForTicks(
		IBlueEvents *cb,
		void* cookie
		) override;

	void RegisterForSimTimeRebase( ISimTimeRebaseNotify* cb ) override;

	void UnregisterForSimTimeRebase( ISimTimeRebaseNotify* cb ) override;


	// Error reporting facility
	void SetError(
		long error,
		const Be::Clsid* reporter,	// optional class id
		const char* format,
		...
		) override;

	const Error* GetError(
		long index
		) override;

	void FormatError(
		char** errorstring
		) override;

	const wchar_t* GetLanguageId() override;

	// put here temporarily
	void PumpOS(
		) override;

	BeInfo* GetInfo(
		) override;
	
	// Used by ExeFile to pass along the startup arguments. ExeFile may expand
	// arguments from a file so we can't rely on GetCommandLineW to get the
	// arguments.
	void SetStartupArgs( const std::vector<std::wstring>& args ) override;

	// Get the list of arguments passed on the command line, after any
	// expansion of arguments coming from a file with the @ convention.
	const std::vector<std::wstring>& GetStartupArgs() const override;

	// Returns true if <arg> is in the list of command line arguments.
	bool HasStartupArg( const std::wstring& arg ) const override;

	// Returns the value associated with the command line argument.
	// If /arg=value is on the command line, this method returns value.
	std::wstring GetStartupArgValue( const std::wstring& arg ) const override;
    
    void ShowErrorMessageBox( const wchar_t* title, const wchar_t* message );

	bool ConstructPathListFromManifest( std::vector<std::wstring>& pathlist, bool verifyManifest ) override;
	void SetMarkupZonesInPython( bool markupZonesInPython ) override;
	void GetInitTab( std::vector<_inittab>& tabs ) const override;

private:
    void PumpOSInternal();
    
	void Sleep();
	void TickTickers();
	void CaptureLogCountsToStats();

	bool VerifyManifestAndGatherDirectives( directives_t& directives );
	void ShowMessageBoxForVerificationFailure( const std::string& errmsg );
	void ProcessLibDirectives( const directives_t& directives, std::vector<std::wstring>& zips );
	std::chrono::time_point<std::chrono::steady_clock> mNextFrame;
};


TYPEDEF_BLUECLASS(BlueOS);

#endif
