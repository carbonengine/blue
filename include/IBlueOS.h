// Copyright © 2000 CCP ehf.

/* 
	*************************************************************************

	IBlueOS.h

	Project:   Blue

	Description:   

		IBlueOS is the interface on a static object which handles most Blue
		related activity, like event scheduling, class info queries, class
		factories, error mechanism, etc... 'BeOS' is a global variable which 
		always points to the static instance of IBlueOS.

		It's a bit of a hassle changing this interface because it will require
		recompile on everything that depends on Blue.


	Dependencies:

		Blue

	*************************************************************************
*/

#ifndef _IBLUEOS_H_
#define _IBLUEOS_H_

#include "Blue.h"
#include "IBluePython.h"
#include "IBlueCallbackMan.h"

#include <vector>

// forward decls
struct IBlueEvents;
struct ICatchupTicks;
struct ISimTimeRebaseNotify;

//////////////////////////////////////////////////////////////////////
//
// BlueOS Interface, support classes and stuff...
//
//////////////////////////////////////////////////////////////////////

enum BLUEERROR
{
	BECLEAR		=  0,			// clears the error log
	BEDEF		= -1,			// no particular error value, default value
	BE32		= -2,			// will call GetLastError()
	BEFLUSH     = -3,			// flushes the error log out to the logger, and clears it
};

struct BeInfo
{
	long mStructSize;			// size of this struct

	// time management
	Be::Time mRealTime;			// Blue UTC time
	Be::Time mSimTime;			// It's like mRealTime but sometimes slowed down in order to manage load
	double mSimDilation;		// This is the current factor between Real and Sim time advancement
	double mDilationSyncFactor;	// The dilation factor we are currently basing synchronization on.  May be different than mSimDilation depending on the needs of synchronization.
	
	// framerate
	double mFps;				// number of frames per second
	Be::Time mFpsRefreshRate;	// time between recalc of fps
	double mLockFramerate;		// framerate locking, 0.0 for no locking
	long m_pumpTicksTotal;		// Number of BlueOS Pump calls since creation
	long m_ioRunsTotal;			// Number of special IO scheduling runs

	// pumping stuff
	long mSleepTime;
	long mOverrideFG;			// -1 : always in background, 1: always in foreground

	// ugh!
	Be::Time mStartTime;
	Be::Time mExtraFakeTime;
	double mTimeWarp;			// speed of time, 1.0 is 1 to 1.

	bool mTurnOffSetError;		// quiet down SetError - no error blurbed while
								// this flag is 'true'. SetError will reset this
								// flag each time.

	Be::Time mTimeAdjusted;		// if not 0, SetTime has been called with this diff.

	bool mMiniDump;				// Should our ExeFile handle its own minidumps
	// Build number
	long mBuildno;
};


