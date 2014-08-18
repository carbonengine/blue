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

#include "include/IBluePython.h"
#include "include/IBlueOS.h"
#include "TaskletTimer.h"
#include "PyScheduler.h"
#include "PythonEvents.h"

#ifdef STACKLESS
#include <stackless_api.h>
#endif

#ifdef _WIN32
#include <crypto.h>
#endif

#if _MSC_VER
#include <hash_map>
#endif
#include <vector>
#include <map>


// Forward declaration
PyObject* PySetLogEchoFunction( PyObject* self, PyObject* args );
PyObject* PyGetLogEchoFunction( PyObject* self, PyObject* args );

//////////////////////////////////////////////////////////////////////
//
// BluePyOS class
//
//////////////////////////////////////////////////////////////////////
BLUE_DECLARE( BluePyOS );

BLUE_CLASS( BluePyOS ) : 
	public IBluePyOS,
	public IPythonEvents
{
public:
	// ctor
	BluePyOS(IRoot* lockobj = NULL);

	// synchro
#if CCP_STACKLESS
	class Synchro* mSynchro;
	PyObject* mPySynchro;
#endif

#ifdef _WIN32
	// spying 
	struct SpyEntry {
		SpyEntry(PyObject *dir, PyObject *notify = 0) :
			mDir(BluePy(dir, true)), mNotify(notify, true) {}
		bool operator == (const SpyEntry &o) const {return PyObject_Compare(mDir, o.mDir) == 0;}

		BluePy mDir;
		BluePy mNotify;
	};
	std::vector<SpyEntry> mSpyDirs;
	std::vector<HANDLE> mSpyHandles;
	
	// Process info header
	PyObject* mProcessInfoHeader;
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

	void ProcessLibDirectives( const directives_t& directives, std::vector<std::wstring>& zips );
	bool VerifyManifestAndGatherDirectives( directives_t& directives );
	void ShowMessageBoxForVerificationFailure( int type, const std::wstring& errmsg );
#endif
	
	// init funcs and corresponding fini functions
	bool InitBasicModuleSupport();
	bool FiniBasicModuleSupport();
	
	bool InitIncludePaths(std::wstring &path);

	void BuildConcatenatedPathFromPathlist( const std::vector<std::wstring>& pathlist, std::wstring& path );

	void ProcessSpyHandles();
	void LogCpuUsageAndOtherStats();


	PyObject* mExceptionHandler;
	
	//The extra list of python hooks for context switch
	PyObject *mContextHooks;
	
	PyObject* CreateTaskletImpl(
		PyObject* meth, 
		PyObject* args, 
		PyObject* kw,
		PyObject* ctx
		);

	PyObject* CallPyObjectWithTrap(
		PyObject* meth, 
		PyObject* args = 0, 
		PyObject* ctx = 0
		);


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
	PyObject* mCpuUsage;
	
	// This flag is set based on a command line argument (/telemetryMarkup).
	// This is used in bluepy.py to determine whether decorators  and metaclass
	// for markup do anything. The reason this variable lives here is to ensure
	// that it can be read from the command line before any Python code is loaded.
	bool mMarkupZonesInPython;

	long mSliceWarning;
	long mBeNiceSlice; //default benice in milliseconds
    long mPerformanceUpdateFrequency; // How often PumpPython updates process performance data

	// for python events
	CPythonEvents mPyPorts[_PYPORTLAST+1];

	// exit procs
	PyObject *mExitProcs; //a python list
	
#if CCP_STACKLESS
	CTaskletTimer mTTimer; //the new tasklet timing object
#endif

private:
	double GetTimeSinceSwitch(bool update = false); //time since last tasklet switch
	bool UpdateTaskletRunTime(PyObject *tasklet, double elapsed);

private:
	PyObject	*mBlueModule;  //weak ref to us as a module;
	BluePy mTaskletExt;		//the tasklet extension class
	bool mInit;	
	int mSoftspace;			//for python's print statement
	bool mPackaged;
#if CCP_STACKLESS
	PyScheduler mScheduler; //to run the watchdog

	//The time when the last tasklet switch occurred
	LARGE_INTEGER mLastSwitchTime;
	double mSwitchTimePeriod; //inverse of frequency
	BluePy mstoredContext_str; //attribute name strings
	BluePy mrunTime_str;
#endif

public:

	void OnTaskletSwitch(PyObject *from, PyObject *to);

	bool RecurseFolder(
		PyObject* result, 
		const char* directory, 
		Py_ssize_t prefixlen,
		const char* filter
		);

private:
	void HandleException(const char *message);

public:
	EXPOSE_TO_BLUE();

	PyObject* PyAddExitProc( PyObject* args );
	PyObject* PyGetArg( PyObject* args );
	PyObject* PyGetEnv( PyObject* args );
	PyObject* PyDumpState( PyObject* args );
	PyObject* Py_DumpLockCount( PyObject* args );
	PyObject* Py_EnableTrace( PyObject* args );
	PyObject* Py_GetWrapperList( PyObject* args );
	PyObject* Py_GetObjectState( PyObject* args );
	PyObject* Pywrite( PyObject* args );
	PyObject* PyGetThunkers( PyObject* args );
	PyObject* PyCreateTasklet( PyObject* args );
	PyObject* PySpyDirectory( PyObject* args );
	PyObject* PyNextScheduledEvent( PyObject* args );
	PyObject* PyGetClipboardData( PyObject* args );
	PyObject* PySetClipboardData( PyObject* args );
	PyObject* PyGetThreadTimes( PyObject* args );
	PyObject* PyProbeStuff( PyObject* args );
	PyObject* PyGetTimeSinceSwitch( PyObject* args );
	PyObject* PyBeNice( PyObject* args );
	PyObject* PyXUtil_Index( PyObject* args );
	PyObject* PyXUtil_Filter( PyObject* args );
	PyObject* PyXUtil_SwapLists( PyObject* args );
	PyObject* PyGetMaxRunTime( PyObject* args );
	PyObject* PySetMaxRunTime( PyObject* args );
	

	//--------------------------------------------------------------------
	// IBluePyOS interface
	//--------------------------------------------------------------------
	// Magic blue to python marriage
	//--------------------------------------------------------------------
	
	// returns a pyobject representation of 'object'
	BluePythonObject* WrapBlueObject(
		IRoot* object
		);

	const PyMethodDef* GetGenericThunker(
		const char* name, 
		const Be::ClassInfo* type
		);


	//--------------------------------------------------------------------
	// Python engine
	//--------------------------------------------------------------------

	// the startup.
	// will pump python automatically in the context
	// of the callers thread
	bool Startup(
		);	

	// the shutdown
	void Shutdown(
		int level
		);

	// the pumping
	int PumpPython(
		bool quit
		);

	void SetEventHandler(
		IPythonEvents* handler
		);

	
	//--------------------------------------------------------------------
	// Convenience functions
	//--------------------------------------------------------------------
	
	PyObject* PyError(
		PyObject* exception = NULL
		);

	bool PyFlushError(
		const char *whence
		);

	PyObject *PyErr_BlueError();

	void RebaseSimClock(Be::Time oldTime, Be::Time newTime);

	//Turn a python exception into a string
	void FormatException(char **result);

	bool IsPackaged() {return mPackaged;}

	bool CanYield();
	bool Yield();

	void GetSchedulerStats(
		int &inQueue1,
		int &inQueue2,
		float &lastTime,
		float &maxTime
		);

	void DoStackTrace(
		PyObject* frame =0
		);

	PyObject *GetStackTrace(
		PyObject* frame =0
		);

	ITaskletTimer *GetTaskletTimer();  //Get the tasklet timer object

	//blocktrapping call methods
	PyObject *CallMethodWithTrap(PyObject *target, const char *method, const char *ctxt, const char *format, ...);

	bool PythonEvent(const char *event, PyObject * arg);

public:
	PyObject* CreateTasklet(
		PyObject* meth, 
		PyObject* args, 
		PyObject* kw
		);

    // --------------------------------------------------------------------
	// Event dispatching
    // --------------------------------------------------------------------
	bool DispatchEvent(
		IRoot* caller,
		const char* context,
		const char* eventName,
		PyObject** pRetval,
		const char* format,
		va_list vargs,
		bool post
		);

	bool SendEvent(
		IRoot* caller,
		const char* context,
		const char* eventName,
		PyObject** pRetval = NULL,
		const char* format = NULL,
		...
		);

	bool PostEvent(
		IRoot* caller,
		const char* context,
		const char* eventName,
		const char* format = NULL,
		...
		);

	//--------------------------------------------------------------------
	// IPythonEvents interface
	//--------------------------------------------------------------------
	void OnWrite(
		PYPORT port,
		const char* text
		);	
};

TYPEDEF_BLUECLASS_WR(BluePyOS); //need weakref support for the singleton factory

// For testing crashdumps
static PyObject* PyCrashHorribly( PyObject* module, PyObject* args )
{

	bool reallyCrash = false;
	if( !PyArg_ParseTuple( args, "b", &reallyCrash ) )
	{
		return NULL;
	}
	else if( !reallyCrash )
	{
		Py_RETURN_NONE;
	}

	CCP_LOGERR( "About to throw an exception that will kill Blue." );
	BeOS->SetError( BEFLUSH, 0, "" );

	volatile int i = 0;

	return PyLong_FromLong( 1/i );
}
MAP_FUNCTION( "CrashHorribly", PyCrashHorribly, "CrashHorribly( bool reallyCrash )\nCrashes Blue. Pass in True if you really want to crash.\n Intended for testing crashdumps etc." );


// Callbacks for python to call when it starts and stops GC
extern "C" {
	void * PyOS_GcStart(void);
	void PyOS_GcStop(void* arg);
}

#endif // _BLUEPYTHON_H_
