// blue.cpp : Defines the entry point for the DLL application.
//

#include "StdAfx.h"

#include "include/Blue.h"
#include "include/Blue.cxx"
#include "BlueExposure/include/InterfaceDefinitions.cxx"
#include "BluePaths.h"
#include "BlueOS.h"
#include "logger/Logger.h"
#include "ResourceLoading.h"
#include "BlueExposure/BlueLuaThunkers.h"

const char* g_moduleName = "blue";
std::wstring s_logDeviceName( L"EVE" );

#ifdef _WIN32
#include "win32.h"
static HINSTANCE s_instance = NULL;
#endif


// The templated container classes need special treatment here. Generally
// each exposed class gets its own Python type object, but the templated
// objects all share one type object. This setup here below ensures that
// this type object is known to Python.
typedef BlueList<IRoot> GenericList;
TYPEDEF_BLUECLASS(GenericList);
BLUE_DEFINE( GenericList );

typedef BlueDict<IRoot> GenericDict;
TYPEDEF_BLUECLASS( GenericDict );
BLUE_DEFINE(  GenericDict );

#ifdef _WIN32

namespace
{
	CcpMutex s_logMutex( g_moduleName, "s_logMutex" );
	void LogToLogServer( CcpLogChannel_t& logObject, CCP::LogType type, unsigned long userData, const char* message )
	{
		CcpAutoMutex guard( s_logMutex );

		if( userData == 0 )
		{
			userData = LogTypeToTLOGFLAG( type );
		}

		Log__global( *(TLOGOBJECT*)&logObject, (TLOGFLAG)userData, message );
	}
}

#if BLUE_WITH_PYTHON
PyObject* PyAttachToLogServer( PyObject* self, PyObject* args )
{
	if( Log__IsLogging() )
	{
		CCP_LOG( "LogServer can't reattach" );
		Py_RETURN_NONE;
	}

	Log__InitLibrary( (LONG_PTR)s_instance, CW2A( s_logDeviceName.c_str()));
	if( Log__IsLogging() )
	{
		// Instruct the CCP logging system to echo to the LogServer as well
		CCP::RegisterLogEcho( &LogToLogServer, CCP::LOGTYPE_INFO, true );
		CCP_LOG( "LogServer has been attached" );
	}
	else
	{
		CCP_LOG( "Failed to attach to LogServer" );
	}
	Py_RETURN_NONE;
}

MAP_FUNCTION( "AttachToLogServer", PyAttachToLogServer, "Attaches to the log server" );

PyObject* PyEnableDebuggerLogging( PyObject* self, PyObject* args )
{
	int threshold = 0;

	if( PyArg_ParseTuple( args, "|i", &threshold ) )
	{
		if( threshold < CCP::LOGTYPE_LOWEST )
		{
			threshold = CCP::LOGTYPE_LOWEST;
		}
		if( threshold > CCP::LOGTYPE_HIGHEST )
		{
			threshold = CCP::LOGTYPE_HIGHEST;
		}
	}

	// Instruct the CCP logging system to echo to the debugger output window.
	CCP::RegisterLogEcho( &CCP::LogToDebugger, (CCP::LogType)threshold, true );

	Py_RETURN_NONE;
}

MAP_FUNCTION( "EnableDebuggerLogging", PyEnableDebuggerLogging, "Enables echoing of log to debugger output window" );
#endif


BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
	s_instance = instance;

	if (reason == DLL_PROCESS_ATTACH)
	{
		//turn off heap checking
		int oldFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
		int newFlag = (oldFlag & 0xffff) | _CRTDBG_CHECK_DEFAULT_DF;
		newFlag &= ~_CRTDBG_CHECK_ALWAYS_DF;
		_CrtSetDbgFlag(newFlag);

		DisableThreadLibraryCalls(instance);

		// Inform the logging system of the main thread
		CCP::SetLogMainThreadId();

		unsigned int memoryLoad = 0;

		// This is duplicating work from ExeFile but I don't see a good
		// way around that. If I have ExeFile call functions in BeOS or
		// something I can't get data from arguments until after all
		// the initialization process is done.
		std::vector<std::wstring> argv = GetSplitCommandLine();

		for( size_t i = 1; i < argv.size(); ++i )
		{
			const std::wstring &arg = argv[i];
			if( arg.find( L"/logDevice=" ) == 0 )
			{
				s_logDeviceName = arg.substr( 11 );
			}
			else if( arg.find( L"/logDebugger" ) == 0 )
			{
				// Instruct the CCP logging system to echo to the debugger output window.
				CCP::RegisterLogEcho( &CCP::LogToDebugger, CCP::LOGTYPE_INFO, true );
			}
			else if( arg.find( L"/memoryTracking" ) == 0 )
			{
				MemoryTrackerInitialize();
			}
			else if( arg.find( L"/memoryTracking=" ) == 0 )
			{
				MemoryTrackerInitialize();
			}
			else if( arg.find( L"/memoryLoad=" ) == 0 )
			{
				memoryLoad = _wtoi( arg.substr( 12 ).c_str() );
			}
		}

		Log__InitLibrary( (LONG_PTR)s_instance, CW2A( s_logDeviceName.c_str()));
		if( Log__IsLogging() )
		{
			// Instruct the CCP logging system to echo to the LogServer as well
			CCP::RegisterLogEcho( &LogToLogServer, CCP::LOGTYPE_INFO, true );
		}
				
		CCP_LOG( "Blue module starting" );
		
		OSVERSIONINFOEX ver = {0};
		ver.dwOSVersionInfoSize = sizeof(ver);
		GetWindowsVersion( ver );
		CCP_LOG( "Windows version %d.%d.%d \"%s\" platform:%d sp:%d.%d suitemask:%d product:%d",
			ver.dwMajorVersion,
			ver.dwMinorVersion,
			ver.dwBuildNumber,
			ver.szCSDVersion,
			ver.dwPlatformId, 
			ver.wServicePackMajor, ver.wServicePackMinor, 
			ver.wSuiteMask, ver.wProductType);

		if( memoryLoad )
		{
			unsigned int memSize = memoryLoad * 1024*1024;
			unsigned int oneHundredMegs = 100*1024*1024;
			while( memSize >= oneHundredMegs )
			{
				void* p = CCP_MALLOC( "memoryLoad", oneHundredMegs );
				if( !p )
				{
					CCP_LOGERR( "Allocating 100 MB for artificial memory load failed" );
				}
				else
				{
					CCP_LOG( "Allocated 100 MB for artificial memory load" );
					memset( p, 0, oneHundredMegs );
				}
				memSize -= oneHundredMegs;
			}
			void* p = CCP_MALLOC( "memoryLoad", memSize );
			if( !p )
			{
				CCP_LOGERR( "Allocating %d MB for artificial memory load failed", memSize / (1024*1024) );
			}
			else
			{
				CCP_LOG( "Allocated %d MB for artificial memory load", memSize / (1024*1024) );
				memset( p, 0, memSize );
			}
		}

#if CCP_STACKLESS
		BeClasses->RegisterClasses( BlueRegistration::GetClassRegs() );
		BlueInitializePaths();
#endif
	}
	else if (reason == DLL_PROCESS_DETACH)
	{
		BeClasses->UnregisterClasses( BlueRegistration::GetClassRegs() );
		CCP_LOG( "Blue module terminating");
		
		// not terminating this here, there are a lot
		// of automagic classes that are still alive
		// and may have to express themselves
		//LOGTERMINATE();
	}
	else if (reason == DLL_THREAD_ATTACH)
	{
	}
	else if (reason == DLL_THREAD_DETACH)
	{
	}

	return TRUE;
}
#elif defined( __ORBIS__ )
extern "C" int DLLEXPORT module_start( size_t, const void* )
{
	return 0;
}
#endif

