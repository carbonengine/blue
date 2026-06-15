// Copyright © 2014 CCP ehf.

#include <sstream>
#include <string>
#include "StdAfx.h"

#if BLUE_WITH_PYTHON

#include "BluePython.h"

#include <BlueStatistics.h>

#include "IBlueOS.h"
#include "IMotherLode.h"
#include "IBlueResMan.h"
#include "IBlueObjectRecycler.h"
#include "IBluePaths.h"
#include <Scheduler.h>

#include "blueloginmemory.h"
#include "LogToPython.h"
#include "PrettyPrint.h"
#include "BlueHeapq.h"
#include "Marshal.h"
#include "PyRowset.h"
#include "crypto.h"
#include "BlueClipboard.h"
#include "errormessage.h"
#include "PyMemory.h"

#if _WIN32
#include "win32.h"
#if CCP_STACKLESS
#include "Logger/Logger.h"
#endif
#endif

#if CCP_STACKLESS
#include "Synchro.h"
#endif

#if CCP_STACKLESS
#include "BlueNet.h"
#endif

#include "ScopedBlockTrap.h"

IBluePyOS* PyOS = nullptr;

static CcpLogChannel_t s_chPy = CCP_LOG_DEFINE_CHANNEL( "Python Logs" );
static CcpLogChannel_t s_chMemory = CCP_LOG_DEFINE_CHANNEL( "Memory" );

static const char* PORTNAMES[] =
{
	"stdout",
	"stderr",
	"loginfo",
	"logwarn",
	"logerr",
	"logfatal"
};


struct TimerString
{
	const char* mName;
	PyObject* mContext;
};

static TimerString TIMERS[] =
{
	{"PyOS::Synchro Tick", NULL},
	{"PyOS::PyError", NULL},
	{"PyOS::Create Tasklet", NULL},
	{"PyOS::Run Watchdog", NULL},
	{"PyOS::StacklessIoDispatch", NULL},
};

// Timer strings
enum
{
	TIMER_TICK,
	TIMER_PYERROR,
	TIMER_CREATETASKLET,
	TIMER_RUNWATCHDOG,
	TIMER_STACKLESSIO,
};


struct TaskletSwitch
{
	const char* mName;
	PyObject* mContext;
};

static TaskletSwitch TASKLETS[] =
{
	{"Other tasklets", NULL},
	{"Main tasklet", NULL},
	{"Zombie tasklet", NULL},
};

static const int OTHERTASKLETS = 0;
static const int MAINTASKLET = 1;
static const int VOODOOTASKLET = 2;

CCP_STATS_DECLARE( runnablesLeftOver,	"Blue/Synchro/runnablesLeftOver", false, CST_COUNTER_LOW, "Tasklets that were runnable but not scheduled" );

//use this because the TASKLETS thing may be cleared at the end.
class SafeAutoTasklet : public AutoTasklet
{
public:
	SafeAutoTasklet(ITaskletTimer *timer, PyObject *context, bool active = true) :
		AutoTasklet(timer, context, active && context != 0)
	{}
};

//////////////////////////////////////////////////////////////////////
//
// BluePyOS class
//
//////////////////////////////////////////////////////////////////////


#if CCP_STACKLESS
// Tasklet performance counting
static int OnTaskletSwitch(PyTaskletObject *from, PyTaskletObject *to)
{
	BlueStatistics::OnTaskletSwitch( (PyObject*)from, (PyObject*)to );

	PyOS->OnTaskletSwitch( (PyObject*)from, (PyObject*)to );
	return 0;
}
#endif


//--------------------------------------------------------------------
BluePyOS::BluePyOS(IRoot* lockobj) :
	mSynchro( nullptr ),
	mPySynchro( nullptr ),
	mThreads( "BluePyOS/mThreads" ),
	mScheduler(0.25), //initialize to min 4 fps
	mMarkupZonesInPython(false),
	mExceptionHandler( nullptr ),
	PARENTLOCK( mCpuUsage ),
	mSliceWarning( 200 ),
	mBeNiceSlice( 15 ),
	mPerformanceUpdateFrequency( 100000000 )
{
	mBlueModule = 0;
	mExitProcs = 0;

	mContextHooks = 0;

	mInit = 0;
	mSoftspace = 0;
	mPackaged = false;
	mInterpreterMode = false;

	//default value for optimize
	mOptimizeFlag = -1;

	//Context hooks:
	mContextHooks = PyList_New(0);
	//attribute strings
	mstoredContext_str = BluePy(PyUnicode_InternFromString("storedContext"));
	mrunTime_str = BluePy(PyUnicode_InternFromString("runTime"));
}


PyTypeObject *LogChannelType();

// This is a little Python helper script to allow loading flavoured Python C extensions through a simple `import extension_without_flavor`.
// It kind of obsoletes the old `blue.LoadExtension` helper method.
const char* IMPORT_HOOK_HELPER_SCRIPT = R"(
import importlib.machinery
import os
import sys


class BlueFlavoredExtensionFileLoader(importlib.machinery.ExtensionFileLoader):
    def __init__(self, name, path, flavor):
        super().__init__(name, path)
        self.flavor = flavor

    def create_module(self, spec):
        import sys
        unflavored_name = spec.name
        spec.name = f"{spec.name}{self.flavor}"
        module = super().create_module(spec)
        sys.modules[unflavored_name] = module
        sys.modules[spec.name] = module

        return module


class BlueFlavoredExtensionImporter:
    def __init__(self, flavor, binary_path, loader_class):
        import _imp
        import os
        self.flavor = flavor
        self.loader_class = loader_class
        self.bin_path = os.path.dirname(binary_path) if os.path.isfile(binary_path) else binary_path
        self.extension_suffixes = [f"{self.flavor}{suffix}" for suffix in _imp.extension_suffixes()]

    def find_spec(self, fullname, path=None, target=None):
        import importlib
        import os
        import sys
        for suffix in self.extension_suffixes:
            search_paths = [os.path.join(search_path, f"{fullname}{suffix}") for search_path in sys.path + [self.bin_path]]

            found = False

            for search_path in search_paths:
                if os.path.exists(search_path):
                    found = True
                    origin = search_path

            if found:
                break
        else:
            return None  # no flavored version was found, return None

        return importlib.machinery.ModuleSpec(fullname, self.loader_class(fullname, origin, self.flavor), origin=origin)

sys.meta_path.insert(0, BlueFlavoredExtensionImporter(CCP_BUILD_FLAVOR, CCP_EXEFILE_PATH, BlueFlavoredExtensionFileLoader))
)";

bool InstallImportHook()
{
	// Extend the Pyhon import mechanism with support for C extensions that use our "build flavors".
	// This needs to happen before we import any flavored C extensions.
#if ~(~CCP_BUILD_FLAVOR + 0) == 0 && ~(~CCP_BUILD_FLAVOR + 1) == 1
	// this is not a flavoured build, so no import hook is needed
	return true;
#else
	PyObject* globalDict = PyDict_New();
	if ( !globalDict ) {
		CCP_LOGERR_CH( s_chPy, "Failed to create global dict for import helper script execution");
		return false;
	}
	auto executablePath = WideToUTF8( CcpExecutablePath() );
	PyObject* localDict = Py_BuildValue( "{ssss}", "CCP_BUILD_FLAVOR", CCP_STRINGIZE( CCP_BUILD_FLAVOR ), "CCP_EXEFILE_PATH", executablePath.c_str() );
	if ( !localDict ) {
		CCP_LOGERR_CH( s_chPy, "Failed to create local dict for import helper script execution");
		Py_DecRef( globalDict );
		return false;
	}
	if ( ! PyRun_String( IMPORT_HOOK_HELPER_SCRIPT, Py_file_input, globalDict, localDict ) )
	{
		CCP_LOGERR_CH( s_chPy, "Failed executing import helper script" );
		Py_DecRef( localDict );
		Py_DecRef( globalDict );
		return false;
	}
	Py_DecRef( localDict );
	Py_DecRef( globalDict );

	return true;
#endif
}

