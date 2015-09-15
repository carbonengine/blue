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
#include "BlueSocketLogger.h"
#include "CcpUtils/PyCpp.h"

const char* g_moduleName = "blue";
std::wstring s_logDeviceName( L"EVE" );

#ifdef _WIN32
#include "win32.h"
static HINSTANCE s_instance = NULL;
#elif defined(__APPLE__)
#include <fcntl.h>
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

#endif

#if BLUE_WITH_PYTHON
PyObject* PyAttachToLogServer( PyObject* self, PyObject* args )
{
	if( StartSocketLogger() )
	{
		CCP::RegisterLogEcho( &LogToSocketLogger, CCP::LOGTYPE_INFO, true );
		CCP_LOG( "Socket logger has been attached" );
	}
	else
	{
		CCP_LOG( "Failed to attach to socket logger" );
	}
#ifdef _WIN32
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
#endif
	Py_RETURN_NONE;
}

MAP_FUNCTION( "AttachToLogServer", PyAttachToLogServer, "Attaches to the log server" );


//--------------------------------------------------------------------
// AtomicFileRead and Write
// The atomicity is guaratneed by the OS locking thingie, so we can
// release the GIL.
//--------------------------------------------------------------------
namespace
{

PyObject* PyAtomicFileRead(PyObject *self, PyObject* args)
{
	PyObject *filename;
	if (!PyArg_ParseTuple(args, "O!", &PyBaseString_Type, &filename))
		return NULL;
	
	
	BluePy ufn(PyUnicode_FromObject(filename));
	if (!ufn) return 0;
#ifdef _WIN32	
	HANDLE h = INVALID_HANDLE_VALUE;
	DWORD fileSize;
	BY_HANDLE_FILE_INFORMATION info;
	{
		Ccp::PyAllowThreads _allow;
		for(int i = 0; i<10; i++) {
			h = CreateFileW(PyUnicode_AS_UNICODE(ufn.o),
							GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, 0);
			if (h==INVALID_HANDLE_VALUE) {
				DWORD code = GetLastError();
				if (code == ERROR_SHARING_VIOLATION) {
					Sleep(10);
					continue;
				}
			}
			break;
		}
		if (h==INVALID_HANDLE_VALUE)
			goto HERR;

		fileSize = GetFileSize(h, 0);
		if (fileSize == INVALID_FILE_SIZE)
			goto HERR;
		
		BOOL success = GetFileInformationByHandle(h, &info);
		if (!success)
			goto HERR;
	}

	{
		BluePy r(PyString_FromStringAndSize(0, fileSize));
		if (!r) {
			CloseHandle(h);
			return 0;
		}
		DWORD read;
		{
			Ccp::PyAllowThreads _allow;
			BOOL success = ReadFile(h, PyString_AsString(r), fileSize, &read, 0);
			if (!success)
				goto HERR;
				
			CloseHandle(h);
			h = INVALID_HANDLE_VALUE;
		}
		if (read != fileSize)
			return PyErr_SetString(PyExc_RuntimeError, "Read short file"), 0;
		
		return r.Detach();
	}

HERR:
	PyWin32Error();
	if (h != INVALID_HANDLE_VALUE)
		CloseHandle(h);
	return 0;

#elif defined(__APPLE__)

	CW2A filenameStr( reinterpret_cast<const wchar_t*>( PyUnicode_AS_UNICODE( ufn.o ) ) );
	int f;
	long fileSize;
	{
		Ccp::PyAllowThreads _allow;
        f = open( filenameStr, O_RDONLY | O_SHLOCK );
		if( f < 0 )
		{
			return PyErr_SetFromErrnoWithFilename( PyExc_OSError, filenameStr );
		}
		fileSize = lseek( f, 0, SEEK_END );
		lseek( f, 0, SEEK_SET );
	}

	BluePy r( PyString_FromStringAndSize( nullptr, Py_ssize_t( fileSize ) ) );
	if( !r )
	{
		close( f );
		return nullptr;
	}
	ssize_t bytes;
	{
		Ccp::PyAllowThreads _allow;
		bytes = read( f, PyString_AsString(r), fileSize );
		close( f );
	}
	if( long( bytes ) < fileSize )
	{
		return PyErr_SetString(PyExc_RuntimeError, "Read short file"), nullptr;
	}
	return r.Detach();

#else
#error PyAtomicFileRead implementation missing
#endif
}


//Again, atomicity is guaranteed by the os locking ops
PyObject* PyAtomicFileWrite(PyObject *self, PyObject* args)
{
	PyObject *filename;
	PyObject *dataO;
	if (!PyArg_ParseTuple(args, "O!O", &PyBaseString_Type, &filename, &dataO))
		return NULL;
	BluePy ufn(PyUnicode_FromObject(filename));
	if (!ufn) return 0;
	PyBufferProcs *buffer = dataO->ob_type->tp_as_buffer;
	if (!buffer || !buffer->bf_getreadbuffer)
		return PyErr_SetString(PyExc_TypeError, "expected a buffer object"), nullptr;
	
#ifdef _WIN32

	HANDLE h;
	{
		Ccp::PyAllowThreads _allow;		
		for(int i = 0; i<10; i++) {
			h = CreateFileW(PyUnicode_AS_UNICODE(ufn.o),
							GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, 0);
			if (h==INVALID_HANDLE_VALUE) {
				DWORD code = GetLastError();
				if (code == ERROR_SHARING_VIOLATION) {
					Sleep(10);
					continue;
				}
			}
			break;
		}
		if (h==INVALID_HANDLE_VALUE)
			goto HERR;
	}

	Py_ssize_t segcount = buffer->bf_getsegcount(dataO, 0);
	for(Py_ssize_t i = 0; i<segcount; i++){
		void *data;
		Py_ssize_t datalen = buffer->bf_getreadbuffer(dataO, i, &data);
		if (datalen<0) {
			CloseHandle(h);
			return 0;
		}
		//support only DWORD sizes yet
		DWORD written;
		BOOL success;
		{
			Ccp::PyAllowThreads _allow;		
			success = WriteFile(h, data, (DWORD)datalen, &written, 0);
			if (!success)
				goto HERR;
			if (i+1 == segcount) {
				CloseHandle(h);
				h = INVALID_HANDLE_VALUE;
			}
		}
		if (written != datalen) {
			if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
			return PyErr_SetString(PyExc_IOError, "Wrote short file"), 0;
		}
		if (i+1 < segcount) {
			DWORD moved = SetFilePointer(h, (DWORD)datalen, 0, FILE_CURRENT);
			if (moved == INVALID_SET_FILE_POINTER)
				goto HERR;
		}
	}	
	if (h != INVALID_HANDLE_VALUE)
		CloseHandle(h);
	Py_INCREF(Py_None);
	return Py_None;

HERR:
	PyWin32Error();
	if (h != INVALID_HANDLE_VALUE)
		CloseHandle(h);
	return 0;

#elif defined(__APPLE__)

	CW2A filenameStr( reinterpret_cast<const wchar_t*>( PyUnicode_AS_UNICODE( ufn.o ) ) );
	int f;
	{
		Ccp::PyAllowThreads _allow;
		auto f = open( filenameStr, O_WRONLY | O_EXLOCK | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR );
		if( f < 0 )
		{
			return PyErr_SetFromErrnoWithFilename( PyExc_OSError, filenameStr );
		}
	}
	Py_ssize_t segcount = buffer->bf_getsegcount( dataO, 0 );
	for( Py_ssize_t i = 0; i < segcount; i++ )
	{
		void *data;
		Py_ssize_t datalen = buffer->bf_getreadbuffer( dataO, i, &data );
		if( datalen < 0 ) 
		{
			close( f );
			return nullptr;
		}
		//support only DWORD sizes yet
		ssize_t written;
		{
			Ccp::PyAllowThreads _allow;
			written = write( f, data, datalen );
			if( i + 1 == segcount ) 
			{
				close( f );
				f = -1;
			}
		}
		if( written != datalen ) 
		{
			if( f > 0 )
			{
				close( f );
			}
			return PyErr_SetString( PyExc_IOError, "Wrote short file" ), nullptr;
		}
	}	
	Py_RETURN_NONE;

#else
#error PyAtomicFileWrite implementation missing
#endif
}

}

