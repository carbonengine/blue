// Copyright © 2014 CCP ehf.

// blue.cpp : Defines the entry point for the DLL application.
//

#include "StdAfx.h"

#include "Blue.h"
#include "BluePaths.h"
#include "BlueOS.h"
#include "ResourceLoading.h"
#include "BlueSocketLogger.h"
#if BLUE_WITH_PYTHON
#include "BluePyCpp.h"
#include <Scheduler.h>
#include <Find.h>
#include "version.h"
#include "BluePython.h"
#endif

ICrashReporter* BeCrashes{nullptr};

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


bool AttachToLogServer()
{
	bool result = false;
	if ( IsSocketLoggerConnected() )
	{
		CCP_LOG( "Socket logger is already attached" );
		result = true;
	}
	else if ( StartSocketLogger() )
	{
		CCP::RegisterLogEcho( &LogToSocketLogger, CCP::LOGTYPE_INFO, true, CCP::LOG_ECHO_REQUIRES_PRIVILEGE_CHECK );
		CCP_LOG( "Socket logger has been attached" );
		result = true;
	}
	else
	{
		CCP_LOG( "Failed to attach to socket logger" );
		result = false;
	}
	return result;
}

MAP_FUNCTION_AND_WRAP( "AttachToLogServer", AttachToLogServer, "Attaches to the log server" );

#if BLUE_WITH_PYTHON

PyObject* PyGetVersionChangelist(PyObject *self, PyObject* args)
{
	return PyUnicode_FromString(EVECHANGELIST);
}

PyObject* PyGetVersionBranch(PyObject *self, PyObject* args)
{
	return PyUnicode_FromString(EVEBRANCH);
}

MAP_FUNCTION("GetChangelist", PyGetVersionChangelist, "Reports the changelist of the blue library");
MAP_FUNCTION("GetBranch", PyGetVersionBranch, "Reports the branch of the blue library");

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
	if (!PyArg_ParseTuple(args, "U", &filename))
		return NULL;


	BluePy ufn(PyUnicode_FromObject(filename));
	if (!ufn) return 0;
