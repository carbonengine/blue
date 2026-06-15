// Copyright © 2014 CCP ehf.

/* 
	*************************************************************************

	BluePython.h

	Author:    Matthias Gudmundsson
	Created:   Nov. 2000
	OS:        Win32
	Project:   Blue

	Description:   

		Implementation of IBluePyOS interface


	Dependencies:

		Blue

	(c) CCP 2000

	*************************************************************************
*/

#ifndef _BLUEPYTHON_H_
#define _BLUEPYTHON_H_

#include "IBluePython.h"
#include "IBlueOS.h"
#include "TaskletTimer.h"
#include "PyScheduler.h"

#include <socketmodule.h>

#include "manifest.h"

#include <vector>
#include <map>


// Forward declaration
PyObject* PySetLogEchoFunction( PyObject* self, PyObject* args );
PyObject* PyGetLogEchoFunction( PyObject* self, PyObject* args );

// FIXME need to update the call sites!
BLUE_CLASS( BlueCpuUsage ) : public IRoot
{
public:
	explicit BlueCpuUsage( IRoot* obj = nullptr ) {}
	virtual ~BlueCpuUsage() = default;

	EXPOSE_TO_BLUE();

	Be::Time timestamp{0};
	// CPU
	uint64_t userThreadCpuUsage{0};
	uint64_t userProcessCpuUsage{0};
	uint64_t kernelThreadCpuUsage{0};
	uint64_t kernelProcessCpuUsage{0};
	// Memory
	size_t pageFileUsage{0}; // also referred to as "Virtual Memory"
	size_t pythonMemoryUsage{0};
	size_t workingSetSize{0};
	uint64_t pageFaultCount{0};
	//	size_t blueMemory;
	// Scheduler
	double fps{0.0};
	size_t taskletsYielding{0};
	size_t taskletsSleeping{0};
	double taskletsSchedulerDuration{0.0};
	double taskletsSchedulerMaxDuration{ 0.0 };
	double taskletsSchedulerDurationOvershoot{ 0.0 };
	uint64_t taskletsQueued{0};
	uint64_t taskletsActive{ 0 };
	uint64_t scheduleManagersActive{ 0 };
	uint64_t channelsActive{ 0 };
	uint64_t taskletsProcessed{ 0 };
	uint64_t taskletsSwitched{ 0 };

};
BLUE_DECLARE_VECTOR( BlueCpuUsage );
TYPEDEF_BLUECLASS( BlueCpuUsage );

//////////////////////////////////////////////////////////////////////
//
// BluePyOS class
//
//////////////////////////////////////////////////////////////////////
BLUE_DECLARE( BluePyOS );