//--------------------------------------------------------------------
bool BluePyOS::InitBasicModuleSupport()
{
	// put myself into python as a module
	static struct PyModuleDef moduleDef {
		PyModuleDef_HEAD_INIT,
		"blue",
		"",
		-1,
		nullptr,
	};
	mBlueModule = PyModule_Create( &moduleDef );
	PyObject* dict = PyModule_GetDict(mBlueModule); //borrowed ref

	BlueRegisterToModule( mBlueModule, BlueRegistration::GetClassRegs(),
									   BlueRegistration::GetFuncRegs(),
									   BlueRegistration::GetEnumRegs(),
									   BlueRegistration::GetTestRegs(),
									   BlueRegistration::GetThunkerRegs(),
									   BlueRegistration::GetFuncSignatures() );

	BlueRegisterObjectsToModule( mBlueModule, BlueRegistration::GetObjectRegs() );
	BlueRegisterExceptionsToModule( mBlueModule, BlueRegistration::GetExceptionRegs() );

	// Get ref to schedule manager for main thread
	// This will ensure the main thread's schedule manager alive throughout game usage
	mMainScheduler = BluePy( SchedulerAPI()->PyScheduler_GetScheduler() );

	// Set the main tasklet as block trapped.
	BluePy currentTasklet(SchedulerAPI()->PyScheduler_GetCurrent());
	if (!currentTasklet)
		return false;
	SchedulerAPI()->PyTasklet_SetBlockTrap((PyTaskletObject*)(PyObject*)currentTasklet, 1);

		// Initialize error exception
    PyDict_SetItemString(dict, "error", PyExc_BlueError);

	PyModule_AddObject( mBlueModule, "logInMemory", BlueWrapObjectForPython( BlueLogInMemory::GetInstance() ));

	// Insert PyOS as python object
	if (PyModule_AddObject(mBlueModule, "pyos", BlueWrapObjectForPython(PyOS)))
		return false;

	// Add the blue wrapper
	if (PyModule_AddObject(mBlueModule, "BlueWrapper", (PyObject*)BePyTypePtr))
		return false;

	// Add the logchannel object
	if (PyDict_SetItemString(dict, "LogChannel", (PyObject*)LogChannelType()))
		return false;

    PyObject* heapqModule = InitHeapq();
	if ( !heapqModule )
		throw PyError();
    if ( PyModule_AddObject( mBlueModule, "heapq", heapqModule ) ) {
        return false;
    }

    // Add the DBRowsetStuff
    if( !DBRowsetInit( mBlueModule ) )
        return false;

#if _WIN32
	//init the submodules blue.win32 and blue.heapq.  The latter is required for the Marshal::New()
	if ( !initwin32(mBlueModule) )
		return false;
#endif

	if( !InitCryptoModule(mBlueModule) )
		return false;

	if (!MarshalInit(mBlueModule))
		return false;

	// Insert custom marshaller
	PyObject* marshal = Marshal::New();
	if( marshal )
	{
		if( PyModule_AddObject( mBlueModule, "marshal", marshal ) )
			return false;
	}

	PyObject* sys_modules = PyImport_GetModuleDict();

#if CCP_STACKLESS
	auto carbonSocketModule = PyImport_ImportModule( "_carbonsocket" );
	if( !carbonSocketModule )
	{
		CCP_LOGERR_CH( s_chPy, "Failed to import `_carbonsocket` module" );
		return false;
	}

	if( PyDict_SetItemString( sys_modules, "_socket", carbonSocketModule ) != 0 )
	{
		CCP_LOGERR_CH( s_chPy, "Failed to patch `_socket` module" );
		return false;
	}

	// Reload socket module with patched _socket module in order for the module
	// to pick up on our _socket implementation. This is necessary if the socket module
	// has already been imported.
	if( auto socketModule = PyDict_GetItemString( sys_modules, "socket" ) )
	{
		PyImport_ReloadModule( socketModule );
	}

	auto carbonSSLModule = PyImport_ImportModule( "_carbonssl" );
	if( !carbonSSLModule )
	{
		CCP_LOGERR_CH( s_chPy, "Failed to import `_carbonssl` module" );
		return false;
	}

	if( PyDict_SetItemString( sys_modules, "_ssl", carbonSSLModule ) != 0 )
	{
		CCP_LOGERR_CH( s_chPy, "Failed to patch `_ssl` module" );
		return false;
	}

	auto carbonSelectModule = PyImport_ImportModule( "carbonselect" );
	if( !carbonSelectModule )
	{
		CCP_LOGERR_CH( s_chPy, "Failed to import `carbonselect` module" );
		return false;
	}

	if( PyDict_SetItemString( sys_modules, "select", carbonSelectModule ) != 0 )
	{
		CCP_LOGERR_CH( s_chPy, "Failed to patch select module" );
		return false;
	}

	// c-routing support
	if ( !BeNet->Init( mBlueModule, mSocketAPI ) ) {
		return false;
	}

	mSynchro = CCP_NEW( "BluePyOS/mSyncro" ) Synchro;
	mPySynchro = mSynchro;
	PyDict_SetItemString(dict, "synchro", mSynchro);
#endif

    if ( PyDict_SetItemString( sys_modules, "blue.heapq", heapqModule ) != 0 ) {
        return false;
    }

	return true;
}


//--------------------------------------------------------------------
bool BluePyOS::FiniBasicModuleSupport()
{
#if CCP_STACKLESS

	//synchro and pysynchro share a reference
	//Todo, this should go somewhere else
	mPySynchro = 0;
	Synchro *synchro = mSynchro;
	mSynchro = 0;
	synchro->Shutdown();
	Py_DECREF(synchro);

#endif


	//Get the Blue module dict
	PyObject* dict = PyModule_GetDict(mBlueModule); //borrowed reference

	// clear the module dict
	PyDict_Clear(dict);

	Py_DECREF(PyExc_BlueError);
	PyExc_BlueError = 0;

	//now, get the sys.modules dict
	dict = PyImport_GetModuleDict();
	PyObject *modules = PyDict_GetItemString(dict, "modules");

	//and delete the "blue" module (by replacing it with None)
	PyDict_SetItemString(modules, g_moduleName, Py_None);

	mBlueModule = 0;
	return true;
}

//--------------------------------------------------------------------
BluePythonObject* BluePyOS::WrapBlueObject(
	IRoot* object
	)
{
	BluePythonObject* ret = BlueWrapObjectForPython(object);
	return ret;
}


//--------------------------------------------------------------------

#if CCP_STACKLESS

//--------------------------------------------------------------------
static PyObject* ClockThisImpl(PyObject* ctx, PyObject* fn, PyObject* args, PyObject* kw)
{
	//Be a bit careful here, to pass on any error generatoed by the actual call.
	PyObject *result, *etype, *evalue, *etb;
	{
		AutoTasklet _at(PyOS->GetTaskletTimer(), ctx);
		result = PyObject_Call(fn, args, kw);
		PyErr_Fetch(&etype, &evalue, &etb);
	}
	PyErr_Restore(etype, evalue, etb);
	return result;
}