#ifdef _WIN32
	HANDLE h = INVALID_HANDLE_VALUE;
	DWORD fileSize;
	BY_HANDLE_FILE_INFORMATION info;
	wchar_t* fileName = PyUnicode_AsWideCharString( ufn.o, NULL );
	if ( !fileName )
	{
		return nullptr;
	}
	{
		Ccp::PyAllowThreads _allow;
		for(int i = 0; i<10; i++) {
            h = CreateFileW(fileName,
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
		BluePy r(PyBytes_FromStringAndSize(0, fileSize));
		if (!r) {
			goto HERR;
		}
		DWORD read;
		{
            char *buffer = PyBytes_AsString(r);
			Ccp::PyAllowThreads _allow;
            BOOL success = ReadFile(h, buffer, fileSize, &read, 0);
			if (!success)
				goto HERR;

			CloseHandle(h);
			h = INVALID_HANDLE_VALUE;
		}
		if (read != fileSize)
		{
			PyErr_SetString( PyExc_RuntimeError, "Read short file" );
			return nullptr;
		}

		PyMem_Free( fileName );
		return r.Detach();
	}

HERR:
	PyMem_Free( fileName );
	PyWin32Error();
	if (h != INVALID_HANDLE_VALUE)
		CloseHandle(h);
	return 0;

#elif defined(__APPLE__)
	wchar_t* tmp = PyUnicode_AsWideCharString( ufn.o, nullptr );
	if ( !tmp )
	{
		return nullptr;
	}
	CW2A filenameStr( tmp );
	PyMem_Free( tmp );
	int f;
	long fileSize;
	{
		Ccp::PyAllowThreads _allow;
        f = open( filenameStr, O_RDONLY | O_SHLOCK );
		if( f >= 0 )
		{
            fileSize = lseek( f, 0, SEEK_END );
            lseek( f, 0, SEEK_SET );
        }
	}
    if( f < 0 )
    {
        return PyErr_SetFromErrnoWithFilename( PyExc_OSError, filenameStr );
    }

	BluePy r( PyBytes_FromStringAndSize( nullptr, Py_ssize_t( fileSize ) ) );
	if( !r )
	{
		close( f );
		return nullptr;
	}
	ssize_t bytes;
	{
		Ccp::PyAllowThreads _allow;
		bytes = read( f, PyBytes_AsString(r), fileSize );
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
	PyObject* ufn;
	Py_buffer buffer;
	if (!PyArg_ParseTuple(args, "Uy*", &ufn, &buffer))
		return NULL;

	if ( ! PyBuffer_IsContiguous( &buffer, 'A' ) ) {
		PyErr_SetString( PyExc_NotImplementedError, "AtomicFileWrite is not implemented for non-contiguous buffers" );
		return nullptr;
	}

#ifdef _WIN32

//	Py_ssize_t segcount;
	HANDLE h;
	wchar_t* fileName = PyUnicode_AsWideCharString( ufn, NULL );
	{
		Ccp::PyAllowThreads _allow;
		for(int i = 0; i<10; i++) {
            h = CreateFileW(fileName,
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


		//support only DWORD sizes yet
		DWORD written;
		BOOL success;
		{
			Ccp::PyAllowThreads _allow;
			success = WriteFile(h, buffer.buf, (DWORD)buffer.len, &written, 0);
			if (!success)
				goto HERR;
		}
		if (written != buffer.len) {
			PyErr_SetString(PyExc_IOError, "Wrote short file");
			goto HERR;
		}
	if (h != INVALID_HANDLE_VALUE)
		CloseHandle(h);
	PyMem_Free( fileName );
	Py_INCREF(Py_None);
	return Py_None;

HERR:
	PyWin32Error();
	PyMem_Free( fileName );
	if (h != INVALID_HANDLE_VALUE)
		CloseHandle(h);
	return 0;

#elif defined(__APPLE__)

	Py_UNICODE *fileName = PyUnicode_AsWideCharString(ufn, nullptr);
	CW2A filenameStr( reinterpret_cast<const wchar_t*>( fileName ) );
	int f;
	{
		Ccp::PyAllowThreads _allow;
		f = open( filenameStr, O_WRONLY | O_EXLOCK | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR );
    }
    if( f < 0 )
    {
        return PyErr_SetFromErrnoWithFilename( PyExc_OSError, filenameStr );
    }
	ON_BLOCK_EXIT( [&] { close( f ); } );
	ssize_t written;
	{
		 Ccp::PyAllowThreads _allow;
		 written = write( f, buffer.buf, buffer.len );
	}
	if( written != buffer.len )
	{
		 return PyErr_SetString( PyExc_IOError, "Wrote short file" ), nullptr;
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
	":raises: OSError, IOError\n"
	":param filename: path to file\n"
	":type filename: basestring\n"
	":rtype: str" );

MAP_FUNCTION(
	"AtomicFileWrite",
	PyAtomicFileWrite,
	"Writes an entire file atomically. Raises OSError, IOError.\n"
	":param filename: path to file\n"
	":type filename: basestring\n"
	":param contents: buffer containing file contents to write\n"
	":type contents: buffer\n"
	":rtype: None" );


MAP_FUNCTION_AND_WRAP(
	"FindRoute",
	FindRoute,
	"Searches for all routes from one object to another. A route is a sequence of getattr or index operations.\n"
	"This function is relatively heavy and should not be used in production code, but rather is indended for\n"
	"in-house tools.\n"
	"Function returns a list of all paths from one object to another, where each path is a list of tuples\n"
	"(parent, type, index) where parent is a parent blue object, type is 0 for getattr call and 1 for index call,\n"
	"index is either an attribute name or a list/dict key."
	":param from: blue object that is the start of the path\n"
	":param to: blue object that is the end of the path\n"
	":returns: list of paths between the objects\n"
	":rtype: list" );


MAP_FUNCTION_AND_WRAP(
	"FindFirstRoute",
	PyFindFirstRoute,
	"Searches for the first route from one object to another. A route is a sequence of getattr or index operations.\n"
	"This function is relatively heavy and should not be used in production code, but rather is indended for\n"
	"in-house tools.\n"
	"Function returns the first path it locates from one object to another, where each path is a list of tuples\n"
	"(parent, type, index) where parent is a parent blue object, type is 0 for getattr call and 1 for index call,\n"
	"index is either an attribute name or a list/dict key."
	":param from: blue object that is the start of the path\n"
	":param to: blue object that is the end of the path\n"
	":returns: path between the objects\n"
	":rtype: list" );


MAP_FUNCTION_AND_WRAP(
	"FindAllReferences",
	FindAllReferences,
	"Finds all references to all objects in an object tree. The result of the function is a dictionary with all\n"
	"objects in the tree as keys. Values of the dictionary are lists of references to the given object key.\n"
	"These references follow the same format as the result of FindRoute function: they are tuples\n"
	"(parent, key type, key) where parent is the \"parent\" object that has a reference to the key,\n"
	"key type is 0 for object attributes and 1 for indices (lists or dictionaries), key is either an\n"
	"attribute name or an index value.\n"
	":param root: the root of object tree\n"
	":rtype: Dict[IRoot, list]" );


MAP_FUNCTION_AND_WRAP(
	"FindInterface",
	FindInterface,
	"Searches though hierarhy of object for all objects implementing the given interface.\n"
	":param root: root object\n"
	":param iidName: interface name\n"
	":returns: list of objects that implements the given interface\n"
	":rtype: list" );


MAP_FUNCTION_AND_WRAP(
	"FindReference",
	FindReference,
	"Checks if the given destination object is referenced by any object in the hierarhy of the source object.\n"
	":param source: source object\n"
	":param dest: destination object\n" );

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

MAP_FUNCTION(
	"EnableDebuggerLogging",
	PyEnableDebuggerLogging,
	"Enables echoing of log to debugger output window\n"
	":param threshold: minimum severity level required for the message to be output\n"
	":type threshold: Optional[int]\n"
	":rtype: None" );
#endif
#endif

void BlueInitializeSocketLogger()
{
    CCP_LOG( "Connecting to socket logger" );

	if( StartSocketLogger() )
	{
		CCP::RegisterLogEcho( &LogToSocketLogger, CCP::LOGTYPE_INFO, true, CCP::LOG_ECHO_REQUIRES_PRIVILEGE_CHECK );
		CCP_LOG( "Socket logger has been attached" );
	}
	else
	{
		CCP_LOG( "Failed to attach to socket logger" );
	}
}

void BlueShutdownSocketLogger()
{
	CCP_LOG( "Shutting down socket logger" );
	FlushSocketLogger();
	StopSocketLogger();
}

void LogToCrashReporter( CcpLogChannel_t& logObject, CCP::LogType type, unsigned long userData, const char* message )
{
	static char buf[256]{};
	// Copy up to one byte less than the buffer to ensure the buffer is null terminated
	strncpy_s( buf, std::extent_v<decltype(buf)>, message, std::extent_v<decltype(buf)> - 1 );

	BeCrashes->SetCrashKeyValue( "lastLoggedError", buf );
}

void BlueSetCrashReporter( ICrashReporter* crashReporter )
{
	BeCrashes = crashReporter;
	CCP::RegisterLogEcho(LogToCrashReporter, CCP::LOGTYPE_ERR, true);
}

void BlueLogFuncChannel( CcpLogChannel_t& logObject, CCP::LogType type, unsigned long userData, const char* format, va_list args )
{
	LogFuncChannel_v( logObject, type, userData, format, args );
}

void BlueShowError()
{
	if ( ! BeOS )
	{
		fprintf( stderr, "Cannot show blue errors before blue was initialized" );
		return;
	}

	if( BeOS->GetError( 0 ) )
	{
		char* err{nullptr};
		BeOS->FormatError( &err );
		BeOS->SetError( BEFLUSH ); //output to logger
		BeOS->SetError( BECLEAR ); //clear it
	}
}

bool BlueSetSearchPaths( const std::vector<std::wstring>& searchPaths )
{
	for( const std::wstring& s : searchPaths )
	{
		size_t pos = s.find_first_of( L'=' );
		if( pos == std::wstring::npos )
		{
			CCP_LOGWARN( "Invalid path specification: %S", s.c_str() );
			continue;
		}

		std::wstring keyW = s.substr( 0, pos );
		std::wstring valueW = s.substr( pos + 1 );

		if( !BeIsSuccess( BePaths->SetSearchPathW( CW2A( keyW.c_str() ), valueW.c_str() ) ) )
		{
			return false;
		}
	}

	return true;
}

void BlueSetStartupArgs( const std::vector<std::wstring>& args )
{
	BeOS->SetStartupArgs( args );
}

bool BlueHasStartupArg( const std::wstring& name )
{
	return BeOS->HasStartupArg( name );
}

bool BlueRunStackless()
{
	return BeOS->RunStackless();
}

void BlueTerminate( int exitCode )
{
	BeOS->Terminate( exitCode );
}

bool BlueIsPackaged()
{
	return BeOS->IsPackaged();
}

void BlueGetInitTab( std::vector<_inittab>& inittab )
{
	BeOS->GetInitTab( inittab );
}

bool BlueConstructPathListFromManifest( std::vector<std::wstring>& pathList, bool verifyManifest )
{
	return BeOS->ConstructPathListFromManifest( pathList, verifyManifest );
}

void BlueResolvePathForWritingW( const std::wstring& path, std::wstring& resolved )
{
	resolved = BePaths->ResolvePathForWritingW( path );
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

	// We need to call RegisterClasses early so that BluePaths can be correctly constructed.
	BeClasses->RegisterClasses( BlueRegistration::GetClassRegs() );
}

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
		else if( PyLong_Check( code ) )
		{
			s_exitCode = int( PyLong_AsLong( code ) );
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
	exceptHookDef.ml_doc = "Patched sys.excepthook that records process exit code.\nPart of Blue exit patching";
	exceptHookDef.ml_flags = METH_VARARGS;
	exceptHookDef.ml_meth = &BlueExceptHook;
	exceptHookDef.ml_name = "excepthook";
	PyObject* fn = PyCFunction_New( &exceptHookDef, nullptr );
	PySys_SetObject( (char*)"excepthook", fn );
	Py_DecRef( fn );

	s_savedSysExit = PySys_GetObject( (char*)"exit" );

	static PyMethodDef exitDef;
	exitDef.ml_doc = "Patched sys.exit that records process exit code.\nPart of Blue exit patching";
	exitDef.ml_flags = METH_VARARGS;
	exitDef.ml_meth = &BlueExit;
	exitDef.ml_name = "exit";
	fn = PyCFunction_New( &exitDef, nullptr );
	PySys_SetObject( (char*)"exit", fn );
	Py_DecRef( fn );

	Py_DecRef( sysmodule );
}

}

PyMODINIT_FUNC BLUE_EXPORTED_INIT
	CCP_CONCATENATE( PyInit_blue, CCP_BUILD_FLAVOR ) (void)
{
	CCP_LOG( "Initializing Resource Loading" );
	BlueInitializeResourceLoading();

	CCP_LOG( "BeOS Startup" );
	if ( ! BeOS->Startup(0) )
	{
			return nullptr;
	}

	PatchPythonExit();

	auto blueModule = PyOS->BlueModule();
	return blueModule;
}


#endif