#if !CCP_STACKLESS

#if BLUE_WITH_PYTHON

namespace
{

// Saved process exit code
static int s_exitCode = 0;
// Saved original sys.exit function
PyObject* s_savedSysExit = nullptr;

void ExtractReturnCode( PyObject* code )
{
	if( code )
	{
		if( code == Py_None )
		{
			s_exitCode = 0;
		}
		else if( PyInt_Check( code ) )
		{
			s_exitCode = int( PyInt_AsLong( code ) );
		}
		else
		{
			s_exitCode = 1;
		}
	}
}

PyObject* BlueExceptHook( PyObject* self, PyObject* args )
{
	s_exitCode = 1;
	return PyObject_Call( PySys_GetObject( (char*)"__excepthook__" ), args, nullptr );
}

PyObject* BlueExit( PyObject* self, PyObject* args )
{
	PyObject* code = nullptr;
	if( PyArg_ParseTuple( args, "|O", &code ) )
	{
		ExtractReturnCode( code );
		Py_XDECREF( code );
	}
	return PyObject_Call( s_savedSysExit, args, nullptr );
}

void BlueAtExit()
{
	BeOS->Terminate( s_exitCode );
}

void PatchPythonExit()
{
	Py_AtExit( &BlueAtExit );

	PyObject* sysmodule = PyImport_ImportModule("sys");

	static PyMethodDef exceptHookDef;
	exceptHookDef.ml_doc = "Pathed sys.excepthook that records process exit code.\nPart of Blue exit patching";
	exceptHookDef.ml_flags = METH_VARARGS;
	exceptHookDef.ml_meth = &BlueExceptHook;
	exceptHookDef.ml_name = "excepthook";
	PyObject* fn = PyCFunction_New( &exceptHookDef, nullptr );
	PySys_SetObject( (char*)"excepthook", fn );
	Py_DecRef( fn );

	s_savedSysExit = PySys_GetObject( (char*)"exit" );

	static PyMethodDef exitDef;
	exitDef.ml_doc = "Pathed sys.exit that records process exit code.\nPart of Blue exit patching";
	exitDef.ml_flags = METH_VARARGS;
	exitDef.ml_meth = &BlueExit;
	exitDef.ml_name = "exit";
	fn = PyCFunction_New( &exitDef, nullptr );
	PySys_SetObject( (char*)"exit", fn );
	Py_DecRef( fn );

	Py_DecRef( sysmodule );
}

}

PyMODINIT_FUNC
	initblue(void)
{
	// Inform the logging system of the main thread
	CCP::SetLogMainThreadId();

	// Register classes as early as possible - otherwise CreateInstance won't work
	BeClasses->RegisterClasses( BlueRegistration::GetClassRegs() );

	BlueInitializePaths();
	BlueInitializeResourceLoading();

	BeOS->Startup(13,0);

	PatchPythonExit();
}

#elif BLUE_WITH_LUA

extern "C" int DLLEXPORT luaopen_blue( lua_State* ls )
{
	// Inform the logging system of the main thread
	CCP::SetLogMainThreadId();

	// Instruct the CCP logging system to echo to the debugger output window.
	CCP::RegisterLogEcho( &CCP::LogToDebugger, CCP::LOGTYPE_LOWEST, true );

	BeClasses->RegisterClasses( BlueRegistration::GetClassRegs() );
	BlueRegisterClasses( ls, g_moduleName, BlueRegistration::GetClassRegs() );
	BlueRegisterFunctions( ls, g_moduleName, BlueRegistration::GetFuncRegs() );
	BlueRegisterInterfaceMethods( ls, BlueRegistration::GetThunkerRegs() );

	BlueInitializePaths();
	BlueInitializeResourceLoading();

	BlueRegisterObjectsToModule( ls, g_moduleName, BlueRegistration::GetObjectRegs() );

	BeOS->Startup(13,0);

	return 1;
}
#endif

#endif