//--------------------------------------------------------------------
static PyObject* ClockThis(PyObject*, PyObject* args, PyObject* kw)
{
	// def ClockThis(ctx, fn, *args, **kw)

	if (PyTuple_GET_SIZE(args) >= 2)
    {
        PyObject* ctx = PyTuple_GET_ITEM(args, 0);
        PyObject* fn = PyTuple_GET_ITEM(args, 1);
        if( PyUnicode_Check(ctx) && PyCallable_Check(fn) )
        {
            PyObject* realArgs = PyTuple_GetSlice(args, 2, PyTuple_GET_SIZE(args));
            PyObject* ret = ClockThisImpl(ctx, fn, realArgs, kw);
            Py_DECREF(realArgs);
            return ret;
        }
    }
	PyErr_SetString(PyExc_TypeError, "I want string/unicode as first argument and callable as second");
	return 0;
}

//--------------------------------------------------------------------
static PyObject* ClockThisWithoutStars(PyObject*, PyObject* args)
{
	// def ClockThisWithoutTheStars(ctx, fn, args, kw={})

	PyObject* ctx;
	PyObject* fn;
	PyObject* realArgs;
	PyObject* kw = NULL;
	if (!PyArg_ParseTuple(args, "OOO!|O!", &ctx, &fn, &PyTuple_Type, &realArgs, &PyDict_Type, &kw))
		return NULL;

	if (!PyUnicode_Check(ctx))
		return PyErr_SetString(PyExc_TypeError, "I want a string context as first argument"), nullptr;

	if (!PyCallable_Check(fn))
	{
		PyErr_SetString(PyExc_TypeError, "I want a callable as second argument");
		return NULL;
	}

	return ClockThisImpl(ctx, fn, realArgs, kw);
}

CCP_STATS_DECLARED_ELSEWHERE( pyMemory );
static PyObject* PyGetPyMalloced(PyObject*, PyObject*)
{
	return PyLong_FromLongLong( int64_t( CCP_STATS_GET( pyMemory ) ) );
}


//--------------------------------------------------------------------
// ClockThis utility functions
static PyMethodDef clockthis[] = {
	{"ClockThis", (PyCFunction)ClockThis, METH_VARARGS | METH_KEYWORDS, "ClockThis utility"},
	{"ClockThisWithoutTheStars", ClockThisWithoutStars, METH_VARARGS, "ClockThisWithoutTheStars utility"},
	// `getpymalloced` is not related to clockthis at all, but you know, it's also a customization to `sys`
	{"getpymalloced", PyGetPyMalloced, METH_VARARGS, "Returns the size of memory allocated by Python, in bytes."},
	{nullptr}
};

#endif


void BluePyOS::EnsureAssertionsEnabled()
{
#ifdef _MSC_VER
	//Python turns off assertions for the C runtime!  Undo that
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_WNDW);
#endif
}

bool BluePyOS::Startup()
{
	if (mInit)
		return true;

	BeTimer timer("BluePyOS::Startup()");

	//simple tasklet timing
	mLastSwitchTime = CcpGetTimestamp();
	mSwitchTimePeriod = 1.0/double( CcpGetTimestampFrequency() );

	mLastThreadCpuUsage = mLastProcessCpuUsage = 0;
	mLastThreadKernelUsage = mLastProcessKernelUsage = 0;
	mLastCpuUpdate = 0;

	mInterpreterMode = BeOS->HasStartupArg( L"py" );

	this->EnsureAssertionsEnabled();

	for (unsigned int i = 0; i < sizeof TIMERS / sizeof *TIMERS; i++)
		TIMERS[i].mContext = PyUnicode_InternFromString(TIMERS[i].mName);

	// insert clockthis utility functions
	for (PyMethodDef* md = clockthis; md->ml_meth != NULL; md++)
	{
		PyObject* fn = PyCFunction_New(md, NULL);
		if ( PySys_SetObject((char*)md->ml_name, fn) == -1 ) {
			CCP_LOGERR( "Failed adding %s to sys module", md->ml_name );
		}
		Py_DECREF(fn);
	}

	for (unsigned int i = 0; i < sizeof TASKLETS / sizeof TASKLETS[0]; i++)
	{
		TASKLETS[i].mContext = PyUnicode_InternFromString(TASKLETS[i].mName);
	}

	if ( !InstallImportHook() )
	{
		return false;
	}

	mSocketAPI = reinterpret_cast<PySocketModule_APIObject*>( PySocketModule_ImportModuleAndAPI() );
	if ( !mSocketAPI )
	{
		CCP_LOGERR( "Failed acquiring carbon-io socket module" );
		PyFlushError( nullptr );
		return false;
	}

	CCP_LOG( "Init basic module support" );
	if (!InitBasicModuleSupport())
	{
		CCP_LOGERR( "InitBasicModuleSupport() failed" );
		PyFlushError(0);
		Py_Finalize();
		return false;
	}

    if ( !mTTimer.InitPythonObjects() ) {
        CCP_LOGERR( "Failed initalizing TaskletTimers" );
        PyFlushError( nullptr );
        return false;
    }
    // Reset the tasklet counter (can only do this after pyinit and module support, we register modules and types)
	mTTimer.Reset();
	SchedulerAPI()->PyScheduler_SetScheduleFastCallback( ::OnTaskletSwitch );

	mInit = true;
	return true;
}


//--------------------------------------------------------------------
void BluePyOS::Shutdown(int level)
{
	// If Telemetry was running we have to stop it before Python goes
	// down as it assumes tasklets are active.
	g_statistics->StopTelemetry();
	g_statistics->Update();

	if (!mInit)
		return ;

	if (level == 1) {
		//shutdown, level 1.  Benign exit, call exit handlers, kill tasklets, etc.

		// Make sure we release a potential reflock to an external Python object:
		SetLogEchoFunction( CCP::LOGTYPE_LOWEST, Py_None );

		// call exit procs
		BluePy bluepy(PyImport_ImportModule("bluepycore"));
		if (bluepy) {
			CCP_LOG_CH( s_chPy, "Running bluepycore.Shutdown");
			BluePy res(BluePy(PyObject_CallMethod(bluepy, const_cast<char*>( "Shutdown" ), const_cast<char*>( "O" ), mExitProcs?mExitProcs:Py_None)));
			Py_CLEAR(mExitProcs);
			if (!res)
				HandleException("BluePyOS::Shutdown::bluepycore.Shutdown");
			CCP_LOG_CH( s_chPy, "Finished running bluepycore.Shutdown");
		}
		PyErr_Clear();
		return;
	}

	//Delete all deco objects.  They hold tons of references.  Shouldn't be necessary,
	//but it's prophylactic.
	// todo: BlueWrapper::ReleaseAllDecos();

	// Detach app exception handler
	Py_XDECREF(mExceptionHandler);
	mExceptionHandler = NULL;

	// Finish basic module support
	FiniBasicModuleSupport();

#if CCP_STACKLESS
	SchedulerAPI()->PyScheduler_SetScheduleFastCallback( 0 );
	Py_XDECREF(mContextHooks);
	mrunTime_str.Release();
	mstoredContext_str.Release();

	ssize_t i;

	for (i = 0; i < sizeof TASKLETS / sizeof TASKLETS[0]; i++) {
		Py_DECREF(TASKLETS[i].mContext);
		TASKLETS[i].mContext = 0;
	}

	// remove clockthis utility functions
	for (PyMethodDef* md = clockthis; md->ml_meth != NULL; md++)
		PySys_SetObject((char*)md->ml_name, 0);

	// Release timer names
	for (i = 0; i < sizeof TIMERS / sizeof *TIMERS; i++) {
		Py_DECREF(TIMERS[i].mContext);
		TIMERS[i].mContext = 0;
	}

#endif

	// Shut down python now!
	Py_Finalize();

	// Now, all python references to the blue wrappers should be gone.
	// todo: BlueWrapper::Shutdown();

	mInit = false;
}