BLUE_INTERFACE(IBlueOS) : public IRoot
{
	
	//--------------------------------------------------------------------
	// Blue OS startup / shutdown
	//--------------------------------------------------------------------

	// the startup
	virtual bool Startup(
		int pyOptimizeFlag
		) = 0;
	
	// the way to end things (we always shutdown with a terminate).  For more info
	// read this: http://carbon/wiki/Blue_Shutdown
	virtual void Terminate( int retCode=0 ) = 0;

	typedef void (TerminationCallback)();

	// This method has replace the old Shutdown registration methods.
	// If you have action that must be taken in a C++ DLL before the application
	// is shut down with a termination signal, create a termination callback and register it
	// by calling this method. Most often a DLL can be terminated without any special
	// handling; this method should therefore only be required for a few of our DLLs.
	virtual void RegisterIndispensableTerminationStep( TerminationCallback* callback ) = 0;


	//--------------------------------------------------------------------
	// Scheduling and such...
	//--------------------------------------------------------------------
	
	virtual void RegisterForTicks(
		IBlueEvents *cb,
		void* cookie
		) = 0;

	virtual void UnregisterForTicks(
		IBlueEvents *cb,
		void* cookie
		) = 0;

	virtual void RegisterForSimTimeRebase( ISimTimeRebaseNotify* cb ) = 0;

	virtual void UnregisterForSimTimeRebase( ISimTimeRebaseNotify* cb ) = 0;

	//--------------------------------------------------------------------
	// Error reporting facility
	//--------------------------------------------------------------------

	// 'error' is one of BLUEERROR enum values, or a HRESULT value
	// see comments for BLUERROR for further details
	virtual void SetError(
		long error,					// BLUEERROR enum or a HRESULT value
		const Be::Clsid* reporter = NULL,
		const char* format = NULL,
		...
		) = 0;

#if _WIN32
    typedef DWORD OsErrorType;
#else
    typedef errno_t OsErrorType;
#endif
    
	struct Error
	{
		long mError;
		const Be::Clsid* mSource;
		const char* mDescription;
		Be::Time mTimestamp;
		OsErrorType mOsError; // set if mError is BE32
	};
	
	
	// returns error log for index = 0 to n, NULL when no more
	// errors available
	virtual const Error* GetError(
		long index
		) = 0;

	// formats error into a beautiful report
	// returns BlueString in 'errorstring', i.e. call CCP_FREE on it
	virtual void FormatError(
		char** errorstring
		) = 0;

	virtual const wchar_t* GetLanguageId() = 0;

	//--------------------------------------------------------------------
	// Management and such
	//--------------------------------------------------------------------
	
	// put here temporarily
	virtual void PumpOS(
		) = 0;
	
	//--------------------------------------------------------------------
	// Information
	//--------------------------------------------------------------------
	virtual BeInfo* GetInfo(
		) = 0;

	virtual Be::Time GetActualTime(
		) = 0;

	virtual bool IsPackaged() = 0;

	// This returns the cached values of GetSmoothedTime
	virtual Be::Time GetCurrentFrameTime( ) const = 0;

	virtual bool RunStackless() = 0;

	// Used by ExeFile to pass along the startup arguments. ExeFile may expand
	// arguments from a file so we can't rely on GetCommandLineW to get the
	// arguments.
	virtual void SetStartupArgs( const std::vector<std::wstring>& args ) = 0;

	// Get the list of arguments passed on the command line, after any
	// expansion of arguments coming from a file with the @ convention.
	virtual const std::vector<std::wstring>& GetStartupArgs() const = 0;

	// Returns true if <arg> is in the list of command line arguments.
	virtual bool HasStartupArg( const std::wstring& arg ) const = 0;

	virtual void GetInitTab( std::vector<_inittab>& tabs ) const = 0;

	// Returns the value associated with the command line argument.
	// If /arg=value is on the command line, this method returns value.
	virtual std::wstring GetStartupArgValue( const std::wstring& arg ) const = 0;
	virtual void SetMarkupZonesInPython( bool markupZonesInPython ) = 0;
	virtual bool ConstructPathListFromManifest( std::vector<std::wstring> & pathlist, bool verifyManifest ) = 0;
};

extern BLUEIMPORT IBlueOS* BeOS;
extern "C" BLUEIMPORT IBlueOS* BlueGetBeOS();

struct IBlueEvents
{
	
	//--------------------------------------------------------------------
	// Basic event notification
	//--------------------------------------------------------------------
	
	virtual void OnTick(
		Be::Time realTime,		// Time since client started
		Be::Time simTime,		// The same, but for simulation time
		void* cookie			// user supplied cookie value
		) = 0;

	
};

// The interface to variable-rate tickers
struct IVariableTicker
{
	virtual void OnTick(
		Be::Time timestamp,		// Time since client started
		float deltaT_sec,		// Time elapsed since previous pump-cycle
		void* cookie			// user supplied cookie value
		) = 0;
};

// Inherit from IVariableTicker, to make CatchupTicks compatible with the new ticking code
struct ICatchupTicks : public IVariableTicker
{
	// This is called at the end of a frame indicating that
	// all ticks for this system in this frame have completed
	virtual void OnPostFrameTick(
		Be::Time timestamp,		// Time since client started
		void* cookie			// user supplied cookie value
		) = 0;
};

struct ISimTimeRebaseNotify
{
	virtual void OnSimClockRebase( Be::Time oldTime, Be::Time newTime ) = 0;
};

#endif