BLUE_CLASS( BluePyOS ) :
	public IBluePyOS
{
public:
	// ctor
	BluePyOS( IRoot* lockobj = NULL );

	// synchro
#if CCP_STACKLESS
	class Synchro* mSynchro;
	PyObject* mPySynchro;
#endif

#if CCP_STACKLESS
	// debug
	struct Thread
	{
		PyObject* mTraceback;
		PyObject* mContinuation;
	};

	typedef TrackableStdVector<Thread> Threads;
	Threads mThreads;
#endif

	void ProcessLibDirectives( const directives_t& directives, std::vector<std::wstring>& zips );
	bool VerifyManifestAndGatherDirectives( directives_t & directives );
	void ShowMessageBoxForVerificationFailure( const std::string& errmsg );

	// init funcs and corresponding fini functions
	bool InitBasicModuleSupport();
	bool FiniBasicModuleSupport();

	bool InitIncludePaths( std::wstring & path );


	void LogCpuUsageAndOtherStats();

	PyObject* mExceptionHandler;

	//The extra list of python hooks for context switch
	PyObject* mContextHooks;

	PyObject* CreateTaskletImpl(
		PyObject * meth,
		PyObject * args,
		PyObject * kw,
		PyObject * ctx );

	PyObject* CallPyObjectWithTrap(
		PyObject * meth,
		PyObject* args = 0,
		PyObject* ctx = 0 );


	struct ThreadSnapshot
	{
		uint64_t time;
		uint64_t kernel, user;
	};

	uint64_t mLastCpuUpdate;
	uint64_t mLastThreadCpuUsage;
	uint64_t mLastThreadKernelUsage;
	uint64_t mLastProcessCpuUsage;
	uint64_t mLastProcessKernelUsage;
	PBlueCpuUsageVector mCpuUsage;

	// This flag is set based on a command line argument (/telemetryMarkup).
	// This is used in bluepy.py to determine whether decorators  and metaclass
	// for markup do anything. The reason this variable lives here is to ensure
	// that it can be read from the command line before any Python code is loaded.
	bool mMarkupZonesInPython;

	long mSliceWarning;
	long mBeNiceSlice; //default benice in milliseconds
	long mPerformanceUpdateFrequency; // How often PumpPython updates process performance data

	// exit procs
	PyObject* mExitProcs; //a python list

#if CCP_STACKLESS
	CTaskletTimer mTTimer; //the new tasklet timing object
#endif

private:
	double GetTimeSinceSwitch( bool update = false ); //time since last tasklet switch
	bool UpdateTaskletRunTime( PyObject * tasklet, double elapsed );

private:
	PyObject* mBlueModule; //weak ref to us as a module;
	BluePy mTaskletExt; //the tasklet extension class
	bool mInit;
	int mSoftspace; //for python's print statement
	bool mPackaged;
	bool mInterpreterMode; // do we run in interpreter mode?

	BlueScriptCallback m_scatterEvent;
	BlueScriptCallback m_sendEvent;
	BlueScriptCallback m_chainEvent;

#if CCP_STACKLESS
	PyScheduler mScheduler; //to run the watchdog

	//The time when the last tasklet switch occurred
	uint64_t mLastSwitchTime;
	double mSwitchTimePeriod; //inverse of frequency
	BluePy mstoredContext_str; //attribute name strings
	BluePy mrunTime_str;
#endif

	BluePy mMainScheduler;

	PySocketModule_APIObject *mSocketAPI;

public:
	void OnTaskletSwitch( PyObject *from, PyObject *to ) override;

	bool RecurseFolder(
		PyObject * result,
		const char* directory,
		Py_ssize_t prefixlen,
		const char* filter );

private:
	void HandleException( const char* message );

//	startup functions
	static void EnsureAssertionsEnabled();

public:
	EXPOSE_TO_BLUE();

	PyObject* PyAddExitProc( PyObject* args );
	PyObject* PyGetArg( PyObject* args );
	PyObject* PyDumpState( PyObject* args );
	PyObject* Py_EnableTrace( PyObject* args );
	PyObject* Py_GetWrapperList( PyObject* args );
	PyObject* Py_GetObjectState( PyObject* args );
	PyObject* Pywrite( PyObject* args );
	PyObject* PyGetThunkers( PyObject* args );
	PyObject* PyCreateTasklet( PyObject* args );
	PyObject* PySetClipboardData( PyObject* args );
	PyObject* PyNextScheduledEvent( PyObject* args );
	PyObject* PyGetTimeSinceSwitch( PyObject* args );
	PyObject* PyBeNice( PyObject* args );
	PyObject* PyXUtil_Index( PyObject* args );
	PyObject* PyXUtil_Filter( PyObject* args );
	PyObject* PyGetMaxRunTime( PyObject* args );
	PyObject* PySetMaxRunTime( PyObject* args );
	

	//--------------------------------------------------------------------
	// IBluePyOS interface
	//--------------------------------------------------------------------
	// Magic blue to python marriage
	//--------------------------------------------------------------------

	// returns a pyobject representation of 'object'
	BluePythonObject* WrapBlueObject(
		IRoot * object ) override;

	const PyMethodDef* GetGenericThunker(
		const char* name,
		const Be::ClassInfo* type );


	//--------------------------------------------------------------------
	// Python engine
	//--------------------------------------------------------------------

	// the startup.
	// will pump python automatically in the context
	// of the callers thread
	bool Startup() override;

	// the shutdown
	void Shutdown(
		int level ) override;

	// the pumping
	int PumpPython(
		bool quit ) override;

	PyObject* BlueModule() override { return mBlueModule; }


	//--------------------------------------------------------------------
	// Convenience functions
	//--------------------------------------------------------------------

	PyObject* PyError(
		PyObject* exception = NULL ) override;

	bool PyFlushError(
		const char* whence ) override;

	PyObject* PyErr_BlueError() override;

	void RebaseSimClock( Be::Time oldTime, Be::Time newTime ) override;

	//Turn a python exception into a string
	void FormatException( char** result ) override;

	bool IsPackaged() override
	{
		return mPackaged;
	}
	void SetPackaged( bool packaged ) override
	{
		mPackaged = packaged;
	}
	void SetMarkupZonesInPython(bool markupZonesInPython) override
	{
		mMarkupZonesInPython = markupZonesInPython;
	}
	bool IsInterpreterMode() override
	{
		return mInterpreterMode;
	}

	bool CanYield() override;
	bool Yield() override;

	SchedulerStats& GetSchedulerStats( ) override;

//	void DoStackTrace(
//		PyObject* frame = 0 ) override;
//
//	PyObject* GetStackTrace(
//		PyObject* frame = 0 ) override;

	ITaskletTimer* GetTaskletTimer() override; //Get the tasklet timer object

	//blocktrapping call methods
	PyObject* CallMethodWithTrap( PyObject * target, const char* method, const char* ctxt, const char* format, ... ) override;

	bool PythonEvent( const char* event, PyObject* arg ) override;

public:
	PyObject* CreateTasklet(
		PyObject * meth,
		PyObject * args,
		PyObject * kw ) override;

	// --------------------------------------------------------------------
	// Event dispatching
	// --------------------------------------------------------------------
	bool DispatchEvent(
		IRoot * caller,
		const char* context,
		const char* eventName,
		PyObject** pRetval,
		const char* format,
		va_list vargs,
		bool post );

	bool SendEvent(
		IRoot * caller,
		const char* context,
		const char* eventName,
		PyObject** pRetval = NULL,
		const char* format = NULL,
		... ) override;

	bool PostEvent(
		IRoot * caller,
		const char* context,
		const char* eventName,
		const char* format = NULL,
		... ) override;
};