#if CCP_STACKLESS

//Return a borrowed reference to the stored context, or the creation context.
static PyObject *GetContext(PyTaskletObject *task)
{
	if (!task)
		return TASKLETS[VOODOOTASKLET].mContext;
	if(SchedulerAPI()->PyTasklet_IsMain(task))
		return TASKLETS[MAINTASKLET].mContext;
	PyObject *r = PyObject_GetAttrString((PyObject*)task, "storedContext");
	Py_XDECREF(r);
	if (!r || r == Py_None) {
		PyObject *rr = PyObject_GetAttrString((PyObject*)task, "context");
		Py_XDECREF(rr);
		if (rr)
			r = rr;
		else
			PyErr_Clear();
	}
	if (r)
		return r;

	PyErr_Clear();
	return TASKLETS[OTHERTASKLETS].mContext;
}

#endif


//--------------------------------------------------------------------
void BluePyOS::OnTaskletSwitch(PyObject* _from, PyObject* _to)
{
#if CCP_STACKLESS
	//new api.  NULL -> non-zero: entering main tasklet
	//          non-null -> non-zero: switching between tasklets, possibly on tasklet exit,
	//                                possibly on tasklet entry.
	CCP_ASSERT((!_from && _to) || (_from && _to));

	PyTaskletObject *from = (PyTaskletObject*)_from, *to = (PyTaskletObject*)_to;

	//time for simple tasklet timing;
	double elapsed = GetTimeSinceSwitch(true);

	//We need to preserve any error state:
	PyObject *etype, *evalue, *etraceback;
	PyErr_Fetch(&etype, &evalue, &etraceback);

	PyObject *prev = mTTimer.SwitchStack((intptr_t)to);

	//Store the context we came from in our tasklet extension object.
	bool fromNotMain = from && !SchedulerAPI()->PyTasklet_IsMain(from);
	if (fromNotMain && PyObject_SetAttr((PyObject*)from, mstoredContext_str, prev) < 0)
		PyErr_Clear();
	Py_DECREF(prev);

	//update tasklet run time
	if (fromNotMain) {
		if (!UpdateTaskletRunTime((PyObject*)from, elapsed))
			PyErr_Clear();
	}

	//Call the other context hooks
	if (!from)
		from = (PyTaskletObject*)Py_None;
	if (!to)
		to = (PyTaskletObject*)Py_None;
	Py_ssize_t n = PyList_GET_SIZE(mContextHooks);
	for (Py_ssize_t i=0; i<n; i++) {
		PyObject *hook = PyList_GET_ITEM(mContextHooks, i);
		PyObject *result = PyObject_CallFunction(hook, (char*)"OO", from, to);
		Py_XDECREF(result);
		if (!result)
			PyError();
	}

	//restore previous error state
	PyErr_Restore(etype, evalue, etraceback);
#endif
}


#if CCP_STACKLESS
//--------------------------------------------------------------------
double BluePyOS::GetTimeSinceSwitch(bool update)
{
	auto now = CcpGetTimestamp();
	double elapsed = (double(now)-double(mLastSwitchTime)) * mSwitchTimePeriod;
	if (update)
		mLastSwitchTime = now;
	return elapsed;
}


//--------------------------------------------------------------------
bool BluePyOS::UpdateTaskletRunTime(PyObject *tasklet, double elapsed)
{
	BluePy oldObj(PyObject_GetAttr((PyObject*)tasklet, mrunTime_str));
	if (!oldObj)
		return false; //it has to be there already.
	double oldVal = PyFloat_AS_DOUBLE(oldObj.o); //ignore errors here.
	BluePy newObj(PyFloat_FromDouble(oldVal + elapsed));
	if (!newObj)
		return false;
	if (PyObject_SetAttr((PyObject*)tasklet, mrunTime_str, newObj) < 0)
		return false;
	return true;
}
#endif


//--------------------------------------------------------------------
int BluePyOS::PumpPython(bool quit)
{
#if CCP_STACKLESS
	if (PyErr_Occurred())
		PyFlushError("PumpPython::start");

	{
		CCP_STATS_ZONE( "Blue/CarbonIO" );
		SafeAutoTasklet _at(&mTTimer, TIMERS[TIMER_STACKLESSIO].mContext);
		mSocketAPI->dispatch();
	}

	// Synchro.  This will make tasklets runnable
	{
		CCP_STATS_ZONE( "Blue/synchro" );

		SafeAutoTasklet _at(&mTTimer, TIMERS[TIMER_TICK].mContext);
		if (!quit && !mSynchro->Tick())
			PyFlushError("PumpPython::synchro");
	}

	// Run from the runnable queue
	{
		CCP_STATS_ZONE( "Blue/RunWatchdog" );

		SafeAutoTasklet _at(&mTTimer, TIMERS[TIMER_RUNWATCHDOG].mContext);
		if( !mScheduler.Run() )
		{
			PyFlushError("PumpPython::Scheduler");
		}

		CCP_STATS_SET( runnablesLeftOver, SchedulerAPI()->PyScheduler_GetRunCount() - 1 );
	}

	LogCpuUsageAndOtherStats();

#endif

	return 1;
}


//custom formatting of traceback for those early startup problems
static std::string FormatTraceback(PyObject *tb)
{
	std::string result = "Traceback (most recent call first):\n";
	BluePy t(tb, true);
	while (t && PyObject_IsTrue(t) ){
		std::string filename = "<none>";
		BluePy frame(PyObject_GetAttrString(t, "tb_frame"));
		if (frame) {
			BluePy code(PyObject_GetAttrString(frame, "f_code"));
			if (code) {
				BluePy fname(PyObject_GetAttrString(code, "co_filename"));
				if (fname)
					filename = std::string(PyUnicode_AsUTF8(fname));
			}
		}
		PyErr_Clear();
		int line = 0;
		BluePy lineno(PyObject_GetAttrString(t, "tb_lineno"));
		if (lineno)
			line = int( PyLong_AsLong(lineno) );
		PyErr_Clear();
		char clineno[16];
		sprintf_s(clineno, "%i\n", line);
		result += filename+":"+clineno;
		t = BluePy(PyObject_GetAttrString(t, "tb_next"));
	}
	result += "Traceback end\n";
	PyErr_Clear();
	return result;
}


//This we may need if we get an exception very early, when 'traceback' isn't avaliable.
static std::string FormatExceptionFallback(PyObject *type, PyObject *val, PyObject *tb)
{
	std::string result = "Simple exception format:\n";
	BluePy s(PyObject_Repr(type));
	if (s)
		result += std::string("Type: ") + PyUnicode_AsUTF8(s) + "\n";
	if (val) {
		s = BluePy(PyObject_Repr(val));
		if (s)
			result += std::string("Value: ") + PyUnicode_AsUTF8(s) + "\n";
	}
	if (tb)
		result += FormatTraceback(tb);
	result += "Simple exception end\n";
	PyErr_Clear();
	return result;
}