MAP_FUNCTION( 
	"AtomicFileRead",
	PyAtomicFileRead,
	"Reads an entire file atomically. Returns the contents of the file as a string.\n"
	"Raises OSError, IOError.\n"
	"Arguments:\n"
	"filename - path to file" );

MAP_FUNCTION( 
	"AtomicFileWrite",
	PyAtomicFileWrite,
	"Writes an entire file atomically. Raises OSError, IOError.\n"
	"Arguments:\n"
	"filename - path to file\n"
	"contents - buffer containing file contents to write" );



#ifdef _WIN32

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
#endif


#ifdef _WIN32

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

		BlueModuleStartup();
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

void BlueInitializeSocketLogger()
{
    CCP_LOG( "Connecting to socket logger" );
    
	if( StartSocketLogger() )
	{
		CCP::RegisterLogEcho( &LogToSocketLogger, CCP::LOGTYPE_INFO, true );
		CCP_LOG( "Socket logger has been attached" );
	}
	else
	{
		CCP_LOG( "Failed to attach to socket logger" );
	}
}

void BlueModuleStartup()
{
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
            memoryLoad = atoi( CW2A( arg.substr( 12 ).c_str() ) );
        }
    }
    
#ifdef _WIN32
    Log__InitLibrary( (LONG_PTR)s_instance, CW2A( s_logDeviceName.c_str()));
    if( Log__IsLogging() )
    {
        // Instruct the CCP logging system to echo to the LogServer as well
        CCP::RegisterLogEcho( &LogToLogServer, CCP::LOGTYPE_INFO, true );
		CCP_LOG( "Shared memory logger has been attached" );
    }
#endif

    CCP_LOG( "Blue module starting" );
    
#ifdef _WIN32
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
#endif
    
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
#ifndef _WIN32
    BlueModuleStartup();
#endif
	BlueInitializeSocketLogger();

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