TYPEDEF_BLUECLASS_WR( BluePyOS ); //need weakref support for the singleton factory

// For testing crashdumps
static void CrashHorribly( bool reallyCrash )
{
	if( !reallyCrash )
	{
		return;
	}

	BeOS->SetError( BEFLUSH, 0, "" );
	CcpCrashOnPurpose();
}
MAP_FUNCTION_AND_WRAP(
	"CrashHorribly",
	CrashHorribly,
	"Crashes Blue. Intended for testing crashdumps etc.\n"
	":param reallyCrash: Pass in True if you really want to crash" );

namespace
{

class PureVirtualCallHelperBase
{
public:
	PureVirtualCallHelperBase()
	{
		helper();
	}

protected:
	virtual void virtualFunc() = 0;
	void helper()
	{
		this->virtualFunc();
	}
};

class PureVirtualCallHelper : public PureVirtualCallHelperBase
{
protected:
	virtual void virtualFunc()
	{
	}
};
}

// For testing crashdumps
static void PureVirtualCall()
{
	CCP_LOGERR( "About to make a pure virtual call." );

	PureVirtualCallHelper pureVirtualCallHelper;
}
MAP_FUNCTION_AND_WRAP( "PureVirtualCall", PureVirtualCall, "Induces a C++ pure virtual call that is supposed to crash the process." );

extern "C" BLUEIMPORT PyObject* BlueLoadPythonExtension( const char* name );

PyObject* LoadPythonExtension( PyObject*, PyObject* args );
MAP_FUNCTION(
	"LoadExtension",
	LoadPythonExtension,
	"Loads a CCP python extension with the current Blue build flavor. Throws an ImportError if the extension can't be loaded.\n"
	":param name: The extension to load\n"
	":returns: A PyModule object" );


// Callbacks for python to call when it starts and stops GC
extern "C"
{
	void* PyOS_GcStart( void );
	void PyOS_GcStop( void* arg );
}


#endif // _BLUEPYTHON_H_