void BluePyOS::FormatException(char **result)
{
	*result = 0;
	if (!PyErr_Occurred()) {
		//there was no exception!
		*result = CCP_STRDUP( "FormatException", "FormatException() called with no pending python exception" );
		return;
	}
	PyObject *type, *val, *tb;
	PyErr_Fetch(&type, &val, &tb);
	{
		//import the traceback module
		BluePy module(PyImport_ImportModule("traceback"));
		if (!module) goto ERR;
		BluePy lines(PyObject_CallMethod(module, const_cast<char*>( "format_exception" ), const_cast<char*>( "OOO" ), type, val?val:Py_None, tb?tb:Py_None));
		if (!lines) goto ERR;
		BluePy str(PyUnicode_FromString(""));
		if (!str) goto ERR;
		str = BluePy(PyObject_CallMethod(str, const_cast<char*>( "join" ), const_cast<char*>( "O" ), lines.o));
		if (!str) goto ERR;
		*result = CCP_STRDUP( "FormatException", PyUnicode_AsUTF8(str) );
		PyErr_Restore(type, val, tb);
		return;
	}
ERR:
	// Silly failure.  Can happen if we have not initialized traceback module yet.
	PyErr_Clear();
	*result = CCP_STRDUP( "FormatException", FormatExceptionFallback(type, val, tb).c_str() );
	PyErr_Restore(type, val, tb);
	//also print it and clear it in this error case
	PyErr_Print();
	PyErr_Clear();
}


//--------------------------------------------------------------------
// BluePyOS::PyError.  This is used to turn python exceptions into blue errors
//--------------------------------------------------------------------
PyObject* BluePyOS::PyError(
	PyObject* exception
	)
{
	if (exception)
		return exception;  //this is not an exception!

	SafeAutoTasklet _at(GetTaskletTimer(), TIMERS[TIMER_PYERROR].mContext);
	if (!PyErr_Occurred()) {
		BeOS->SetError(BEDEF, Clsid(), "Trying to report a python exception with none pending.");
		return 0;
	}
	HandleException("BluePyOS::PyError()");
	return 0;
}


//--------------------------------------------------------------------
// BluePyOS::PyFlushError.  This is used to turn python exceptions into blue errors
// Similar to PyError, but accepts a "whence" message, and is okay for there to
// be no error.  Use this to periodically check for leaking errors, or to report
// known errors with a proper "whence" message.  Returns true if an error was found
//--------------------------------------------------------------------
bool BluePyOS::PyFlushError(const char *message)
{
	if (!PyErr_Occurred())
		return false;

	AutoTasklet _at(GetTaskletTimer(), "PyOS::PyFlushError");
	HandleException(message);
	return true;
}


// Actual handling of generating the exception
void BluePyOS::HandleException(const char *message)
{
	PyObject* type;
	PyObject* value;
	PyObject* traceback;
	PyErr_Fetch(&type, &value, &traceback);
	CCP_ASSERT(type);
	PyErr_NormalizeException(&type, &value, &traceback);

	if (!message)
		message = "";

	bool handled = false;
	bool failed = false;
	if (mExceptionHandler) {
		// Avoid recursion if exception handler is screwey
		PyObject* exhandler = mExceptionHandler;
		mExceptionHandler = NULL;
		// later we can add some info here, extra stuff, to give the handler context.
		// value and traceback can be Null
		failed = true;
		PyObject* args = Py_BuildValue("(OOOs)", type, value?value:Py_None, traceback?traceback:Py_None, message);
		if (args) {
			PyObject *res = CallPyObjectWithTrap(exhandler, args);
			if (res) {
				handled = true;
				failed = false;
				Py_DECREF(res);
			}
			Py_DECREF(args);
		}
		mExceptionHandler = exhandler;
	}
	if (!handled) {
		PyErr_Restore(type, value, traceback);
		char* str;
		FormatException(&str);
		if (!failed)
			BeOS->SetError(BEDEF, Clsid(), "Unhandled Python exception: %s\n%s", message, str);
		else
			BeOS->SetError(BEDEF, Clsid(), "Failed handling Python exception: %s\n%s", message, str);
		// Also, log the error to the logger!
		CCP_LOGERR_CH( s_chPy, "Unhandled Python exception: %s\n%s\n<end of exception>", message, str);
		CCP_FREE( str );
	} else {
		Py_XDECREF(type);
		Py_XDECREF(value);
		Py_XDECREF(traceback);
		CCP_LOG_CH(s_chPy, "Python handled exception: %s", message);
	}

	PyErr_Clear();
}


//--------------------------------------------------------------------
// BluePyOS::PyErr_BlueError()
// raises a python exception from a blue error.
//--------------------------------------------------------------------
PyObject *BluePyOS::PyErr_BlueError()
{
	return PyThunkLeave(0);
}

//--------------------------------------------------------------------
// BluePyOS::RebaseSimClock()
// Tells Synchro that the sim clock got re-based.
//--------------------------------------------------------------------
void BluePyOS::RebaseSimClock(Be::Time oldTime, Be::Time newTime)
{
#if CCP_STACKLESS
	if( mSynchro != NULL )
	{
		mSynchro->RebaseSimClock(oldTime, newTime);
	}
#endif
}

bool BluePyOS::CanYield()
{
#if CCP_STACKLESS
	if( !PyGILState_Check() )
	{
		// We don't have thread state. Calling GetCurrent() would create new thread state
		// resulting in a new main tasklet which cannot block.
		// Early out because schedule managers only live as long as their Python thread state
		// and decref-ing the current tasklet of a scheduler that does not exist anymore can cause issues.
		return false;
	}
	PyTaskletObject *current = (PyTaskletObject *)SchedulerAPI()->PyScheduler_GetCurrent();
	int isMain = SchedulerAPI()->PyTasklet_IsMain( current );
	int blockTrap = SchedulerAPI()->PyTasklet_GetBlockTrap( current );
	Py_DECREF(current);

	if( isMain )
	{
		// Main tasklet can never yield
		return false;
	}

	if( blockTrap )
	{
		// The tasklet has been explicitly blocked from yielding
		return false;
	}

	return true;

#else

	return false;

#endif

}

bool BluePyOS::Yield()
{
#if CCP_STACKLESS

	PyObject* ret = mSynchro->Yield();
	if( ret )
	{
		Py_DECREF(ret);
	}

	// Note that we don't deal with Python errors here, but rather let them
	// bubble up. This allows the caller to decide whether to deal with it
	// or let it bubble further up.

	return ret != NULL;

#else

	return true;

#endif
}

SchedulerStats& BluePyOS::GetSchedulerStats( )
{
#if CCP_STACKLESS
	return mScheduler.GetStats();
#endif
}

ITaskletTimer *BluePyOS::GetTaskletTimer()
{
#if CCP_STACKLESS
	return &mTTimer;
#else
	return nullptr;
#endif
}



//--------------------------------------------------------------------
PyObject* BluePyOS::CreateTasklet(
	PyObject* meth,
	PyObject* args,
	PyObject* kw
	)
{
	SafeAutoTasklet _at(GetTaskletTimer(), TIMERS[TIMER_CREATETASKLET].mContext);

	PyObject* ctx = NULL;
	if (meth != NULL && PyUnicode_Check(meth))
	{
		//A context string can be passed in as the first argument
		ctx = meth;
		Py_INCREF(ctx);
		meth = args = kw = NULL;
	}

	// Pretty print function
	if (ctx == NULL)
	{
		// Prepare PrettyPrint for caller
		const char* caller;
		int callerline;
		PyFrameObject* callframe = PyEval_GetFrame();
		if (callframe && PyFrame_Check(callframe)) {
			auto frameCode = PyFrame_GetCode( callframe );
			caller = PyUnicode_AsUTF8( frameCode->co_filename );
			callerline = PyFrame_GetLineNumber( callframe );
			Py_DECREF( frameCode );
		} else {
			caller = "CFrame";
			callerline = 0;
		}
		PyObject *tail = PrettyPrint(meth, caller, callerline);
		ctx = PyUnicode_FromString("AutoContext::");
		if (!tail || !ctx)
			return NULL;
		ctx = PyUnicode_Concat(ctx, tail);
                Py_DECREF(tail);
		if (!ctx)
			return NULL;
	}

	PyObject* ret = CreateTaskletImpl(meth, args, kw, ctx);
	Py_DECREF(ctx);

	return ret;
}


//--------------------------------------------------------------------
PyObject* BluePyOS::CreateTaskletImpl(
	PyObject* meth,
	PyObject* args,
	PyObject* kw,
	PyObject* ctx
	)
{
#if CCP_STACKLESS

	CCP_ASSERT( ctx );

	if( !mTaskletExt )
	{
		//The tasklet extension class
		PyObject *bluepy = PyImport_ImportModule("bluepycore");
		if( !bluepy )
		{
			return nullptr;
		}
		mTaskletExt = BluePy(PyObject_GetAttrString(bluepy, "TaskletExt"));
		if( !mTaskletExt )
		{
			return nullptr;
		}

		Py_DECREF(bluepy);
	}

	BluePy result( PyObject_CallFunctionObjArgs( mTaskletExt, ctx, meth, 0 ) );
	if( !result )
	{
		return nullptr;
	}
	if( args && meth && SchedulerAPI()->PyTasklet_Setup( (PyTaskletObject*)result.o, args, kw ) < 0 )
	{
		return nullptr;
	}

	return result.Detach();

#else

	return nullptr;

#endif
}


PyObject *BluePyOS::CallPyObjectWithTrap(
	PyObject *callable,
	PyObject* args,
	PyObject* ctx)
{
#if CCP_STACKLESS

	//capture old block_trap, set it
	int oldTrap;
	BluePy current( SchedulerAPI()->PyScheduler_GetCurrent() );
	if (current) {
		oldTrap = SchedulerAPI()->PyTasklet_GetBlockTrap((PyTaskletObject*)(PyObject*)current);
		SchedulerAPI()->PyTasklet_SetBlockTrap((PyTaskletObject*)(PyObject*)current, 1);
	}

	BluePy ret;
	if (ctx) {
		AutoTasklet _at(&mTTimer, ctx);
		ret = BluePy(PyObject_CallObject(callable, args));
	} else
		ret = BluePy(PyObject_CallObject(callable, args));

	//restore block trap
	if (current)
		SchedulerAPI()->PyTasklet_SetBlockTrap((PyTaskletObject*)(PyObject*)current, oldTrap);
	return ret.Detach();

#else

	BluePy ret = BluePy(PyObject_CallObject(callable, args));
	return ret.Detach();

#endif
}


//--------------------------------------------------------------------
// Event dispatching helper
//--------------------------------------------------------------------
bool BluePyOS::DispatchEvent(
	IRoot* target,
	const char* context,
	const char* eventName,
	PyObject** pRetval,
	const char* format,
	va_list vargs,
	bool post
	)
{
	auto gil = PyGILState_Ensure();
	ON_BLOCK_EXIT( [&gil] { PyGILState_Release( gil ); } );

	// Check naming convention, must be "DoXXX" or "OnXXX"
	if (
		(!post && eventName[0] != 'D' && eventName[1] != 'o') ||
		(post && eventName[0] != 'O' && eventName[1] != 'n')
		)
	{
		BeOS->SetError(
			BEDEF, Clsid(),
			"In SendEvent/PostEvent, event name has incorrect prefix: %s",
			eventName
			);

		return false;
	}

	// Do this now, so we don't have to worry later
	if (pRetval)
		*pRetval = NULL;

	// Get a deco function bound to the blue python wrapper.

	// Does this object have decoration?  If it doesn't then no sweat.
	BlueLockData* ld = BlueInternalHasLockData(target);
	if (!ld || ld->mPythonKlass == NULL)
		return true;

	BluePy targetSelf(BlueWrapObjectForPython(target));
	if (!targetSelf)
		return false;

	//Get the attribute
	bool handled;
	BluePy bound(ld->mPythonKlass->GetAttr(eventName, &handled, targetSelf));
	if (!handled)
		return true;

	// Cook up arguments
	BluePy args;
	if (format != NULL)
		args = BluePy(Py_VaBuildValue((char*)format, vargs));
	else
		args = BluePyTuple(0);
	if (!args) {
		PyError();
		return false;
	}

	BluePyStr ctx(context);
	// Call the method?
	bool retval = true;
	if (!post)
	{
		BluePy ret(CallPyObjectWithTrap(bound, args, ctx));
		if (!ret) {
			PyError();
			retval = false;
		}
		if (pRetval)
			*pRetval = ret.Detach();
	}
	else
	{
		BluePy tasklet( CreateTaskletImpl( bound, args, nullptr, ctx ) );
		if (!tasklet) {
			PyError();
			retval = false;
		}
	}
	return retval;
}

//--------------------------------------------------------------------
// IBluePython::SendEvent
//--------------------------------------------------------------------
bool BluePyOS::SendEvent(
	IRoot* caller,
	const char* context,
	const char* eventName,
	PyObject** pRetval,
	const char* format,
	...
	)
{
	bool ret;

	if (format != NULL)
	{
		va_list vargs;
		va_start(vargs, format);
		ret = DispatchEvent(
			caller, context, eventName, pRetval, format, vargs, false);
		va_end(vargs);
		return ret;
	}
	else
	{
		ret = DispatchEvent(
			caller, context, eventName, pRetval, format, NULL, false);
	}

	return ret;
}


//--------------------------------------------------------------------
// IBluePython::PostEvent
//--------------------------------------------------------------------
bool BluePyOS::PostEvent(
	IRoot* caller,
	const char* context,
	const char* eventName,
	const char* format,
	...
	)
{
	bool ret;

	if (format != NULL)
	{
		va_list vargs;
		va_start(vargs, format);
		ret = DispatchEvent(
			caller, context, eventName, NULL, format, vargs, true);
		va_end(vargs);
		return ret;
	}
	else
	{
		ret = DispatchEvent(
			caller, context, eventName,  NULL, NULL, NULL, true);
	}

	return ret;
}


//--------------------------------------------------------------------
// IPythonEvents::DoStackTrace
//--------------------------------------------------------------------
//PyObject *BluePyOS::GetStackTrace(
//	PyObject *fo
//	)
//{
//#if CCP_STACKLESS
//
//	if (!fo)
//		fo = (PyObject*) PyEval_GetFrame();
//
//	BluePyStr result;
//	while (fo)
//	{
//		BluePyStr line;
//		if (PyFrame_Check(fo))
//		{
//			// regular python frame
//			PyFrameObject *f = reinterpret_cast<PyFrameObject*>(fo);
//			line = BluePyStr::Format(
//				"%s(%d): %s\n",
//				PyUnicode_AsUTF8(f->f_code->co_filename),
//				f->f_lineno,
//				PyUnicode_AsUTF8(f->f_code->co_name)
//				);
//			fo = (PyObject*) f->f_back;
//		} else if (fo->ob_type == &PyCFrame_Type) {
//			// A stackless frame
//			line = BluePyStr("CFrame\n");
//			PyCFrameObject *cf = reinterpret_cast<PyCFrameObject*>(fo);
//			fo = (PyObject*) cf->f_back;
//		} else
//			fo = 0;
//
//		if (line) {
//			if (result)
//				result = result + line;
//			else
//				result = line;
//		}
//	}
//	if (!result)
//		result = BluePyStr("Invalid Python Stack\n");
//	return result.Detach();
//
//#else
//
//	return nullptr;
//
//#endif
//
//}
//
//
//void BluePyOS::DoStackTrace(
//	PyObject * fo
//	)
//{
//	CCP_LOGWARN_CH( s_chPy, "BluePyOS::DoStackTrace <begin>");
//	BluePyStr str = BluePy(GetStackTrace(fo));
//	if (str) {
//		const char *data = str.Str();
//		while (data && *data) {
//			const char *found = strchr(data, '\n');
//			BluePyStr line;
//			if (found) {
//				line = BluePyStr(found-data, data);
//				data = found+1;
//			} else {
//				line = BluePyStr(data);
//				data = 0;
//			}
//			CCP_LOGWARN_CH( s_chPy,"    %s",line.Str());
//		}
//	}
//	CCP_LOGWARN_CH( s_chPy, "BluePyOS::DoStackTrace <end>");
//}




//////////////////////////////////////////////////////////////////////
//
// Python thunkers
//
//////////////////////////////////////////////////////////////////////


//--------------------------------------------------------------------
// AddExitProc
//--------------------------------------------------------------------
PyObject* BluePyOS::PyAddExitProc(PyObject* args)
{
	PyObject* pyobj;

	if (!PyArg_ParseTuple(args, "O", &pyobj))
		return nullptr;

	if (!PyCallable_Check(pyobj))
	{
		PyErr_SetString(PyExc_ValueError,"Argument must be a callable object");
		return nullptr;
	}

	if (!mExitProcs) {
		mExitProcs = PyList_New(0);
		if (!mExitProcs)
			return nullptr;
	}
	PyList_Append(mExitProcs, pyobj);
	Py_RETURN_NONE;
}


//--------------------------------------------------------------------
// GetArg
//--------------------------------------------------------------------

PyObject* BluePyOS::PyGetArg(PyObject* args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;

	const std::vector<std::wstring>& argv = BeOS->GetStartupArgs();

	PyObject* list = PyList_New(argv.size());
	if (!list)
		return 0;

	for (size_t i = 0; i < argv.size(); i++)
	{
		const wchar_t *arg = argv[i].c_str();
		PyList_SET_ITEM(list, i, PyUnicode_FromWideChar(arg, argv[i].size()));
	}

	return list;
}


//--------------------------------------------------------------------
// DumpState
//--------------------------------------------------------------------
PyObject* BluePyOS::PyDumpState(PyObject* args)
{
#if CCP_STACKLESS
	if (!PyArg_ParseTuple(args, ""))
		return NULL;

	char tmp[CCP_MAX_PATH];

	PyObject* file = PySys_GetObject((char*)"stderr");

	if (!file)
		return NULL;

	// first thread is console thread
	PyFile_WriteString("Thread # 0:\n", file);
	PyTraceBack_Here(PyEval_GetFrame());
	PyObject *exc, *val, *tb;
	PyErr_Fetch(&exc, &val, &tb);
	PyTraceBack_Print(tb, file);
	Py_XDECREF(exc);
	Py_XDECREF(val);
	Py_XDECREF(tb);
	PyFile_WriteString("\n\n", file);


	for (unsigned i = 0; i < mThreads.size(); i++)
	{
		sprintf_s(tmp, "Thread # %d:\n", i+1);
		PyFile_WriteString(tmp, file);

		if (mThreads[i].mTraceback)
		{
			PyTraceBack_Print(mThreads[i].mTraceback, file);
		}
		else
		{
			PyFile_WriteString("No stack trace available for this thread\n", file);
		}

		PyFile_WriteString("\n\n", file);
	}
#endif

	Py_INCREF(Py_None);
	return Py_None;
}

//--------------------------------------------------------------------
// write
//--------------------------------------------------------------------
PyObject* BluePyOS::Pywrite(PyObject* args)
{
	const char *text;

	if (!PyArg_ParseTuple(args, "s", &text))
		return NULL;

	Py_INCREF(Py_None);
	return Py_None;
}


//--------------------------------------------------------------------
// CreateTasklet
//--------------------------------------------------------------------
PyObject* BluePyOS::PyCreateTasklet(PyObject* _args)
{
	PyObject* meth = NULL;
	PyObject* args = NULL;
	PyObject* kw = NULL;

	if (!PyArg_ParseTuple(_args, "|OO!O!", &meth, &PyTuple_Type, &args, &PyDict_Type, &kw))
		return NULL;

	return CreateTasklet(meth, args, kw);
}

//--------------------------------------------------------------------
// NextScheduledEvent
//--------------------------------------------------------------------
PyObject* BluePyOS::PyNextScheduledEvent(PyObject* args)
{
	PyErr_WarnEx( PyExc_DeprecationWarning, "NextScheduledEvent serves no purpose any more", 1 );
	Py_RETURN_NONE;
}


//--------------------------------------------------------------------
// SetClipboardData.  Thunks to the BlueClipboard
//--------------------------------------------------------------------
PyObject* BluePyOS::PySetClipboardData(PyObject* args)
{
	PyObject* data;
	if( !PyArg_ParseTuple( args, "O", &data ) )
	{
		return nullptr;
	}
	return BlueClipboard::GetInstance().PySetData( data );
}


#if CCP_STACKLESS

//--------------------------------------------------------------------
PyObject* BluePyOS::PyGetTimeSinceSwitch(PyObject *args)
{
	return PyFloat_FromDouble(GetTimeSinceSwitch(false));
}
#endif


//--------------------------------------------------------------------
PyObject* BluePyOS::PyBeNice(PyObject* args)
{
#if CCP_STACKLESS

	double timeSlice = mBeNiceSlice;  //default timeslice in milliseconds
	if (!PyArg_ParseTuple(args, "|d", &timeSlice))
		return NULL;

	//convert to seconds, which is what GetElapsed returns
	timeSlice *= 0.001f;

	PyTaskletObject* tasklet = (PyTaskletObject*)SchedulerAPI()->PyScheduler_GetCurrent();
	if (!tasklet)
		return 0;
	if (SchedulerAPI()->PyTasklet_IsMain(tasklet) || SchedulerAPI()->PyTasklet_GetBlockTrap(tasklet)) {
		//silently do nothing.  This allows any code to be interspersed with benice comments.
		Py_DECREF(tasklet);
		Py_INCREF(Py_None);
		return Py_None;
	}
	Py_DECREF(tasklet);

	double elapsed = GetTimeSinceSwitch();
	if (elapsed >= timeSlice) {
		return mSynchro->Yield();
	}

#endif

	Py_INCREF(Py_None);
	return Py_None;
}



PyObject * BluePyOS::PyGetMaxRunTime(PyObject *args)
{
#if CCP_STACKLESS
	if (!PyArg_ParseTuple(args, ":GetMaxRunTime"))
		return 0;
	return PyFloat_FromDouble(mScheduler.GetMaxTime());

#else

	return nullptr;

#endif
}


PyObject * BluePyOS::PySetMaxRunTime(PyObject *args)
{
#if CCP_STACKLESS
	float t;
	if (!PyArg_ParseTuple(args, "f:GetMaxRunTime", &t))
		return 0;
	mScheduler.SetMaxTime(t);
#endif

	Py_RETURN_NONE;
}


PyObject * BluePyOS::CallMethodWithTrap(PyObject *target, const char *method, const char *ctxt, const char *format, ...)
{
#if CCP_STACKLESS

	BluePy args;
	if (format && *format) {
		va_list va;
		va_start(va, format);
		args = BluePy(Py_VaBuildValue((char*)format, va));
		va_end(va);
	} else
		args = BluePyTuple(0);

	//capture old block_trap, set it
	int oldTrap;
	BluePy current( SchedulerAPI()->PyScheduler_GetCurrent() );
	if (current) {
		oldTrap = SchedulerAPI()->PyTasklet_GetBlockTrap((PyTaskletObject*)(PyObject*)current);
		SchedulerAPI()->PyTasklet_SetBlockTrap((PyTaskletObject*)(PyObject*)current, 1);
	}
	BluePy ret;
	if (ctxt) {
		AutoTasklet _at(&mTTimer, ctxt);
		if (method && *method)
			ret = BluePy(PyObject_CallMethod(target, (char*)method, (char*)"O", args.o));
		else
			ret = BluePy(PyObject_CallObject(target, args));
	} else {
		if (method && *method)
			ret = BluePy(PyObject_CallMethod(target, (char*)method, (char*)"O", args.o));
		else
			ret = BluePy(PyObject_CallObject(target, args));
	}

	//restore block trap
	if (current)
		SchedulerAPI()->PyTasklet_SetBlockTrap((PyTaskletObject*)(PyObject*)current, oldTrap);

	return ret.Detach();

#else

	return nullptr;

#endif
}


//scatter, chain, or send an event to the python service manager as appropriate
bool BluePyOS::PythonEvent(const char *event, PyObject * arg)
{
	ScopedBlockTrap blockTrap;

	if( strncmp("Do", event, 2) == 0 )
	{
		if( m_sendEvent )
		{
			return m_sendEvent.CallVoid( event, arg );
		}
		else
		{
			CCP_LOGWARN_CH( s_chPy, "PyOS.sendEvent has not been set" );
		}
	}
	else if ( strncmp("Process", event, 7) == 0 )
	{
		if( m_chainEvent )
		{
			return m_chainEvent.CallVoid( event, arg );
		}
		else
		{
			CCP_LOGWARN_CH( s_chPy, "PyOS.chainEvent has not been set" );
		}
	}
	else
	{
		if( m_scatterEvent )
		{
			return m_scatterEvent.CallVoid( event, arg );
		}
		else
		{
			CCP_LOGWARN_CH( s_chPy, "PyOS.scatterEvent has not been set" );
		}
	}

	return true;
}

void BluePyOS::LogCpuUsageAndOtherStats()
{
#if CCP_STACKLESS
    Be::Time now = TimeNow();
	if (now - mLastCpuUpdate >= mPerformanceUpdateFrequency)
	{
		// 10 secs. since last update
		mLastCpuUpdate = now;
		int64_t tkrn, tusr;
		int64_t pkrn, pusr;

        CcpGetThreadTimes( tkrn, tusr );
        CcpGetProcessTimes( pkrn, pusr );

		// Update mem usage stats
		size_t memusage = 0;
		size_t workingset = 0;
		uint32_t pagefaults = 0;

		CcpProcessMemoryInfo memInfo;
		if ( ! CcpGetProcessMemoryInfo( memInfo ) ) {
			CCP_LOGWARN_CH( s_chMemory, "Failed retrieving memory info" );
		}


		BluePy sys(PyImport_ImportModule("sys"));
		BluePy pymem;
		if (sys)
			pymem = BluePy(PyObject_CallMethod(sys, (char*)"getpymalloced", (char*)""));

		Synchro::Stat synchroStat;
		mSynchro->GetLastStat(synchroStat);

		SchedulerStats& schedulerStats = mScheduler.GetStats();
		if (pymem) {
			BlueCpuUsagePtr stats;
			stats.CreateInstance();
			stats->timestamp = now;
			stats->userProcessCpuUsage = pkrn+pusr - mLastProcessCpuUsage;
			stats->userThreadCpuUsage = tkrn+tusr - mLastThreadCpuUsage;
			stats->kernelProcessCpuUsage = pkrn - mLastProcessKernelUsage;
			stats->kernelThreadCpuUsage = tkrn - mLastThreadKernelUsage;

			stats->pageFileUsage = memInfo.pageFileUsage;
			stats->pythonMemoryUsage = PyLong_AsSsize_t( static_cast<PyObject*>( pymem ) );
			stats->workingSetSize = memInfo.workingSetSize;
			stats->pageFaultCount = memInfo.pageFaultCount;

			stats->fps = BeOS->GetInfo()->mFps;
			stats->taskletsYielding = synchroStat.mYielders;
			stats->taskletsSleeping = synchroStat.mSleepers;
			stats->taskletsSchedulerDuration = schedulerStats.lastDurationMs,
			stats->taskletsSchedulerMaxDuration = schedulerStats.maxTimeMs,
			stats->taskletsSchedulerDurationOvershoot = schedulerStats.overshootMs;
			stats->taskletsQueued = schedulerStats.numberOfTaskletsInQueuePostTick;
			stats->taskletsActive = schedulerStats.numberOfActiveTaskets;
			stats->scheduleManagersActive = schedulerStats.numberOfActiveScheduleManagers;
			stats->channelsActive = schedulerStats.numberOfActiveChannels;
			stats->taskletsProcessed = schedulerStats.numberOfTaskletsCompletedLastTick;
			stats->taskletsSwitched = schedulerStats.numberOfTaskletsSwitchedLastTick;

			mCpuUsage.Append(stats.Detach());
		}

		mLastThreadCpuUsage = tkrn+tusr;
		mLastProcessCpuUsage = pkrn+pusr;
		mLastThreadKernelUsage = tkrn;
		mLastProcessKernelUsage = pkrn;
	}
#endif
}

#ifdef _WIN32
//--------------------------------------------------------------------
extern "C" BOOL WINAPI GetProcessMemoryInfo(
    HANDLE Process,
    PPROCESS_MEMORY_COUNTERS ppsmemCounters,
    DWORD cb
    )
{
	typedef BOOL (WINAPI GPMI)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
	static GPMI* fn = NULL;
	if (fn == NULL)
	{
		HMODULE psapi = LoadLibrary("psapi.dll");

		if (psapi == NULL)
			return FALSE;

		fn = (GPMI*)GetProcAddress(psapi, "GetProcessMemoryInfo");

		if (fn == NULL)
			return FALSE;
	}

	return fn(Process, ppsmemCounters, cb);
}

#endif

PyObject* LoadPythonExtension( PyObject* self, PyObject* args )
{
	char* name;
	if( !PyArg_ParseTuple( args, "s", &name ) )
	{
		return nullptr;
	}

	return BlueLoadPythonExtension( name );
}

PyObject* BlueLoadPythonExtension(const char* name) {

// accommodate for release builds, which use `-DCCP_BUILD_FLAVOR=`, e.g. macro without a value
#if ~(~CCP_BUILD_FLAVOR + 0) == 0 && ~(~CCP_BUILD_FLAVOR + 1) == 1
	std::string flavor;
#else
	std::string flavor{ CCP_STRINGIZE( CCP_BUILD_FLAVOR ) };
#endif
	std::string importName = name + flavor;

	CCP_LOGNOTICE( "Trying to load python extension '%s' with flavor '%s'", name, flavor.c_str() );

	PyObject* module = PyImport_ImportModule( importName.c_str() );

	// An import exception will be propogated up, no need to worry about failures here
	if (!module) {
        CCP_LOGERR( "Failed to load python extension '%s' with flavor '%s'", name, flavor.c_str() );
	} else {
        CCP_LOGNOTICE( "Successfully loaded python extension '%s' with flavor '%s'", name, flavor.c_str() );
	}

	return module;
}

#endif
