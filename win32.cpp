//This file defines the blue.win32 module which exposes some quite useful win32 apis to the app


#include "StdAfx.h"

#if BLUE_WITH_PYTHON
#ifdef _WIN32

#include <winsock2.h>
#include <ntverp.h>
#include <Iphlpapi.h>
#include "win32.h"

#include "include/blue.h"
#include "include/IBluePython.h"
#include "include/TransGaming.h"

#include <shellapi.h>

#include <atlbase.h>
#include <psapi.h>
#include <shlobj.h> //shell32 api

#include <string>
#include <map>
#include <vector>

#include <DbgHelp.h>
#include <bits.h>

#include <CcpUtils/PyCpp.h>
using Ccp::PyAllowThreads;


// Need some structure definitions for the extended TCP functionality.  Normally defined in Iphlpapi.h,
// but does not exist in the WinSDK versions prior to Vista (Win6)
//
#if !defined(VER_PRODUCTMAJORVERSION) ||  VER_PRODUCTMAJORVERSION < 6

	typedef enum _TCP_TABLE_CLASS {
		TCP_TABLE_BASIC_LISTENER,
		TCP_TABLE_BASIC_CONNECTIONS,
		TCP_TABLE_BASIC_ALL,
		TCP_TABLE_OWNER_PID_LISTENER,
		TCP_TABLE_OWNER_PID_CONNECTIONS,
		TCP_TABLE_OWNER_PID_ALL,
		TCP_TABLE_OWNER_MODULE_LISTENER,
		TCP_TABLE_OWNER_MODULE_CONNECTIONS,
		TCP_TABLE_OWNER_MODULE_ALL
	} TCP_TABLE_CLASS, *PTCP_TABLE_CLASS;

	typedef enum _TCP_BOOLEAN_OPTIONAL {
		TcpBoolOptDisabled = 0,
		TcpBoolOptEnabled,
		TcpBoolOptUnchanged = -1
	} TCP_BOOLEAN_OPTIONAL, *PTCP_BOOLEAN_OPTIONAL;

	typedef enum {
		TcpConnectionEstatsSynOpts,
		TcpConnectionEstatsData,
		TcpConnectionEstatsSndCong,
		TcpConnectionEstatsPath,
		TcpConnectionEstatsSendBuff,
		TcpConnectionEstatsRec,
		TcpConnectionEstatsObsRec,
		TcpConnectionEstatsBandwidth,
		TcpConnectionEstatsFineRtt,
		TcpConnectionEstatsMaximum,
	} TCP_ESTATS_TYPE, *PTCP_ESTATS_TYPE;

	typedef struct _TCP_ESTATS_DATA_RW_v0 {
		BOOLEAN EnableCollection;
	} TCP_ESTATS_DATA_RW_v0, *PTCP_ESTATS_DATA_RW_v0;

	typedef struct _TCP_ESTATS_DATA_ROD_v0 {
		ULONG64 DataBytesOut;
		ULONG64 DataSegsOut;
		ULONG64 DataBytesIn;
		ULONG64 DataSegsIn;
		ULONG64 SegsOut;
		ULONG64 SegsIn;
		ULONG SoftErrors;
		ULONG SoftErrorReason;
		ULONG SndUna;
		ULONG SndNxt;
		ULONG SndMax;
		ULONG64 ThruBytesAcked;
		ULONG RcvNxt;
		ULONG64 ThruBytesReceived;
	} TCP_ESTATS_DATA_ROD_v0, *PTCP_ESTATS_DATA_ROD_v0;

	typedef struct _TCP_ESTATS_SND_CONG_ROD_v0 {
		ULONG SndLimTransRwin;
		ULONG SndLimTimeRwin;
		SIZE_T SndLimBytesRwin;
		ULONG SndLimTransCwnd;
		ULONG SndLimTimeCwnd;
		SIZE_T SndLimBytesCwnd;
		ULONG SndLimTransSnd;
		ULONG SndLimTimeSnd;
		SIZE_T SndLimBytesSnd;
		ULONG SlowStart;
		ULONG CongAvoid;
		ULONG OtherReductions;
		ULONG CurCwnd;
		ULONG MaxSsCwnd;
		ULONG MaxCaCwnd;
		ULONG CurSsthresh;
		ULONG MaxSsthresh;
		ULONG MinSsthresh;
	} TCP_ESTATS_SND_CONG_ROD_v0, *PTCP_ESTATS_SND_CONG_ROD_v0;

	typedef struct _TCP_ESTATS_SND_CONG_RW_v0 {
		BOOLEAN EnableCollection;
	} TCP_ESTATS_SND_CONG_RW_v0, *PTCP_ESTATS_SND_CONG_RW_v0;

	typedef struct _TCP_ESTATS_PATH_ROD_v0 {
		ULONG FastRetran;
		ULONG Timeouts;
		ULONG SubsequentTimeouts;
		ULONG CurTimeoutCount;
		ULONG AbruptTimeouts;
		ULONG PktsRetrans;
		ULONG BytesRetrans;
		ULONG DupAcksIn;
		ULONG SacksRcvd;
		ULONG SackBlocksRcvd;
		ULONG CongSignals;
		ULONG PreCongSumCwnd;
		ULONG PreCongSumRtt;
		ULONG PostCongSumRtt;
		ULONG PostCongCountRtt;
		ULONG EcnSignals;
		ULONG EceRcvd;
		ULONG SendStall;
		ULONG QuenchRcvd;
		ULONG RetranThresh;
		ULONG SndDupAckEpisodes;
		ULONG SumBytesReordered;
		ULONG NonRecovDa;
		ULONG NonRecovDaEpisodes;
		ULONG AckAfterFr;
		ULONG DsackDups;
		ULONG SampleRtt;
		ULONG SmoothedRtt;
		ULONG RttVar;
		ULONG MaxRtt;
		ULONG MinRtt;
		ULONG SumRtt;
		ULONG CountRtt;
		ULONG CurRto;
		ULONG MaxRto;
		ULONG MinRto;
		ULONG CurMss;
		ULONG MaxMss;
		ULONG MinMss;
		ULONG SpuriousRtoDetections;
	} TCP_ESTATS_PATH_ROD_v0, *PTCP_ESTATS_PATH_ROD_v0;

	typedef struct _TCP_ESTATS_PATH_RW_v0 {
		BOOLEAN EnableCollection;
	} TCP_ESTATS_PATH_RW_v0, *PTCP_ESTATS_PATH_RW_v0;

	typedef struct _TCP_ESTATS_REC_ROD_v0 {
		ULONG CurRwinSent;
		ULONG MaxRwinSent;
		ULONG MinRwinSent;
		ULONG LimRwin;
		ULONG DupAckEpisodes;
		ULONG DupAcksOut;
		ULONG CeRcvd;
		ULONG EcnSent;
		ULONG EcnNoncesRcvd;
		ULONG CurReasmQueue;
		ULONG MaxReasmQueue;
		SIZE_T CurAppRQueue;
		SIZE_T MaxAppRQueue;
		UCHAR WinScaleSent;
	} TCP_ESTATS_REC_ROD_v0, *PTCP_ESTATS_REC_ROD_v0;

	typedef struct _TCP_ESTATS_REC_RW_v0 {
		BOOLEAN EnableCollection;
	} TCP_ESTATS_REC_RW_v0, *PTCP_ESTATS_REC_RW_v0;

	typedef struct _TCP_ESTATS_BANDWIDTH_RW_v0 {
		TCP_BOOLEAN_OPTIONAL EnableCollectionOutbound;
		TCP_BOOLEAN_OPTIONAL EnableCollectionInbound;
	} TCP_ESTATS_BANDWIDTH_RW_v0, *PTCP_ESTATS_BANDWIDTH_RW_v0;

	typedef struct _TCP_ESTATS_BANDWIDTH_ROD_v0 {
		ULONG64 OutboundBandwidth;
		ULONG64 InboundBandwidth;
		ULONG64 OutboundInstability;
		ULONG64 InboundInstability;
		BOOLEAN OutboundBandwidthPeaked;
		BOOLEAN InboundBandwidthPeaked;
	} TCP_ESTATS_BANDWIDTH_ROD_v0, *PTCP_ESTATS_BANDWIDTH_ROD_v0;


#endif



namespace {

//A Class to softload stuff.
class SoftLoader {
public:
	void Init() {}

	SoftLoader() {
#define LOAD(L, N) m##N = ( N##_t ) Load(L, #N)
	LOAD("kernel32.dll", GetThreadTimes);
	LOAD("kernel32.dll", GetProcessTimes);
	LOAD("kernel32.dll", GetProcessWorkingSetSize);
	LOAD("kernel32.dll", SetProcessWorkingSetSize);
	LOAD("kernel32.dll", GetProcessHandleCount);
	LOAD("kernel32.dll", GetProcessIoCounters);
	LOAD("kernel32.dll", GetSystemTimeAdjustment);
	LOAD("kernel32.dll", GetNativeSystemInfo);
	LOAD("kernel32.dll", SetThreadIdealProcessor);
	LOAD("kernel32.dll", IsWow64Process);
	LOAD("kernel32.dll", GlobalMemoryStatusEx);
	LOAD("WtsApi32.dll", WTSRegisterSessionNotification);
	LOAD("WtsApi32.dll", WTSUnRegisterSessionNotification);
	LOAD("Psapi.dll", GetProcessMemoryInfo);
	LOAD("Iphlpapi.dll", GetPerTcpConnectionEStats);
	LOAD("Iphlpapi.dll", SetPerTcpConnectionEStats);
	LOAD("Iphlpapi.dll", GetExtendedTcpTable);
#undef LOAD
}

	static void *Load(const char *dll, const char *function)
	{
		HMODULE lLib = LoadLibrary(dll);
		if (lLib == NULL)
			return 0;
		HMODULE h = GetModuleHandle(dll); //we assume the module is already loaded
			if (!h) return 0;
		return GetProcAddress(h, function);
	}

//typedefs
typedef BOOL (WINAPI * GetThreadTimes_t)(HANDLE, LPFILETIME, LPFILETIME, LPFILETIME, LPFILETIME);
typedef BOOL (WINAPI *GetProcessTimes_t)(HANDLE, LPFILETIME, LPFILETIME, LPFILETIME, LPFILETIME);
typedef BOOL (WINAPI *GetProcessMemoryInfo_t)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
typedef BOOL (WINAPI *GetProcessWorkingSetSize_t)(HANDLE, PSIZE_T, PSIZE_T);
typedef BOOL (WINAPI *SetProcessWorkingSetSize_t)(HANDLE, SIZE_T, SIZE_T);
typedef BOOL (WINAPI *GetProcessHandleCount_t)(HANDLE, PDWORD);
typedef BOOL (WINAPI *GetProcessIoCounters_t)(HANDLE, PIO_COUNTERS);
typedef BOOL (WINAPI *GetSystemTimeAdjustment_t)(PDWORD, PDWORD, PBOOL);
typedef void (WINAPI *GetNativeSystemInfo_t)(LPSYSTEM_INFO);
typedef DWORD (WINAPI *SetThreadIdealProcessor_t)(HANDLE, DWORD);
typedef BOOL (WINAPI *GlobalMemoryStatusEx_t)(LPMEMORYSTATUSEX);
typedef BOOL (WINAPI *IsWow64Process_t)(HANDLE, PBOOL);
typedef BOOL (WINAPI *WTSRegisterSessionNotification_t)(HWND, DWORD);
typedef BOOL (WINAPI *WTSUnRegisterSessionNotification_t)(HWND);
typedef DWORD (WINAPI *GetExtendedTcpTable_t)(PVOID, PDWORD, BOOL, ULONG, TCP_TABLE_CLASS, ULONG);    
typedef ULONG (WINAPI *SetPerTcpConnectionEStats_t)(PMIB_TCPROW, DWORD, PUCHAR, ULONG, ULONG, ULONG);
typedef ULONG (WINAPI *GetPerTcpConnectionEStats_t) (PMIB_TCPROW, DWORD, PUCHAR, ULONG, ULONG,
												     PUCHAR, ULONG, ULONG, PUCHAR, ULONG, ULONG);



#define DEF(N) \
	N##_t m##N;\
	N##_t N() {Init(); return m##N;}

DEF(GetThreadTimes)
DEF(GetProcessTimes)
DEF(GetProcessMemoryInfo)
DEF(GetProcessWorkingSetSize)
DEF(SetProcessWorkingSetSize)
DEF(GetProcessHandleCount)
DEF(GetProcessIoCounters)
DEF(GetSystemTimeAdjustment)
DEF(GetNativeSystemInfo)
DEF(SetThreadIdealProcessor)
DEF(GlobalMemoryStatusEx)
DEF(WTSRegisterSessionNotification)
DEF(WTSUnRegisterSessionNotification)
DEF(IsWow64Process)
DEF(GetExtendedTcpTable)
DEF(SetPerTcpConnectionEStats)
DEF(GetPerTcpConnectionEStats)
#undef DEF
};
SoftLoader *loader = 0;


//Python function prototypes:
#define PROTO(N) PyObject *Py##N(PyObject *self, PyObject *args);
PROTO(GetSystemDirectory)
PROTO(GetSystemWow64Directory)
PROTO(GetFileVersionInfo)
PROTO(MessageBox)

//debug functions
PROTO(DebugBreak)
PROTO(DebugCrash)
PROTO(OutputDebugString)

//clipboard functions
PROTO(GetClipboardData)
PROTO(GetClipboardUnicode)
PROTO(SetClipboardData)

//specialfile dump functions
PROTO(AtomicFileRead)
PROTO(AtomicFileWrite)

//registry stuff
PROTO(RegistryGetValue)
PROTO(RegistryGetSubkeys)
PROTO(RegistryGetValueNames)
PROTO(RegistryGetValues)

//custom routines
PROTO(SetNamedEvent)
PROTO(ShellExecute)

//vtune api functions
PROTO(VTPause)
PROTO(VTResume)

//memory and time queries
PROTO(GetThreadTimes)
PROTO(GetProcessTimes)
PROTO(GlobalMemoryStatus)
PROTO(GetProcessMemoryInfo)
PROTO(GetProcessWorkingSetSize)
PROTO(SetProcessWorkingSetSize)
PROTO(GetProcessHandleCount)
PROTO(GetProcessIoCounters)
PROTO(GetSystemTime)
PROTO(GetSystemTimeAsFileTime)
PROTO(GetTickCount)
PROTO(GetSystemTimeAdjustment)
PROTO(QueryPerformanceCounter)
PROTO(QueryPerformanceFrequency)

//network queries
PROTO(ToggleTcpEStats)
PROTO(GetProcessTcpEStats)

//process info and stuff
PROTO(GetSystemInfo)
PROTO(GetNativeSystemInfo)
PROTO(IsWow64Process)
PROTO(IsTransgaming)
PROTO(TGGetOS)
PROTO(TGGetSystemInfo)
PROTO(GetProcessBits)
PROTO(GetSystemBits)
PROTO(GetVersionEx)
PROTO(GetProcessAffinityMask)
PROTO(SetProcessAffinityMask)
PROTO(SetThreadAffinityMask)
PROTO(SetThreadIdealProcessor)
PROTO(IsProcessorFeaturePresent)

//IP stuff
PROTO(GetAdaptersInfo)

//Debug Heap stuff
PROTO(_CrtSetAllocHook)

PROTO(MiniDumpWriteDump)

//Shell32
PROTO(SHGetFolderPath)

//Bits, Background Intelligent Transfer Service
PROTO(BitsQueueDownload)
PROTO(BitsGetDownloadStatus)
PROTO(BitsAction)
PROTO(InitializeCom)
PROTO(UnInitializeCom)
PROTO(GetWindowsServiceStatus)

//a single test function
PROTO(Test)

//Windows session change notification
PROTO(WTSRegisterSessionNotification)
PROTO(WTSUnRegisterSessionNotification)

#undef PROTO

//pythoh method definitions
PyMethodDef methods[] = {
#define DEF(N) {#N, Py##N, METH_VARARGS},
DEF(GetSystemDirectory)
DEF(GetSystemWow64Directory)
DEF(GetFileVersionInfo)
{ "MessageBox", PyMessageBox, METH_VARARGS, 
	"Display a message box to the user, with buttons" 
	"\n returns an integer that indicates which button was pressed, see"
	"\n Win32 API/MessageBox function documentation for meaning of these values"
	"\n"
	"\nArguments:"
	"\ntext - the message to be displayed, needs to be separated by linefeeds if"
	"\n       it doesn't fit on one line"
	"\ntitle - the dialog box title"
	"\nflags - an integer that specifies what buttons are displayed to the user and"
	"\n        what icon is used.  See Win32 API/MessageBox function documentation"
	"\n        for details (default is question mark icon with Yes & No buttons)" 
},
DEF(DebugBreak)
DEF(DebugCrash)
DEF(OutputDebugString)
DEF(GetClipboardData)
DEF(GetClipboardUnicode)
DEF(SetClipboardData)
DEF(AtomicFileRead)
DEF(AtomicFileWrite)
DEF(RegistryGetValue)
DEF(RegistryGetSubkeys)
DEF(RegistryGetValueNames)
DEF(RegistryGetValues)
DEF(SetNamedEvent)
DEF(ShellExecute)
DEF(VTPause)
DEF(VTResume)
DEF(GetThreadTimes)
DEF(GetProcessTimes)
DEF(GlobalMemoryStatus)
DEF(GetProcessMemoryInfo)
DEF(GetProcessWorkingSetSize)
DEF(SetProcessWorkingSetSize)
DEF(GetProcessHandleCount)
DEF(GetProcessIoCounters)
DEF(GetSystemTime)
DEF(GetSystemTimeAsFileTime)
DEF(GetTickCount)
DEF(GetSystemTimeAdjustment)
DEF(QueryPerformanceCounter)
DEF(QueryPerformanceFrequency)
DEF(ToggleTcpEStats)
DEF(GetProcessTcpEStats)
DEF(GetSystemInfo)
DEF(GetNativeSystemInfo)
DEF(IsWow64Process)
DEF(IsTransgaming)
DEF(TGGetOS)
DEF(TGGetSystemInfo)
DEF(GetProcessBits)
DEF(GetSystemBits)
DEF(GetVersionEx)
DEF(GetProcessAffinityMask)
DEF(SetProcessAffinityMask)
DEF(SetThreadAffinityMask)
DEF(SetThreadIdealProcessor)
DEF(IsProcessorFeaturePresent)
DEF(GetAdaptersInfo)
DEF(_CrtSetAllocHook)
DEF(MiniDumpWriteDump)
DEF(SHGetFolderPath)

DEF(BitsQueueDownload)
DEF(BitsGetDownloadStatus)
DEF(BitsAction)
DEF(InitializeCom)
DEF(UnInitializeCom)
DEF(GetWindowsServiceStatus)

DEF(Test)
DEF(WTSRegisterSessionNotification)
DEF(WTSUnRegisterSessionNotification)
#undef DEF
{0}
};

//heapq stuff, here for the time being
PyObject *PyHeapPush(PyObject *self, PyObject *args);
PyObject *PyHeapPop(PyObject *self, PyObject *args);
PyObject *PyHeapify(PyObject *self, PyObject *args);
PyObject *PyHeapReplace(PyObject *self, PyObject *args);
PyObject *PyHeapSort(PyObject *self, PyObject *args);
PyObject *PyHeapCheck(PyObject *self, PyObject *args);

PyMethodDef heapqmethods[] = {
	{"heappush", PyHeapPush, METH_VARARGS, "push an element onto heap"},
	{"heappop", PyHeapPop, METH_VARARGS, "pop an element off heap"},
	{"heapify", PyHeapify, METH_VARARGS, "put list inn heap order"},
	{"heapreplace", PyHeapReplace, METH_VARARGS, "pop and subsequently push an item"},
	{"heapsort", PyHeapSort, METH_VARARGS, "sort a list in heap order (large to small)"},
	{"heapcheck", PyHeapCheck, METH_VARARGS, "Test the heap property"},
	{0}
};

	
PyObject *PyGetSystemDirectory(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL; 
	TCHAR szPath[MAX_PATH];
	if (!GetSystemDirectory(szPath, MAX_PATH))
		return PyWin32Error("GetSystemDirectory");
	return PyString_FromString(szPath);
}

PyObject* PyGetSystemWow64Directory( PyObject* self, PyObject* args )
{
	if( !PyArg_ParseTuple( args, "" ) )
	{
		return NULL;
	}
	TCHAR szPath[MAX_PATH];
	if( !GetSystemWow64Directory( szPath, MAX_PATH ) )
	{
		return PyWin32Error( "GetSystemWow64Directory" );
	}
	return PyString_FromString( szPath );
}

PyObject *PyGetFileVersionInfo(PyObject *self, PyObject *args)
{
	char *fileName;
	if (!PyArg_ParseTuple(args, "s", &fileName))
		return NULL;

	DWORD dwHandle;
    DWORD size = GetFileVersionInfoSize( fileName, &dwHandle );
	if (!size)
		return PyWin32Error();
		
	char *buffer = CCP_NEW("PyGetFileVersionInfo/buffer") char[size];
	if (!GetFileVersionInfo(fileName, 0, size, buffer)) {
		CCP_DELETE[] buffer;
		return PyWin32Error();
	}

	//now, get the result
	BluePyDict d(0);

	VS_FIXEDFILEINFO* pVersion;
	UINT len;
	if (VerQueryValue(buffer, TEXT("\\"), (VOID**)&pVersion, &len) && pVersion && len) {
		BluePyDict d1(0);
		LONG64 val = (LONG64)pVersion->dwFileVersionMS << 32 | pVersion->dwFileVersionLS;
		d1.Set("FileVersion", BluePy(PyLong_FromUnsignedLongLong(val)));

		val = (LONG64)pVersion->dwProductVersionMS << 32 | pVersion->dwProductVersionLS;
		d1.Set("ProductVersion", BluePy(PyLong_FromUnsignedLongLong(val)));

		val = (LONG64)pVersion->dwFileDateMS << 32 | pVersion->dwFileDateLS;
		d1.Set("FileDate", BluePy(PyLong_FromUnsignedLongLong(val)));

		d1.Set("FileFlagsMask", BluePy(PyLong_FromUnsignedLong(pVersion->dwFileFlagsMask)));

		d1.Set("FileFlags", BluePy(PyLong_FromUnsignedLong(pVersion->dwFileFlags)));

		d1.Set("FileOS", BluePy(PyLong_FromUnsignedLong(pVersion->dwFileOS)));

		d1.Set("FileType", BluePy(PyLong_FromUnsignedLong(pVersion->dwFileType)));
		d1.Set("FileSubtype", BluePy(PyLong_FromUnsignedLong(pVersion->dwFileSubtype)));
		d.Set("\\", d1);
	}

	char *keys[] = {"Comments", "InternalName", "ProductName", "CompanyName", "LegalCopyright", "ProductVersion", 
		"FileDescription", "LegalTrademarks", "LegalTrademarks", "PrivateBuild", 
		"FileVersion", "FileVersion", "SpecialBuild"};

	struct LANGANDCODEPAGE {
		WORD wLanguage;
		WORD wCodePage;
	} *lpTranslate;
	if (VerQueryValue(buffer, TEXT("\\VarFileInfo\\Translation"), (VOID**)&lpTranslate, &len) && lpTranslate && len) {
		for(unsigned int i = 0; i<(len/sizeof(LANGANDCODEPAGE)); i++) {
			for(int j = 0; j<(sizeof(keys)/sizeof(*keys)); j++) {
				char key[256];
				wsprintf(key, "\\StringFileInfo\\%04x%04x\\%s", lpTranslate[i].wLanguage, lpTranslate[i].wCodePage, keys[j]);

				char *result = 0;
				if (VerQueryValue(buffer, key, (void**)&result, &len) && result && len) 
					d.Set(key, BluePy(PyString_FromString((char*)result)));
			}
		}
	}
	CCP_DELETE[] buffer;
	return d.Detach();
}


PyObject *PyOutputDebugString(PyObject *self, PyObject *args)
{
	PyObject *data;
	if (!PyArg_ParseTuple(args, "O:OutputDebugString", &data))
		return NULL;
	if (PyString_Check(data))
		OutputDebugStringA(PyString_AS_STRING(data));
	else if (PyUnicode_Check(data))
		OutputDebugStringW(PyUnicode_AS_UNICODE(data));
	else
		return PyErr_SetString(PyExc_TypeError, "argument must be string or unicode"), 0;
	Py_INCREF(Py_None);
	return Py_None;
}


PyObject *PyDebugBreak(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL; 
	DebugBreak();
	Py_INCREF(Py_None);
	return Py_None;
}


PyObject *PyDebugCrash(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	__int64 *foo = 0;
	*foo = 123;	
	Py_INCREF(Py_None);
	return Py_None;
}


PyObject* PyMessageBox( PyObject *self, PyObject* args )
{
	PyObject* textObjects[2];
	int flags = MB_ICONQUESTION | MB_YESNO;

	if( !PyArg_ParseTuple( args, "OO|i:MessageBox", textObjects, textObjects+1, &flags ) )
	{
		return NULL;
	}

	for( int i = 0; i < 2; ++i )
	{
		PyObject* obj = textObjects[i];
		// We convert Python string objects to unicode objects - so that we just have
		// to handle unicode strings and can support the situation where just one of
		// title and text arguments is provided as unicode.  Since the PyUnicode_From methods
		// create new objects we add a reference when no conversion is done, so that we
		// can always safely decrement the reference at the end of this function.
		if( PyString_Check( obj ) )
		{
			// Replacing the object in textObjects is safe because the object provided by 
			// ParseTuple is just a borrowed reference - we don't need to track it.  The new 
			// object created here needs to be dec-ref'ed.
			textObjects[i] = PyUnicode_FromString( PyString_AsString( obj ) );
		}
		else
		{
			// Here we add a reference to the originally borrowed reference so that
			// we can just always dec-ref the textObjects items, regardless of where
			// they came from.
			Py_INCREF( obj );
		}
	}

	const wchar_t* text = PyUnicode_AsUnicode( textObjects[0] );
	const wchar_t* title = PyUnicode_AsUnicode( textObjects[1] );
	if( !text || !title )
	{
		PyErr_SetString( PyExc_TypeError, "String or unicode arguments expected" );
		return NULL;
	}

	int ret = ::MessageBoxW( NULL, text, title, flags );
	// OK, we're done using the unicode strings, we can safely release our references
	// to the Python objects ...
	for( int i = 0; i < 2; ++i )
	{
		Py_DECREF( textObjects[i] );
	}
	return PyInt_FromLong( ret );
}


//--------------------------------------------------------------------
// GetClipboardData
//--------------------------------------------------------------------
PyObject* PyGetClipboardData(PyObject *self, PyObject* args)
{
	int format = CF_TEXT;
	if (!PyArg_ParseTuple(args, "|i", &format))
		return NULL;

	if (!OpenClipboard(NULL))
		return PyWin32Error();

	if (!IsClipboardFormatAvailable(format)) {
		CloseClipboard();
		Py_INCREF(Py_None);
		return Py_None;
	}
	
	HANDLE hdata = GetClipboardData(format);
	if (!hdata) goto error;

	SIZE_T size = GlobalSize(hdata);
	if (!size) goto error;
		
	char* string = (char*)GlobalLock(hdata);
	if (!string) goto error;
		
	PyObject *result;
	if (format == CF_TEXT)
		result = PyString_FromString(string);
	else
		result = PyString_FromStringAndSize(string, size);
	GlobalUnlock(string);
	CloseClipboard();
	return result;

error:
	CloseClipboard();
	return PyWin32Error();
}


PyObject* PyGetClipboardUnicode(PyObject *self, PyObject *args)
{
	const int format = CF_UNICODETEXT;
	if (!PyArg_ParseTuple(args, ""))
		return NULL;

	if (!OpenClipboard(NULL))
		return PyWin32Error();

	if (!IsClipboardFormatAvailable(format)) {
		CloseClipboard();
		Py_INCREF(Py_None);
		return Py_None;
	}
	
	HANDLE hdata = GetClipboardData(format);
	if (!hdata) goto error;
	
	WCHAR* string = (WCHAR*)GlobalLock(hdata);
	if (!string) goto error;
		
	PyObject *result = PyUnicode_FromWideChar(string, wcslen(string));
	GlobalUnlock(string);
	CloseClipboard();
	return result;

error:
	CloseClipboard();
	return PyWin32Error();
}

//--------------------------------------------------------------------
// SetClipboardData
//--------------------------------------------------------------------
PyObject* PySetClipboardData(PyObject *self, PyObject* args)
{
	PyObject *data;
	int format = CF_TEXT;

	if (!PyArg_ParseTuple(args, "O|i", &data, &format))
		return NULL;
	if (!PyString_Check(data) && !PyUnicode_Check(data))
		return PyErr_SetString(PyExc_TypeError, "string or unicode object required"), 0;

	HGLOBAL hdata;
	if (PyString_Check(data)) {
		char *str;
		Py_ssize_t len;
		PyString_AsStringAndSize(data, &str, &len);

		hdata= GlobalAlloc(GMEM_MOVEABLE, len+1);
		if (!hdata)
			return PyWin32Error();
		char* dest = (char*)GlobalLock(hdata);
		memcpy(dest, str, len);
		dest[len] = '\0';
		GlobalUnlock(dest);
	} else {
		format = CF_UNICODETEXT;
		Py_ssize_t len = PyUnicode_GET_SIZE(data);
		hdata= GlobalAlloc(GMEM_MOVEABLE, (len+1)*sizeof(WCHAR));
		if (!hdata)
			return PyWin32Error();

		WCHAR* dest = (WCHAR*)GlobalLock(hdata);
		PyUnicode_AsWideChar((PyUnicodeObject*)data, dest, len);
		dest[len] = 0;
		GlobalUnlock(dest);
	}
	
	if (!OpenClipboard(NULL)) {
		GlobalDiscard(hdata);
		return PyWin32Error();
	}
	EmptyClipboard();
	SetClipboardData(format, hdata);
	CloseClipboard();

	Py_INCREF(Py_None);
	return Py_None;
}


//--------------------------------------------------------------------
// AtomicFileRead and Write
// The atomicity is guaratneed by the OS locking thingie, so we can
// release the GIL.
//--------------------------------------------------------------------
PyObject* PyAtomicFileRead(PyObject *self, PyObject* args)
{
	PyObject *filename;
	if (!PyArg_ParseTuple(args, "O!", &PyBaseString_Type, &filename))
		return NULL;
	
	
	BluePy ufn(PyUnicode_FromObject(filename));
	if (!ufn) return 0;
	
	HANDLE h = INVALID_HANDLE_VALUE;
	DWORD fileSize;
	BY_HANDLE_FILE_INFORMATION info;
	{
		PyAllowThreads _allow;
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
			PyAllowThreads _allow;
			BOOL success = ReadFile(h, PyString_AsString(r), fileSize, &read, 0);
			if (!success)
				goto HERR;
				
			CloseHandle(h);
			h = INVALID_HANDLE_VALUE;
		}
		if (read != fileSize)
			return PyErr_SetString(PyExc_RuntimeError, "Read short file"), 0;
		
		BluePy fileInfo(Py_BuildValue(
			"{s:i,s:K,s:K,s:K,s:i,s:i,s:i,s:i,s:i}",
			"dwFileAttributes", info.dwFileAttributes,
			"ftCreationTime", *(unsigned __int64*)&info.ftCreationTime,
			"ftLastAccessTime", *(unsigned __int64*)&info.ftLastAccessTime,
			"ftLastWriteTime", *(unsigned __int64*)&info.ftLastWriteTime,
			"dwVolumeSerialNumber", info.dwVolumeSerialNumber,
			"nFileSizeHigh", info.nFileSizeHigh, 
			"nFileSizeLow", info.nFileSizeLow,
			"nNumberOfLinks", info.nNumberOfLinks,
			"nFileIndexHigh", info.nFileIndexHigh,
			"nFileIndexLow", info.nFileIndexLow
			));
		if (!fileInfo)
			return 0;
		return Py_BuildValue("(O,O)", r.o, fileInfo.o);
	}

HERR:
	PyWin32Error();
	if (h != INVALID_HANDLE_VALUE)
		CloseHandle(h);
	return 0;
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
		return PyErr_SetString(PyExc_TypeError, "expected a buffer object"), 0;
	
	HANDLE h;
	{
		PyAllowThreads _allow;		
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
			PyAllowThreads _allow;		
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
}

/////////////////////////////////////////////
// Registry functions.
struct rootKeys {
	const char *name;
	HKEY value;
};
static const rootKeys registryRoots[] = {
	{"HKEY_CLASSES_ROOT", HKEY_CLASSES_ROOT},
	{"HKEY_CURRENT_CONFIG", HKEY_CURRENT_CONFIG},
	{"HKEY_CURRENT_USER", HKEY_CURRENT_USER},
	{"HKEY_LOCAL_MACHINE", HKEY_LOCAL_MACHINE},
	{"HKEY_USERS", HKEY_USERS},
	{"HKEY_PERFORMANCE_DATA", HKEY_PERFORMANCE_DATA},
	{"HKEY_DYN_DATA", HKEY_DYN_DATA},
	{0}
};


static bool GetHKey(HKEY &hKey, CRegKey &cKey, const char *keyName, REGSAM access)
{
	size_t rootLen = strlen(keyName);
	const char *backslash = strchr(keyName, '\\');
	if (backslash)
		rootLen = backslash - keyName;

	//First, get the root handle
	const rootKeys *rkey;
	for(rkey = registryRoots; rkey->name; rkey++)
		if (!strncmp(rkey->name, keyName, rootLen))
			break;
	if (!rkey->name)
		return PyErr_SetString(PyExc_TypeError, "invalid root"), false;
	hKey = rkey->value;
	
	//if there is more
	if (backslash && strlen(backslash+1)) {
		LONG result = cKey.Open(rkey->value, backslash+1, access);
		if (result != ERROR_SUCCESS)
			return PyErr_SetFromWindowsErr(result), false;
		hKey = cKey;
	}
	return true;
}

static PyObject *ValueToPython(const char *buffer, int valueLen, DWORD valueType)
{
	PyObject *resultO = 0;
	switch(valueType) {
	case REG_BINARY:
		resultO = PyString_FromStringAndSize(buffer, valueLen);
		break;
	case REG_DWORD:
//	case REG_DWORD_LITTLE_ENDIAN:
		resultO = PyInt_FromLong(*(DWORD*)buffer);
		break;
	case REG_DWORD_BIG_ENDIAN: {
		union {
		DWORD r;
			char p[4];
		};
		p[0] = buffer[3];
		p[1] = buffer[2];
		p[2] = buffer[1];
		p[3] = buffer[0];
		resultO = PyInt_FromLong(r);
		break; }
	case REG_SZ:
	case REG_EXPAND_SZ:
		resultO = PyString_FromString(buffer);
		break;
	case REG_NONE:
		resultO = Py_None;
		break;
	case REG_QWORD:
	//case REG_QWORD_LITTLE_ENDIAN:
		resultO = PyLong_FromLongLong(*(__int64*)buffer);
		break;
	case REG_MULTI_SZ: {
		resultO = PyList_New(0);
		if (!resultO) {
			return 0;
		}
		const char *start = buffer, *next;
		for(;;) {
			next = strchr(start, 0);
			if (next==start)
				break;
			PyObject *str = PyString_FromString(start);
			if (!str) {
				Py_DECREF(resultO);
				return 0;
			}
			int r = PyList_Append(resultO, str);
			Py_DECREF(str);
			if (r) {
				Py_DECREF(resultO);
				return 0;
			}
			start = next+1;
		}
		break;}
	default:
		resultO = PyString_FromStringAndSize(buffer, valueLen);
	}
	return resultO;
}


PyObject *PyRegistryGetValue(PyObject *self, PyObject*args)
{
	const char *keyName, *valueName;
    int b64RegRead = 0;
    int flag64BitReg = 0;
    if (!PyArg_ParseTuple(args, "ss|i", &keyName, &valueName, &b64RegRead))
        return NULL;
    if (b64RegRead != 0)
        flag64BitReg = KEY_WOW64_64KEY;

	HKEY hKey;
	CRegKey cKey;
	if (!GetHKey(hKey, cKey, keyName, KEY_QUERY_VALUE | flag64BitReg))
		return 0;
	
	DWORD maxValueLen;
	LONG result = RegQueryInfoKey(hKey, 0, 0, 0, 0, 0, 0, 0, 0, &maxValueLen, 0, 0);
	if (result != ERROR_SUCCESS)
		return PyErr_SetFromWindowsErr(result);

	char *buffer = CCP_NEW("PyRegistryGetValue/buffer") char[maxValueLen];
	DWORD valueType;
	ULONG valueLen = maxValueLen;
	result = RegQueryValueEx(hKey, valueName, 0, &valueType, (LPBYTE)buffer, &valueLen);
	if (result != ERROR_SUCCESS) {
		CCP_DELETE [] buffer;
		return PyErr_SetFromWindowsErr(result);
	}
	PyObject *resultO = ValueToPython(buffer, valueLen, valueType);
	CCP_DELETE[] buffer;
	return resultO;
}


PyObject *PyRegistryGetSubkeys(PyObject *self, PyObject*args)
{
	const char *keyName;
	if (!PyArg_ParseTuple(args, "s", &keyName))
		return NULL;

	HKEY hKey;
	CRegKey cKey;
	if (!GetHKey(hKey, cKey, keyName, KEY_READ))
		return 0;

	ULONG maxKeyLen;
	LONG result = RegQueryInfoKey(hKey, 0, 0, 0, 0, &maxKeyLen, 0, 0, 0, 0, 0, 0);
	if (result != ERROR_SUCCESS)
		return PyErr_SetFromWindowsErr(result);
	maxKeyLen+=1;

	char *buffer = CCP_NEW("PyRegistryGetSubkeys/buffer") char[maxKeyLen];
	PyObject *resultO = PyList_New(0);
	for(int i = 0; true; i++) {
		DWORD keyLen = maxKeyLen;
		FILETIME lastFileTime;
		result = RegEnumKeyEx(hKey, i, buffer, &keyLen, 0, 0, 0, &lastFileTime);
		if (result == ERROR_NO_MORE_ITEMS)
			break;
		if (result != ERROR_SUCCESS) {
			CCP_DELETE [] buffer;
			Py_DECREF(resultO);
			return PyErr_SetFromWindowsErr(result);
		}
		PyObject *str = PyString_FromString(buffer);
		if (!str) {
			CCP_DELETE [] buffer;
			Py_DECREF(resultO);
			return 0;
		}
		int e = PyList_Append(resultO, str);
		Py_DECREF(str);
		if (e) {
			CCP_DELETE [] buffer;
			Py_DECREF(resultO);
			return 0;
		}
	}
	CCP_DELETE [] buffer;
	return resultO;
}

PyObject *PyRegistryGetValueNames(PyObject *self, PyObject*args)
{
	const char *keyName;
	if (!PyArg_ParseTuple(args, "s", &keyName))
		return NULL;

	HKEY hKey;
	CRegKey cKey;
	if (!GetHKey(hKey, cKey, keyName, KEY_READ))
		return 0;

	ULONG maxValueNameLen;
	LONG result = RegQueryInfoKey(hKey, 0, 0, 0, 0, 0, 0, 0, &maxValueNameLen, 0, 0, 0);
	if (result != ERROR_SUCCESS)
		return PyErr_SetFromWindowsErr(result);
	maxValueNameLen += 1;
	
	PyObject *resultO = PyList_New(0);
	if (!result)
		return 0;

	char *buffer = CCP_NEW("PyRegistryGetValueNames/buffer") char[maxValueNameLen];
	for(int i = 0; true; i++) {
		DWORD valueNameLen = maxValueNameLen;
		result = RegEnumValue(hKey, i, buffer, &valueNameLen, 0, 0, 0, 0);
		if (result == ERROR_NO_MORE_ITEMS)
			break;
	
		if (result != ERROR_SUCCESS) {
			CCP_DELETE [] buffer;
			Py_DECREF(resultO);
			return PyErr_SetFromWindowsErr(result);
		}
		PyObject *str = PyString_FromString(buffer);
		if (!str) {
			CCP_DELETE [] buffer;
			Py_DECREF(resultO);
			return 0;
		}
		int e = PyList_Append(resultO, str);
		Py_DECREF(str);
		if (e) {
			CCP_DELETE [] buffer;
			Py_DECREF(resultO);
			return 0;
		}
	}
	CCP_DELETE [] buffer;
	return resultO;
}

PyObject *PyTest(PyObject *self, PyObject *args)
{
	PyObject *a, *b=Py_None, *c=Py_None, *d=Py_None;
	PyObject *e = Py_False;
	if (!PyArg_ParseTuple(args, "O|O(OO)(O)", &a, &b, &c, &d, &e))
		return 0;
	return Py_BuildValue("OO(OO)(O)", a, b, c, d, e);
}
	


PyObject *PyRegistryGetValues(PyObject *self, PyObject*args)
{
	const char *keyName;
	if (!PyArg_ParseTuple(args, "s", &keyName))
		return NULL;

	HKEY hKey;
	CRegKey cKey;
	if (!GetHKey(hKey, cKey, keyName, KEY_READ))
		return 0;

	ULONG maxValueNameLen;
	ULONG maxValueLen;
	LONG result = RegQueryInfoKey(hKey, 0, 0, 0, 0, 0, 0, 0, &maxValueNameLen, &maxValueLen, 0, 0);
	if (result != ERROR_SUCCESS)
		return PyErr_SetFromWindowsErr(result);
	maxValueNameLen += 1;

	PyObject *resultO = PyDict_New();
	if (!resultO)
		return 0;
	
	std::vector<char> nameBuffer(maxValueNameLen);
	std::vector<char> dataBuffer(maxValueLen);

	for(int i = 0; true; i++) {
		DWORD valueNameLen = maxValueNameLen;
		DWORD valueLen = maxValueLen;
		DWORD type;
		result = RegEnumValue(hKey, i, &nameBuffer[0], &valueNameLen, 0, &type, (LPBYTE)&dataBuffer[0], &valueLen);
		if (result == ERROR_NO_MORE_ITEMS)
			break;
		if (result != ERROR_SUCCESS) {
			PyErr_SetFromWindowsErr(result);
			goto FAIL;
		}
		PyObject *value = ValueToPython(&dataBuffer[0], valueLen, type);
		if (!value)
			goto FAIL;
		int e = PyDict_SetItemString(resultO, &nameBuffer[0], value);
		Py_DECREF(value);
		if (e)
			goto FAIL;
	}
	return resultO;

FAIL:
	Py_XDECREF(resultO);
	return 0;
}


//Events
PyObject *PySetNamedEvent(PyObject *self, PyObject *args)
{
	PyObject *name;
	if (!PyArg_ParseTuple(args, "O", &name))
		return 0;
	if (!PyString_Check(name) && !PyUnicode_Check(name))
		return PyErr_SetString(PyExc_TypeError, "string or unicode expected"), 0;
	HANDLE event;
	if (PyString_Check(name))
		event = CreateEventA(0, FALSE, FALSE, PyString_AS_STRING(name));
	else
		event = CreateEventW(0, FALSE, FALSE, PyUnicode_AS_UNICODE(name));
	if (!event)
		return PyWin32Error();
	BOOL r = SetEvent(event);
	CloseHandle(event);
	if (!r)
		return PyWin32Error();
	Py_INCREF(Py_None);
	return Py_None;
}


PyObject *PyShellExecute(PyObject *self, PyObject *args)
{
	std::string msg;
	HWND hwnd = 0;
	int show=0;
	PyObject *verb, *file, *params=0, *dir=0;
	if (!PyArg_ParseTuple(args, "iOO!|O!O!i:ShellExecute", 
						  &hwnd, &verb, &PyBaseString_Type, &file, &PyBaseString_Type, &params,
						  &PyBaseString_Type, &dir, &show))
		return 0;
	if (verb == Py_None)
		verb = 0;
	else {
		verb = PyUnicode_FromObject(verb);
		if (!verb) return 0;
	}
	file = PyUnicode_FromObject(file);
	if (!file) goto BADARG;
	if (params) {
		if (params != Py_None) {
			params = PyUnicode_FromObject(params);
			if (!params) goto BADARG;
		} else
			params = 0;
	}
	if (dir) {
		if (dir != Py_None) {
			dir = PyUnicode_FromObject(dir);
			if (!dir) goto BADARG;
		} else
			dir = 0;
	}
	HINSTANCE r = ShellExecuteW(hwnd,
		verb?PyUnicode_AsUnicode(verb):0,
		PyUnicode_AsUnicode(file),
		params?PyUnicode_AsUnicode(params):0,
		dir?PyUnicode_AsUnicode(dir):0,
		show);
	Py_XDECREF(verb);
	Py_DECREF(file);
	Py_XDECREF(params);
	Py_XDECREF(dir);

#pragma warning( suppress : 4311 )
	int ir = (int)r;

	if (ir > 32) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	const char *es;
	switch (ir) {
		case 0: es = "The operating system is out of memory or resources"; break;
		case ERROR_BAD_FORMAT: es = "The .exe file is invalid (non-Microsoft Win32 .exe or error in .exe image)"; break;
		case SE_ERR_ACCESSDENIED: es = "The operating system denied access to the specified file"; break;
		case SE_ERR_ASSOCINCOMPLETE: es = "The file name association is incomplete or invalid"; break;
		case SE_ERR_DDEBUSY: es = "The Dynamic Data Exchange (DDE) transaction could not be completed because other DDE transactions were being processed"; break;
		case SE_ERR_DDEFAIL: es = "The DDE transaction failed"; break;
		case SE_ERR_DDETIMEOUT: es = "The DDE transaction could not be completed because the request timed out"; break;
		case SE_ERR_DLLNOTFOUND: es = "The specified dynamic-link library (DLL) was not found"; break;
		case SE_ERR_FNF: es = "The specified file was not found"; break;
		case SE_ERR_NOASSOC: es = "There is no application associated with the given file name extension. This error will also be returned if you attempt to print a file that is not printable"; break;
		case SE_ERR_OOM: es = "There was not enough memory to complete the operation"; break;
		case SE_ERR_PNF: es = "The specified path was not found"; break;
		case SE_ERR_SHARE: es = "A sharing violation occurred"; break;
		default: es = "Unknown error"; break;
	}
	return PyErr_Format(PyExc_WindowsError, "ShellExecute failed with error %d: %s", ir, es), 0;
BADARG:
	Py_XDECREF(verb);
	Py_XDECREF(file);
	Py_XDECREF(params);
	Py_XDECREF(dir);
	return 0;
}
			

//heapq methods
inline bool CallCompare(bool &r, PyObject *cmp, PyObject *a, PyObject *b)
{
	PyObject *res = PyObject_CallFunctionObjArgs(cmp, a, b, 0);
	if (!res) return false;
	int t = PyObject_IsTrue(res);
	Py_DECREF(res);
	if (t<0) return false;
	r = !!t;
	return true;
}

static bool HeapPercolateUp(PyListObject *l, Py_ssize_t ci, PyObject *cmp = 0)
{
	PyObject *element = PyList_GET_ITEM(l, ci);
	if (!cmp) {
		while (ci>0) {
			Py_ssize_t pi = (ci-1)>>1;
			PyObject *parent = PyList_GET_ITEM(l, pi);
			int cmp = PyObject_RichCompareBool(parent, element, Py_LE);
			if (cmp<0)
				return false; //exception
			if (cmp)
				break; //parent is less or equal to child, break.
			PyList_SET_ITEM(l, ci, parent);
			ci = pi;
		}
	} else {
		while (ci>0) {
			Py_ssize_t pi = (ci-1)>>1;
			PyObject *parent = PyList_GET_ITEM(l, pi);
			bool t;
			if (!CallCompare(t, cmp, parent, element)) return 0;
			if (t)
				break; //heap property satisfied.
			PyList_SET_ITEM(l, ci, parent);
			ci = pi;
		}
	}
	PyList_SET_ITEM(l, ci, element);
	return true;
}


bool HeapPercolateDown(PyListObject *l, Py_ssize_t size, Py_ssize_t pi, PyObject *cmp = 0)
{
	PyObject *element = PyList_GET_ITEM(l, pi);
	if (!cmp) {
		while (pi < size/2) {
			Py_ssize_t ci = 2*pi+1;
			PyObject *child = PyList_GET_ITEM(l, ci);
			if (ci+1 < size) {
				//pick smaller child
				PyObject *child1 = PyList_GET_ITEM(l, ci+1);
				int cmp = PyObject_RichCompareBool(child, child1, Py_LE);
				if (cmp<0)
					return false;
				if (!cmp){
					//child1 is smaller
					child = child1;
					ci++;
				}
			}
			int cmp = PyObject_RichCompareBool(element, child, Py_LE);
			if (cmp<0)
				return false;
			if (cmp)
				break; //element is less or equal to child.
			PyList_SET_ITEM(l, pi, child);
			pi = ci;
		}
	} else {
		while (pi < size/2) {
			Py_ssize_t ci = 2*pi+1;
			PyObject *child = PyList_GET_ITEM(l, ci);
			bool t;
			if (ci+1 < size) {
				//pick child according to heap property
				PyObject *child1 = PyList_GET_ITEM(l, ci+1);
				if (!CallCompare(t, cmp, child, child1)) return 0;
				if (!t){
					//must pick other child
					child = child1;
					ci++;
				}
			}
			if (!CallCompare(t, cmp, element, child)) return 0;
			if (t)
				break; //heap property satisfied
			PyList_SET_ITEM(l, pi, child);
			pi = ci;
		}
	}
	PyList_SET_ITEM(l, pi, element);
	return true;
}


PyObject *PyHeapPush(PyObject *self, PyObject *args)
{
	PyObject *list, *element, *cmp=0;
	if (!PyArg_ParseTuple(args, "OO|O", &list, &element, &cmp))
		return 0;
	if (!PyList_Check(list))
		return PyErr_SetString(PyExc_TypeError, "list object expected"), 0;
	if (cmp && !PyCallable_Check(cmp))
		return PyErr_SetString(PyExc_TypeError, "calalble object expected"), 0;
	
	if (PyList_Append(list, element))
		return 0;
	if (!HeapPercolateUp((PyListObject*)list, PyList_GET_SIZE(list)-1, cmp))
		return 0;
	Py_INCREF(Py_None);
	return Py_None;
}


PyObject *PyHeapPop(PyObject *self, PyObject *args)
{
	PyObject *list, *cmp=0;
	if (!PyArg_ParseTuple(args, "O|O", &list, cmp))
		return 0;
	if (!PyList_Check(list))
		return PyErr_SetString(PyExc_TypeError, "list object expected"), 0;
	if (cmp && !PyCallable_Check(cmp))
		return PyErr_SetString(PyExc_TypeError, "calalble object expected"), 0;
	
	Py_ssize_t end = PyList_GET_SIZE(list)-1; //tail element
	PyObject *result;
	if (end>=0) {//list is one or more elements
		//get head element
		result = PyList_GET_ITEM(list, 0);
		Py_INCREF(result);
		if (end) { //more than one element
			//swap first and last
			PyList_SET_ITEM(list, 0, PyList_GET_ITEM(list, end));
			PyList_SET_ITEM(list, end, result);
			//delete last element
			PyList_SetSlice(list, end, end+1, 0);
			if (!HeapPercolateDown((PyListObject*)list, end, 0, cmp)) {
				Py_DECREF(result);
				return 0;
			}
		} else  //we had only one element
			PyList_SetSlice(list, 0, 1, 0); //delete the element
	} else
		return PyErr_SetString(PyExc_IndexError, "heap is empty"), 0;
	return result;
}


PyObject *PyHeapify(PyObject *self, PyObject *args)
{
	PyObject *list, *cmp=0;
	if (!PyArg_ParseTuple(args, "O|O", &list, &cmp))
		return 0;
	if (!PyList_Check(list))
		return PyErr_SetString(PyExc_TypeError, "list object expected"), 0;
	if (cmp && !PyCallable_Check(cmp))
		return PyErr_SetString(PyExc_TypeError, "calalble object expected"), 0;

	Py_ssize_t s = PyList_GET_SIZE(list);
	for(Py_ssize_t i = s/2-1; i>=0; i--)
		if (!HeapPercolateDown((PyListObject*)list, s, i, cmp))
			return 0;
	Py_INCREF(Py_None);
	return Py_None;
}


PyObject *PyHeapReplace(PyObject *self, PyObject *args)
{
	PyObject *list, *element, *cmp=0;
	if (!PyArg_ParseTuple(args, "OO|O", &list, &element, &cmp))
		return 0;
	if (!PyList_Check(list))
		return PyErr_SetString(PyExc_TypeError, "list object expected"), 0;
	if (cmp && !PyCallable_Check(cmp))
		return PyErr_SetString(PyExc_TypeError, "calalble object expected"), 0;
	
	Py_ssize_t s = PyList_GET_SIZE(list);
	if (!s)
		return PyErr_SetString(PyExc_IndexError, "heap is empty"), 0;

	//swap the stuff.
	PyObject *result = PyList_GET_ITEM(list, 0); //take the list's reference of the head
	PyList_SET_ITEM(list, 0, element);
	Py_INCREF(element);
	if (!HeapPercolateDown((PyListObject*)list, s, 0, cmp)) {
		Py_DECREF(result);
		return 0;
	}
	return result;
}

	
PyObject *PyHeapSort(PyObject *self, PyObject *args)
{
	PyObject *list, *cmp=0;
	if (!PyArg_ParseTuple(args, "O|O", &list, &cmp))
		return 0;
	if (!PyList_Check(list))
		return PyErr_SetString(PyExc_TypeError, "list object expected"), 0;
	if (cmp && !PyCallable_Check(cmp))
		return PyErr_SetString(PyExc_TypeError, "calalble object expected"), 0;
	
	Py_ssize_t s = PyList_GET_SIZE(list);
	for(Py_ssize_t i = s-1; i>0; i--) {
		//swap head and tail of current list
		PyObject *tmp = PyList_GET_ITEM(list, i);
		PyList_SET_ITEM(list, i, PyList_GET_ITEM(list, 0));
		PyList_SET_ITEM(list, 0, tmp);
		if (!HeapPercolateDown((PyListObject*)list, i, 0, cmp))
			return 0;
	}
	Py_INCREF(Py_None);
	return Py_None;
}


PyObject *PyHeapCheck(PyObject *self, PyObject *args)
{
	PyObject *list, *cmp=0;
	if (!PyArg_ParseTuple(args, "O|O", &list, &cmp))
		return 0;
	if (!PyList_Check(list))
		return PyErr_SetString(PyExc_TypeError, "list object expected"), 0;
	if (cmp && !PyCallable_Check(cmp))
		return PyErr_SetString(PyExc_TypeError, "calalble object expected"), 0;
	
	for (Py_ssize_t i = PyList_GET_SIZE(list)-1; i>0; i--) {
		Py_ssize_t pi = (i-1)>>1;
		bool ok;
		if (cmp) {
			if (!CallCompare(ok, cmp, PyList_GET_ITEM(list, pi), PyList_GET_ITEM(list, i))) return 0;
		} else {
			int r = PyObject_RichCompareBool(PyList_GET_ITEM(list, pi), PyList_GET_ITEM(list, i), Py_LE);
			if (r<0) return 0;
			ok = !!r;
		}
		if (!ok) { //heap property not satisfied.
			Py_INCREF(Py_False);
			return Py_False;
		}
	}
	Py_INCREF(Py_True);
	return Py_True;
}

//VTune stuff
#ifdef VTUNE
#include <VTuneApi.h>
#endif

PyObject *PyVTPause(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))
		return 0;
#ifdef VTUNE
	VTPause();
#endif
	Py_INCREF(Py_None);
	return Py_None;
}
PyObject *PyVTResume(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))
		return 0;
#ifdef VTUNE
	VTResume();
#endif
	Py_INCREF(Py_None);
	return Py_None;
}


//process and thread query functions

PyObject *PyGetThreadTimes(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetThreadTimes"))
		return 0;
	ULARGE_INTEGER creationTime, exitTime, kernelTime, userTime;
	if (!loader->GetThreadTimes())
		return PyErr_SetString(PyExc_NotImplementedError, "not availible on this platform"), 0;
	BOOL ok = loader->GetThreadTimes()(GetCurrentThread(), (PFILETIME)&creationTime, (PFILETIME)&exitTime, (PFILETIME)&kernelTime, (PFILETIME)&userTime);
	if (!ok)
		return PyWin32Error(), 0;

	return Py_BuildValue("NNNN", 
		PyLong_FromUnsignedLongLong(creationTime.QuadPart),
		PyLong_FromUnsignedLongLong(exitTime.QuadPart),
		PyLong_FromUnsignedLongLong(kernelTime.QuadPart),
		PyLong_FromUnsignedLongLong(userTime.QuadPart));
}

PyObject *PyGetProcessTimes(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetProcessTimes"))
		return 0;

	if (!loader->GetProcessTimes())
		return PyErr_SetString(PyExc_NotImplementedError, "not availible on this platform"), 0;
	
	ULARGE_INTEGER creationTime, exitTime, kernelTime, userTime;
	BOOL ok = loader->GetProcessTimes()(GetCurrentProcess(), (PFILETIME)&creationTime, (PFILETIME)&exitTime, (PFILETIME)&kernelTime, (PFILETIME)&userTime);
	if (!ok)
		return PyWin32Error(), 0;

	return Py_BuildValue("NNNN", 
		PyLong_FromUnsignedLongLong(creationTime.QuadPart),
		PyLong_FromUnsignedLongLong(exitTime.QuadPart),
		PyLong_FromUnsignedLongLong(kernelTime.QuadPart),
		PyLong_FromUnsignedLongLong(userTime.QuadPart));
}

PyObject *PyGlobalMemoryStatus(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GlobalMemoryStatus"))
		return 0;
	BluePyDict r(1);
	if (loader->GlobalMemoryStatusEx()) {
		MEMORYSTATUSEX ms;
		ms.dwLength = (DWORD)sizeof(ms);
		BOOL ok = loader->GlobalMemoryStatusEx()(&ms);
		if (!ok)
			return PyWin32Error("GlobalMemoryStatusEx");
		r.Set("MemoryLoad", BluePyInt(ms.dwMemoryLoad));
		r.Set("TotalPhys", BluePy(PyLong_FromUnsignedLongLong(ms.ullTotalPhys)));
		r.Set("AvailPhys", BluePy(PyLong_FromUnsignedLongLong(ms.ullAvailPhys)));
		r.Set("TotalPageFile", BluePy(PyLong_FromUnsignedLongLong(ms.ullTotalPageFile)));
		r.Set("AvailPageFile", BluePy(PyLong_FromUnsignedLongLong(ms.ullAvailPageFile)));
		r.Set("TotalVirtual", BluePy(PyLong_FromUnsignedLongLong(ms.ullTotalVirtual)));
		r.Set("AvailVirtual", BluePy(PyLong_FromUnsignedLongLong(ms.ullAvailVirtual)));
		r.Set("AvailExtendedVirtual", BluePy(PyLong_FromUnsignedLongLong(ms.ullAvailExtendedVirtual)));
	} else {
		MEMORYSTATUS ms;
		ms.dwLength = (DWORD)sizeof(ms);
		GlobalMemoryStatus(&ms);
		r.Set("MemoryLoad", BluePyInt(ms.dwMemoryLoad));
		r.Set("TotalPhys", BluePy(PyLong_FromUnsignedLongLong(ms.dwTotalPhys)));
		r.Set("AvailPhys", BluePy(PyLong_FromUnsignedLongLong(ms.dwAvailPhys)));
		r.Set("TotalPageFile", BluePy(PyLong_FromUnsignedLongLong(ms.dwTotalPageFile)));
		r.Set("AvailPageFile", BluePy(PyLong_FromUnsignedLongLong(ms.dwAvailPageFile)));
		r.Set("TotalVirtual", BluePy(PyLong_FromUnsignedLongLong(ms.dwTotalVirtual)));
		r.Set("AvailVirtual", BluePy(PyLong_FromUnsignedLongLong(ms.dwAvailVirtual)));
		r.Set("AvailExtendedVirtual", BluePy(PyLong_FromUnsignedLongLong(0)));
	}
	return r.Detach();
}
		


PyObject *PyGetProcessMemoryInfo(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetProcessMemoryInfo"))
		return 0;

	/*
	This soft loading sometimes doesn't work, since psapi.dll isn't always in yet.
	SoftLoader::Init();
	if (!SoftLoader::loader->GetProcessMemoryInfo)
		return PyErr_SetString(PyExc_NotImplementedError, "not availible on this platform"), 0;
	
	PROCESS_MEMORY_COUNTERS counters;
	BOOL ok = SoftLoader::loader->GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters));
	*/
	PROCESS_MEMORY_COUNTERS counters;
	BOOL ok = GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters));
	
	if (!ok)
		return PyWin32Error(), 0;

	return Py_BuildValue("{si sN sN sN sN sN sN sN sN}",
		"PageFaultCount",				counters.PageFaultCount,
		"PeakWorkingSetSize",			PyLong_FromUnsignedLongLong(counters.PeakWorkingSetSize), 
		"WorkingSetSize",				PyLong_FromUnsignedLongLong(counters.WorkingSetSize), 
		"QuotaPeakPagedPoolUsage",		PyLong_FromUnsignedLongLong(counters.QuotaPeakPagedPoolUsage), 
		"QuotaPagedPoolUsage",			PyLong_FromUnsignedLongLong(counters.QuotaPagedPoolUsage), 
		"QuotaPeakNonPagedPoolUsage",	PyLong_FromUnsignedLongLong(counters.QuotaPeakNonPagedPoolUsage), 
		"QuotaNonPagedPoolUsage",		PyLong_FromUnsignedLongLong(counters.QuotaNonPagedPoolUsage), 
		"PagefileUsage",				PyLong_FromUnsignedLongLong(counters.PagefileUsage), 
		"PeakPagefileUsage",			PyLong_FromUnsignedLongLong(counters.PeakPagefileUsage));
}


PyObject *PyGetProcessWorkingSetSize(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetProcessWorkingSetSize"))
		return 0;

	if (!loader->GetProcessWorkingSetSize())
		return PyErr_SetString(PyExc_NotImplementedError, "not availible on this platform"), 0;
	
	SIZE_T min, max;
	BOOL ok = loader->GetProcessWorkingSetSize()(GetCurrentProcess(), &min, &max);
	if (!ok)
		return PyWin32Error(), 0;

	return Py_BuildValue("NN", PyInt_FromSize_t(min), PyInt_FromSize_t(max));
}


PyObject *PySetProcessWorkingSetSize(PyObject *self, PyObject *args)
{
	SIZE_T wMin = -1; // default -1, -1 trims the working set temporarily
	SIZE_T wMax = -1;

	if (!PyArg_ParseTuple(args, "|nn:SetProcessWorkingSetSize", &wMin, &wMax))
		return 0;

	if (!loader->SetProcessWorkingSetSize())
		return PyErr_SetString(PyExc_NotImplementedError, "not availible on this platform"), 0;
	
	if (!loader->SetProcessWorkingSetSize()(GetCurrentProcess(), wMin, wMax))
		return PyWin32Error();

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject *PyGetProcessHandleCount(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetProcessHandleCount"))
		return 0;
#if _WIN32_WINNT > 0x0501
	SoftLoader::Init();
	if (!SoftLoader::loader->GetProcessHandleCount)
		return PyErr_SetString(PyExc_NotImplementedError, "not availible on this platform"), 0;
	
	DWORD handleCount;
	BOOL ok = SoftLoader::loader->GetProcessHandleCount(GetCurrentProcess(), &handleCount);
	if (!ok)
		return PyWin32Error(), 0;
	return PyInt_FromLong(handleCount);
#else
	return PyErr_SetString(PyExc_NotImplementedError, "only supported in builds for XP sp1 and server 2003"), 0;
#endif
}


PyObject *PyGetProcessIoCounters(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetProcessIoCounters"))
		return 0;

	if (!loader->GetProcessIoCounters())
		return PyErr_SetString(PyExc_NotImplementedError, "not availible on this platform"), 0;

	IO_COUNTERS counters;
	BOOL ok = loader->GetProcessIoCounters()(GetCurrentProcess(), &counters);
	if (!ok)
		return PyWin32Error(), 0;

	return Py_BuildValue("{sN sN sN sN sN sN}",
		"ReadOperationCount", PyLong_FromUnsignedLongLong(counters.ReadOperationCount),
		"WriteOperationCount", PyLong_FromUnsignedLongLong(counters.WriteOperationCount),
		"OtherOperationCount", PyLong_FromUnsignedLongLong(counters.OtherOperationCount),
		"ReadTransferCount", PyLong_FromUnsignedLongLong(counters.ReadTransferCount),
		"WriteTransferCount", PyLong_FromUnsignedLongLong(counters.WriteTransferCount),
		"OtherTransferCount", PyLong_FromUnsignedLongLong(counters.OtherTransferCount));
}


PyObject *PyGetSystemTime(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetSystemTime"))
		return 0;

	SYSTEMTIME st;
	GetSystemTime(&st);
	return Py_BuildValue("{si si si si si si si si}",
		"Year", st.wYear,
		"Month", st.wMonth,
		"DayOfWeek", st.wDayOfWeek,
		"Day", st.wDay,
		"Hour", st.wHour,
		"Minute", st.wMinute,
		"Second", st.wSecond, 
		"Milliseconds", st.wMilliseconds);
}

PyObject *PyGetSystemTimeAsFileTime(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetSystemTimeAsFileTime"))
		return 0;

	ULARGE_INTEGER ft;
	GetSystemTimeAsFileTime((LPFILETIME)&ft);
	return PyLong_FromUnsignedLongLong(ft.QuadPart);
}


PyObject *PyGetTickCount(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetTickCount"))
		return 0;
	return PyInt_FromLong(GetTickCount());
}


PyObject *PyGetSystemTimeAdjustment(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetSystemTimeAdjustment"))
		return 0;

	DWORD adjustment = 0, increment = 549360;
	BOOL disabled = TRUE;
	if (loader->GetSystemTimeAdjustment()) {
		BOOL ok = loader->GetSystemTimeAdjustment()(&adjustment, &increment, &disabled);
		if (!ok)
			return PyWin32Error("GetSystemTimeAdjustment");
	}
	return Py_BuildValue("iiO", adjustment, increment, disabled?Py_True:Py_False);
}


static PyObject *PackSystemInfo(const SYSTEM_INFO &si)
{
	BluePyDict r(1);
	#define ADDINT(KEY, VAL) r.Set(#KEY, BluePyInt((int)si.VAL))
	#define ADDPTR(KEY, VAL) r.Set(#KEY, BluePy(PyLong_FromUnsignedLongLong((uintptr_t)si.VAL), false))
	const char *arch;
	si.lpMaximumApplicationAddress;
	switch(si.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_INTEL : arch = "PROCESSOR_ARCHITECTURE_INTEL"; break;
	case PROCESSOR_ARCHITECTURE_ALPHA : arch = "PROCESSOR_ARCHITECTURE_ALPHA"; break;
	case PROCESSOR_ARCHITECTURE_PPC : arch = "PROCESSOR_ARCHITECTURE_PPC"; break;
	case PROCESSOR_ARCHITECTURE_IA64 : arch = "PROCESSOR_ARCHITECTURE_IA64"; break;
	case PROCESSOR_ARCHITECTURE_IA32_ON_WIN64 : arch = "PROCESSOR_ARCHITECTURE_IA32_ON_WIN64"; break;
	case PROCESSOR_ARCHITECTURE_AMD64 : arch = "PROCESSOR_ARCHITECTURE_AMD64"; break;
	default : arch = "PROCESSOR_ARCHITECTURE_UNKNOWN"; break;
	};
	r.Set("ProcessorArchitecture", BluePyStr(arch));
	ADDINT(ProcessorLevel, wProcessorLevel);
	ADDINT(ProcessorRevision, wProcessorRevision);
	ADDINT(PageSize, dwPageSize);
	ADDPTR(MinimumApplicationAddress, lpMinimumApplicationAddress);
	ADDPTR(MaximumApplicationAddress, lpMaximumApplicationAddress);
	ADDINT(ActiveProcessorMask, dwActiveProcessorMask);
	ADDINT(NumberOfProcessors, dwNumberOfProcessors);
	ADDINT(AllocationGranularity, dwAllocationGranularity);
	return r.Detach();
}





//  ---------------------------------------------------------------------------

PyObject* PyToggleTcpEStats(PyObject* self, PyObject* args)
{
	if( loader->SetPerTcpConnectionEStats() == NULL || loader->GetExtendedTcpTable() == NULL )
	{
		return PyErr_SetString( PyExc_NotImplementedError, "Not available on this platform" ), NULL;
	}

	int enableInt;
	if( !PyArg_ParseTuple( args, "i:ToggleTCPEStats", &enableInt ) )
	{
		return NULL; // ParseTuple throws its own exception on fail, no need to set error.
	}

	TCP_ESTATS_DATA_RW_v0 dataRW;
	TCP_ESTATS_SND_CONG_RW_v0 sendCongRW;
	TCP_ESTATS_PATH_RW_v0 pathRW;
	TCP_ESTATS_REC_RW_v0 recRW;
	TCP_ESTATS_BANDWIDTH_RW_v0 bandwidthRW;

	if( enableInt == 0 )
	{
		dataRW.EnableCollection = false;
		sendCongRW.EnableCollection = false;
		pathRW.EnableCollection = false;
		recRW.EnableCollection = false;
		bandwidthRW.EnableCollectionOutbound = TcpBoolOptDisabled;
		bandwidthRW.EnableCollectionInbound = TcpBoolOptDisabled;
	}
	else
	{
		dataRW.EnableCollection = true;
		sendCongRW.EnableCollection = true;
		pathRW.EnableCollection = true;
		recRW.EnableCollection = true;
		bandwidthRW.EnableCollectionOutbound = TcpBoolOptEnabled;
		bandwidthRW.EnableCollectionInbound = TcpBoolOptEnabled;
	}

	// Go through the TCP table looking for the entries that our PID owns, then enable/disable all of the
	// modes we care about on the matching entries.

	MIB_TCPTABLE_OWNER_PID* tcpTable = NULL;
	DWORD tableSize = 0;
	DWORD retValue;
	int i;

	// Get the size we need
	retValue = loader->GetExtendedTcpTable()( NULL, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_CONNECTIONS, 0 );
	if( retValue != ERROR_INSUFFICIENT_BUFFER )
	{
		PyErr_SetString( PyExc_RuntimeError, "GetExtendedTcpTable errored incorrectly while getting buffer size" );
		return NULL;
	}

	CcpMallocBuffer tcpTableBuffer( "PyToggleTCPEStats/tcpTable", tableSize );
	tcpTable = ( MIB_TCPTABLE_OWNER_PID* ) tcpTableBuffer.get();
	if( tcpTable == NULL )
	{
		PyErr_SetString( PyExc_MemoryError, "Malloc failed while creating a TCP Table" );
		return NULL;
	}

	retValue = loader->GetExtendedTcpTable()( tcpTable, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_CONNECTIONS, 0 );

	if( retValue != NO_ERROR )
	{
		PyErr_SetString( PyExc_RuntimeError, "GetExtendedTcpTable errored when retrieving the table" );
		return NULL;
	}

	DWORD myPid = GetCurrentProcessId();
	for( i = 0; i < ( int ) tcpTable->dwNumEntries; i++ )
	{
		if( tcpTable->table[i].dwOwningPid != myPid )
		{
			continue;
		}

		if( loader->SetPerTcpConnectionEStats()( ( MIB_TCPROW* ) &tcpTable->table[i], TcpConnectionEstatsData,      ( PUCHAR ) &dataRW,      0, sizeof( dataRW ),      0) != NO_ERROR ||
			loader->SetPerTcpConnectionEStats()( ( MIB_TCPROW* ) &tcpTable->table[i], TcpConnectionEstatsSndCong,   ( PUCHAR ) &sendCongRW,  0, sizeof( sendCongRW ),  0) != NO_ERROR ||
			loader->SetPerTcpConnectionEStats()( ( MIB_TCPROW* ) &tcpTable->table[i], TcpConnectionEstatsPath,      ( PUCHAR ) &pathRW,      0, sizeof( pathRW ),      0) != NO_ERROR ||
			loader->SetPerTcpConnectionEStats()( ( MIB_TCPROW* ) &tcpTable->table[i], TcpConnectionEstatsRec,       ( PUCHAR ) &recRW,       0, sizeof( recRW ),       0) != NO_ERROR ||
			loader->SetPerTcpConnectionEStats()( ( MIB_TCPROW* ) &tcpTable->table[i], TcpConnectionEstatsBandwidth, ( PUCHAR ) &bandwidthRW, 0, sizeof( bandwidthRW ), 0) != NO_ERROR )
		{
			PyErr_SetString( PyExc_RuntimeError, "SetPerTcpConnectionEStats errored when setting enable/disable status" );
			return NULL;
		}
	}

	Py_RETURN_NONE;
}

PyObject* PyGetProcessTcpEStats(PyObject* self, PyObject* args)
{
	if( loader->GetPerTcpConnectionEStats() == NULL || loader->GetExtendedTcpTable() == NULL )
	{
		PyErr_SetString(PyExc_NotImplementedError, "Not available on this platform");
		return NULL;
	}

	if( !PyArg_ParseTuple( args, ":ProcessTCPStatus" ) )
	{
		return NULL; // ParseTuple throws its own exception on fail, no need to set error.
	}	

	// Go through the TCP table looking for the entries that our PID owns, then grab the stats for them
	// and plunk it all into a nice dict for the Python side to process

	MIB_TCPTABLE_OWNER_PID* tcpTable = NULL;
	DWORD tableSize = 0;
	DWORD retValue;
	PyObject* returnList;
	int i;

	// Get the size we need
	retValue = loader->GetExtendedTcpTable()( NULL, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_CONNECTIONS, 0 );
	if( retValue != ERROR_INSUFFICIENT_BUFFER )
	{
		PyErr_SetString( PyExc_RuntimeError, "GetExtendedTcpTable errored incorrectly while getting buffer size" );
		return NULL;
	}

	CcpMallocBuffer tcpTableBuffer( "PyProcessTCPStatus/tcpTable", tableSize );
	tcpTable = ( MIB_TCPTABLE_OWNER_PID* ) tcpTableBuffer.get();
	if( tcpTable == NULL )
	{
		PyErr_SetString( PyExc_MemoryError, "Malloc failed while creating a TCP Table" );
		return NULL;
	}

	retValue = loader->GetExtendedTcpTable()( tcpTable, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_CONNECTIONS, 0 );

	if( retValue != NO_ERROR )
	{
		PyErr_SetString( PyExc_RuntimeError, "GetExtendedTcpTable errored when retrieving the table" );
		return NULL;
	}

	DWORD myPid = GetCurrentProcessId();
	returnList = PyList_New( 0 );
	for( i = 0; i < ( int ) tcpTable->dwNumEntries; i++ )
	{
		if( tcpTable->table[i].dwOwningPid != myPid )
		{
			continue;
		}

		PyObject* valueDictionary = PyDict_New();
		PyList_Append( returnList, valueDictionary );

		PyObject* tmp; // A temp item that will hold the objects we're adding to a dict just long enough to decriment the ref to them.
		
		struct in_addr ipAddr;
		ipAddr.S_un.S_addr = ( u_long ) tcpTable->table[i].dwLocalAddr;
		PyDict_SetItemString( valueDictionary, "localAddr", tmp = PyString_FromString( inet_ntoa( ipAddr ) ) );  Py_DECREF( tmp );
		PyDict_SetItemString( valueDictionary, "localPort", tmp = PyLong_FromLong( ntohs( ( u_short ) tcpTable->table[i].dwLocalPort ) ) );  Py_DECREF( tmp );

		ipAddr.S_un.S_addr = ( u_long ) tcpTable->table[i].dwRemoteAddr;
		PyDict_SetItemString( valueDictionary, "remoteAddr", tmp = PyString_FromString( inet_ntoa( ipAddr ) ) );  Py_DECREF( tmp );
		PyDict_SetItemString( valueDictionary, "remotePort", tmp = PyLong_FromLong( ntohs( ( u_short ) tcpTable->table[i].dwRemotePort ) ) );  Py_DECREF( tmp );

		TCP_ESTATS_DATA_ROD_v0 connData;
		retValue = loader->GetPerTcpConnectionEStats()( ( MIB_TCPROW* ) &tcpTable->table[i], TcpConnectionEstatsData,
			                                            NULL, 0, 0, NULL, 0, 0, ( PUCHAR ) &connData, 0, sizeof( connData ) );
		if( retValue == NO_ERROR )
		{
			PyObject* connDataDict = PyDict_New();
			PyDict_SetItemString( valueDictionary, "connData", connDataDict );

			PyDict_SetItemString( connDataDict, "DataBytesOut",      tmp = PyLong_FromUnsignedLongLong( connData.DataBytesOut ) );       Py_DECREF( tmp );
			PyDict_SetItemString( connDataDict, "DataSegsOut",       tmp = PyLong_FromUnsignedLongLong( connData.DataSegsOut ) );        Py_DECREF( tmp );
			PyDict_SetItemString( connDataDict, "DataBytesIn",       tmp = PyLong_FromUnsignedLongLong( connData.DataBytesIn ) );        Py_DECREF( tmp );
			PyDict_SetItemString( connDataDict, "DataSegsIn",        tmp = PyLong_FromUnsignedLongLong( connData.DataSegsIn ) );         Py_DECREF( tmp );
			PyDict_SetItemString( connDataDict, "SegsOut",           tmp = PyLong_FromUnsignedLongLong( connData.SegsOut ) );	         Py_DECREF( tmp );
			PyDict_SetItemString( connDataDict, "SegsIn",            tmp = PyLong_FromUnsignedLongLong( connData.SegsIn ) );             Py_DECREF( tmp );
			PyDict_SetItemString( connDataDict, "SoftErrors",        tmp = PyLong_FromUnsignedLong( connData.SoftErrors ) );             Py_DECREF( tmp );
			PyDict_SetItemString( connDataDict, "SoftErrorReason",   tmp = PyLong_FromUnsignedLong( connData.SoftErrorReason ) );        Py_DECREF( tmp );
			PyDict_SetItemString( connDataDict, "SndUna",            tmp = PyLong_FromUnsignedLong( connData.SndUna ) );                 Py_DECREF( tmp );
			PyDict_SetItemString( connDataDict, "SndNxt",            tmp = PyLong_FromUnsignedLong( connData.SndNxt ) );                 Py_DECREF( tmp );
			PyDict_SetItemString( connDataDict, "SndMax",            tmp = PyLong_FromUnsignedLong( connData.SndMax ) );                 Py_DECREF( tmp );
			PyDict_SetItemString( connDataDict, "ThruBytesAcked",    tmp = PyLong_FromUnsignedLongLong( connData.ThruBytesAcked ) );     Py_DECREF( tmp );
			PyDict_SetItemString( connDataDict, "RcvNxt",            tmp = PyLong_FromUnsignedLong( connData.RcvNxt ) );                 Py_DECREF( tmp );
			PyDict_SetItemString( connDataDict, "ThruBytesReceived", tmp = PyLong_FromUnsignedLongLong( connData.ThruBytesReceived ) );  Py_DECREF( tmp );

			Py_DECREF( connDataDict );
		}

		TCP_ESTATS_SND_CONG_ROD_v0 congData;
		retValue = loader->GetPerTcpConnectionEStats()( ( MIB_TCPROW* ) &tcpTable->table[i], TcpConnectionEstatsSndCong,
			                                            NULL, 0, 0, NULL, 0, 0, ( PUCHAR ) &congData, 0, sizeof( congData ) );
		if( retValue == NO_ERROR )
		{
			PyObject* congDataDict = PyDict_New();
			PyDict_SetItemString( valueDictionary, "congData", congDataDict );

			PyDict_SetItemString( congDataDict, "SndLimTransRwin", tmp = PyLong_FromUnsignedLong( congData.SndLimTransRwin ) );  Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "SndLimTimeRwin",  tmp = PyLong_FromUnsignedLong( congData.SndLimTimeRwin ) );   Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "SndLimBytesRwin", tmp = _PyLong_FromSize_t( congData.SndLimBytesRwin ) );       Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "SndLimTransCwnd", tmp = PyLong_FromUnsignedLong( congData.SndLimTransCwnd ) );  Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "SndLimTimeCwnd",  tmp = PyLong_FromUnsignedLong( congData.SndLimTimeCwnd ) );   Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "SndLimBytesCwnd", tmp = _PyLong_FromSize_t( congData.SndLimBytesCwnd ) );       Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "SndLimTransSnd",  tmp = PyLong_FromUnsignedLong( congData.SndLimTransSnd ) );   Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "SndLimTimeSnd",   tmp = PyLong_FromUnsignedLong( congData.SndLimTimeSnd ) );    Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "SndLimBytesSnd",  tmp = _PyLong_FromSize_t( congData.SndLimBytesSnd ) );        Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "SlowStart",       tmp = PyLong_FromUnsignedLong( congData.SlowStart ) );        Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "CongAvoid",       tmp = PyLong_FromUnsignedLong( congData.CongAvoid ) );        Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "OtherReductions", tmp = PyLong_FromUnsignedLong( congData.OtherReductions ) );  Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "CurCwnd",         tmp = PyLong_FromUnsignedLong( congData.CurCwnd ) );          Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "MaxSsCwnd",       tmp = PyLong_FromUnsignedLong( congData.MaxSsCwnd ) );        Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "MaxCaCwnd",       tmp = PyLong_FromUnsignedLong( congData.MaxCaCwnd ) );        Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "CurSsthresh",     tmp = PyLong_FromUnsignedLong( congData.CurSsthresh ) );      Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "MaxSsthresh",     tmp = PyLong_FromUnsignedLong( congData.MaxSsthresh ) );      Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "MinSsthresh",     tmp = PyLong_FromUnsignedLong( congData.MinSsthresh ) );      Py_DECREF( tmp );
			
			Py_DECREF( congDataDict );
		}

		TCP_ESTATS_PATH_ROD_v0 pathData;
		retValue = loader->GetPerTcpConnectionEStats()( ( MIB_TCPROW*) &tcpTable->table[i], TcpConnectionEstatsPath,
			                                            NULL, 0, 0, NULL, 0, 0, ( PUCHAR ) &pathData, 0, sizeof(pathData));
		if( retValue == NO_ERROR ) {
			PyObject* pathDataDict = PyDict_New();
			PyDict_SetItemString( valueDictionary, "pathData", pathDataDict );

			PyDict_SetItemString( pathDataDict, "FastRetran",            tmp = PyLong_FromUnsignedLong( pathData.FastRetran ) );             Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "Timeouts",              tmp = PyLong_FromUnsignedLong( pathData.Timeouts ) );               Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "SubsequentTimeouts",    tmp = PyLong_FromUnsignedLong( pathData.SubsequentTimeouts ) );     Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "CurTimeoutCount",       tmp = PyLong_FromUnsignedLong( pathData.CurTimeoutCount ) );        Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "AbruptTimeouts",        tmp = PyLong_FromUnsignedLong( pathData.AbruptTimeouts ) );         Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "PktsRetrans",           tmp = PyLong_FromUnsignedLong( pathData.PktsRetrans ) );            Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "BytesRetrans",          tmp = PyLong_FromUnsignedLong( pathData.BytesRetrans ) );           Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "DupAcksIn",             tmp = PyLong_FromUnsignedLong( pathData.DupAcksIn ) );              Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "SacksRcvd",             tmp = PyLong_FromUnsignedLong( pathData.SacksRcvd ) );              Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "SackBlocksRcvd",        tmp = PyLong_FromUnsignedLong( pathData.SackBlocksRcvd ) );         Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "CongSignals",           tmp = PyLong_FromUnsignedLong( pathData.CongSignals ) );            Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "PreCongSumCwnd",        tmp = PyLong_FromUnsignedLong( pathData.PreCongSumCwnd ) );         Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "PreCongSumRtt",         tmp = PyLong_FromUnsignedLong( pathData.PreCongSumRtt ) );          Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "PostCongSumRtt",        tmp = PyLong_FromUnsignedLong( pathData.PostCongSumRtt ) );         Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "PostCongCountRtt",      tmp = PyLong_FromUnsignedLong( pathData.PostCongCountRtt ) );       Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "EcnSignals",            tmp = PyLong_FromUnsignedLong( pathData.EcnSignals ) );             Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "EceRcvd",               tmp = PyLong_FromUnsignedLong( pathData.EceRcvd ) );                Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "SendStall",             tmp = PyLong_FromUnsignedLong( pathData.SendStall ) );              Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "QuenchRcvd",            tmp = PyLong_FromUnsignedLong( pathData.QuenchRcvd ) );             Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "RetranThresh",          tmp = PyLong_FromUnsignedLong( pathData.RetranThresh ) );           Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "SndDupAckEpisodes",     tmp = PyLong_FromUnsignedLong( pathData.SndDupAckEpisodes ) );      Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "SumBytesReordered",     tmp = PyLong_FromUnsignedLong( pathData.SumBytesReordered ) );      Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "NonRecovDa",            tmp = PyLong_FromUnsignedLong( pathData.NonRecovDa ) );             Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "NonRecovDaEpisodes",    tmp = PyLong_FromUnsignedLong( pathData.NonRecovDaEpisodes ) );     Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "AckAfterFr",            tmp = PyLong_FromUnsignedLong( pathData.AckAfterFr ) );             Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "DsackDups",             tmp = PyLong_FromUnsignedLong( pathData.DsackDups ) );              Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "SampleRtt",             tmp = PyLong_FromUnsignedLong( pathData.SampleRtt ) );              Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "SmoothedRtt",           tmp = PyLong_FromUnsignedLong( pathData.SmoothedRtt ) );            Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "RttVar",                tmp = PyLong_FromUnsignedLong( pathData.RttVar ) );                 Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "MaxRtt",                tmp = PyLong_FromUnsignedLong( pathData.MaxRtt ) );                 Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "MinRtt",                tmp = PyLong_FromUnsignedLong( pathData.MinRtt ) );                 Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "SumRtt",                tmp = PyLong_FromUnsignedLong( pathData.SumRtt ) );                 Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "CountRtt",              tmp = PyLong_FromUnsignedLong( pathData.CountRtt ) );               Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "CurRto",                tmp = PyLong_FromUnsignedLong( pathData.CurRto ) );                 Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "MaxRto",                tmp = PyLong_FromUnsignedLong( pathData.MaxRto ) );                 Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "MinRto",                tmp = PyLong_FromUnsignedLong( pathData.MinRto ) );                 Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "CurMss",                tmp = PyLong_FromUnsignedLong( pathData.CurMss ) );                 Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "MaxMss",                tmp = PyLong_FromUnsignedLong( pathData.MaxMss ) );                 Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "MinMss",                tmp = PyLong_FromUnsignedLong( pathData.MinMss ) );                 Py_DECREF( tmp );
			PyDict_SetItemString( pathDataDict, "SpuriousRtoDetections", tmp = PyLong_FromUnsignedLong( pathData.SpuriousRtoDetections ) );  Py_DECREF( tmp );

			Py_DECREF( pathDataDict );
		}

		TCP_ESTATS_REC_ROD_v0 recStats;
		retValue = loader->GetPerTcpConnectionEStats()( ( MIB_TCPROW* ) &tcpTable->table[i], TcpConnectionEstatsRec,
			                                            NULL, 0, 0, NULL, 0, 0, ( PUCHAR ) &recStats, 0, sizeof( recStats ) );
		if( retValue == NO_ERROR ) {
			PyObject* recDataDict = PyDict_New();
			PyDict_SetItemString( valueDictionary, "recData", recDataDict );

			PyDict_SetItemString( recDataDict, "CurRwinSent",    tmp = PyLong_FromUnsignedLong( recStats.CurRwinSent ) );    Py_DECREF( tmp );
			PyDict_SetItemString( recDataDict, "MaxRwinSent",    tmp = PyLong_FromUnsignedLong( recStats.MaxRwinSent ) );    Py_DECREF( tmp );
			PyDict_SetItemString( recDataDict, "MinRwinSent",    tmp = PyLong_FromUnsignedLong( recStats.MinRwinSent ) );    Py_DECREF( tmp );
			PyDict_SetItemString( recDataDict, "LimRwin",        tmp = PyLong_FromUnsignedLong( recStats.LimRwin ) );        Py_DECREF( tmp );
			PyDict_SetItemString( recDataDict, "DupAckEpisodes", tmp = PyLong_FromUnsignedLong( recStats.DupAckEpisodes ) ); Py_DECREF( tmp );
			PyDict_SetItemString( recDataDict, "DupAcksOut",     tmp = PyLong_FromUnsignedLong( recStats.DupAcksOut ) );     Py_DECREF( tmp );
			PyDict_SetItemString( recDataDict, "CeRcvd",         tmp = PyLong_FromUnsignedLong( recStats.CeRcvd ) );         Py_DECREF( tmp );
			PyDict_SetItemString( recDataDict, "EcnSent",        tmp = PyLong_FromUnsignedLong( recStats.EcnSent ) );        Py_DECREF( tmp );
			PyDict_SetItemString( recDataDict, "EcnNoncesRcvd",  tmp = PyLong_FromUnsignedLong( recStats.EcnNoncesRcvd ) );  Py_DECREF( tmp );
			PyDict_SetItemString( recDataDict, "CurReasmQueue",  tmp = PyLong_FromUnsignedLong( recStats.CurReasmQueue ) );  Py_DECREF( tmp );
			PyDict_SetItemString( recDataDict, "MaxReasmQueue",  tmp = PyLong_FromUnsignedLong( recStats.MaxReasmQueue ) );  Py_DECREF( tmp );
			PyDict_SetItemString( recDataDict, "CurAppRQueue",   tmp = _PyLong_FromSize_t( recStats.CurAppRQueue ) );        Py_DECREF( tmp );
			PyDict_SetItemString( recDataDict, "MaxAppRQueue",   tmp = _PyLong_FromSize_t( recStats.MaxAppRQueue ) );        Py_DECREF( tmp );
			PyDict_SetItemString( recDataDict, "WinScaleSent",   tmp = PyLong_FromUnsignedLong( recStats.WinScaleSent ) );   Py_DECREF( tmp );
			
			Py_DECREF( recDataDict );
		}

		TCP_ESTATS_BANDWIDTH_ROD_v0 bwidthStats;
		retValue = loader->GetPerTcpConnectionEStats()( ( MIB_TCPROW* ) &tcpTable->table[i], TcpConnectionEstatsBandwidth,
			                                            NULL, 0, 0, NULL, 0, 0, ( PUCHAR ) &bwidthStats, 0, sizeof( bwidthStats ) );
		if( retValue == NO_ERROR ) {
			PyObject* bwidthDataDict = PyDict_New();
			PyDict_SetItemString( valueDictionary, "bwidthData", bwidthDataDict );

			PyDict_SetItemString( bwidthDataDict, "OutboundBandwidth",       tmp = PyLong_FromUnsignedLongLong( bwidthStats.OutboundBandwidth ) );    Py_DECREF( tmp );
			PyDict_SetItemString( bwidthDataDict, "InboundBandwidth",        tmp = PyLong_FromUnsignedLongLong( bwidthStats.InboundBandwidth ) );     Py_DECREF( tmp );
			PyDict_SetItemString( bwidthDataDict, "OutboundInstability",     tmp = PyLong_FromUnsignedLongLong( bwidthStats.OutboundInstability ) );  Py_DECREF( tmp );
			PyDict_SetItemString( bwidthDataDict, "InboundInstability",      tmp = PyLong_FromUnsignedLongLong( bwidthStats.InboundInstability ) );   Py_DECREF( tmp );
			PyDict_SetItemString( bwidthDataDict, "OutboundBandwidthPeaked", tmp = PyLong_FromUnsignedLong( bwidthStats.OutboundBandwidthPeaked ) );  Py_DECREF( tmp );
			PyDict_SetItemString( bwidthDataDict, "InboundBandwidthPeaked",  tmp = PyLong_FromUnsignedLong( bwidthStats.InboundBandwidthPeaked ) );   Py_DECREF( tmp );

			Py_DECREF( bwidthDataDict );
		}
		
		Py_DECREF( valueDictionary );
	}

	return returnList;
}

PyObject *PyGetSystemInfo(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetSystemInfo"))
		return 0;
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return PackSystemInfo(si);
}

PyObject *PyGetNativeSystemInfo(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetNativeSystemInfo"))
		return 0;

	SYSTEM_INFO si;
	if (loader->GetNativeSystemInfo())
		loader->GetNativeSystemInfo()(&si);
	else
		GetSystemInfo(&si);
	return PackSystemInfo(si);
}

PyObject *PyIsWow64Process(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":IsWow64Process"))
		return 0;
	if (!loader->IsWow64Process()) {
		Py_INCREF(Py_False);
		return Py_False;
	}
	BOOL result;
	BOOL ok = loader->IsWow64Process()(GetCurrentProcess(), &result);
	if (!ok)
		return PyWin32Error("IsWow64Process");
	PyObject *ret = result?Py_True:Py_False;
	Py_INCREF(ret);
	return ret;
}


PyObject *PyIsTransgaming(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":IsTransgaming"))
		return 0;
	PyObject *result = Py_False;
	
	if ( IsTransgaming() )
	{
		result = Py_True;
	}
	Py_INCREF(result);
	return result;
}


PyObject *PyTGGetOS(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":TGGetOS"))
		return 0;

	HMODULE hMod = GetModuleHandle("ntdll");
	typedef const char * (WINAPI *  TGGetOS) (void);
	TGGetOS pFunc = (TGGetOS)GetProcAddress (hMod, "TGGetOS");
	if (!pFunc)
		return PyErr_SetString(PyExc_NotImplementedError, "TGGetOS"), 0;
	return PyString_FromString(pFunc());
}


PyObject *PyTGGetSystemInfo(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":TGGetSystemInfo"))
		return 0;

	HMODULE hMod = GetModuleHandle("ntdll");
	typedef BOOL (WINAPI *  TGGetSystemInfo) (
		char *platform_type,
		size_t platform_type_size,
		DWORD *platform_major_version,
		DWORD *platform_minor_version,
		char *platform_extra,
		size_t platform_extra_size,
		char *platform_distro,
		size_t platform_distro_size,
		DWORD *platform_bitcount);

	TGGetSystemInfo pFunc = (TGGetSystemInfo)GetProcAddress (hMod, "TGGetSystemInfo");
	if (!pFunc)
		return PyErr_SetString(PyExc_NotImplementedError, "TGGetSystemInfo"), 0;

	char platform_type[1025];
	char platform_extra[1025];
	char platform_distro[1025];
	DWORD major, minor, bitcount;
	BOOL ok = pFunc(
		platform_type, sizeof(platform_type),
		&major, &minor,
		platform_extra, sizeof(platform_extra),
		platform_distro, sizeof(platform_distro),
		&bitcount);
	if (!ok)
		return PyErr_SetString(PyExc_RuntimeError, "TGGetSystemInfo"), 0;

	return Py_BuildValue("{ss sk sk ss ss sk}",
		"platform_type", platform_type, 
		"platform_major_version", major,
		"platform_minor_version", minor, 
		"platform_extra", platform_extra,
		"platform_distro", platform_distro,
		"platform_bitcount", bitcount);
}


PyObject *PyGetProcessBits(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetProcessBits"))
		return 0;
#ifdef _WIN64
	return PyInt_FromLong(64);
#else
	return PyInt_FromLong(32);
#endif
}

PyObject *PyGetSystemBits(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetSystemBits"))
		return 0;
#ifdef _WIN64
	long bits = 64;
#else
	long bits = 32;
	if (loader->IsWow64Process()) {
		BOOL ok, r;
		ok = loader->IsWow64Process()(GetCurrentProcess(), &r);
		if (ok && r)
			bits = 64;
	}
#endif
	return PyInt_FromLong(bits);
}


PyObject *PyGetVersionEx(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetVersionEx"))
		return 0;
	OSVERSIONINFOEX vi;
	vi.dwOSVersionInfoSize = sizeof(vi);
	BOOL ok = GetVersionEx((LPOSVERSIONINFO)&vi);
	if (!ok)
		return PyWin32Error("GetVersionEx");
	return Py_BuildValue("{si si si si si ss si si si si}", 
#define V(S) #S, vi.S
		V(dwOSVersionInfoSize),
		V(dwMajorVersion),
		V(dwMinorVersion),
		V(dwBuildNumber),
		V(dwPlatformId),
		V(szCSDVersion),
		V(wServicePackMajor),
		V(wServicePackMinor),
		V(wSuiteMask),
		V(wProductType) );
#undef V
}
		

PyObject *PyQueryPerformanceCounter(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":QueryPerformanceCounter"))
		return 0;
	static LARGE_INTEGER safety = {0};
	LARGE_INTEGER li;
	BOOL r = QueryPerformanceCounter(&li);
	if (!r)
		return PyWin32Error("QueryPerformanceCounter");
	//Here a failsafe for the platforms with out of sync multicore performance counters.
	//make it at least not run backwards.
	if (li.QuadPart > safety.QuadPart)
		safety.QuadPart = li.QuadPart;
	else
		li.QuadPart = safety.QuadPart;
	return PyLong_FromLongLong(li.QuadPart);
}


PyObject *PyQueryPerformanceFrequency(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":QueryPerformanceFrequency"))
		return 0;
	LARGE_INTEGER li;
	BOOL r = QueryPerformanceFrequency(&li);
	if (!r)
		return PyWin32Error("QueryPerformanceFrequency");
	return PyLong_FromLongLong(li.QuadPart);
}

/****************************
 * Session Notifications
 */
PyObject *PyWTSRegisterSessionNotification(PyObject *self, PyObject *args)
{
	HWND hwnd;
	DWORD flags;
	if ( !PyArg_ParseTuple(args, "ii", &hwnd, &flags ) )
		return 0;

	if (!loader->WTSRegisterSessionNotification())
		return PyWin32Error("WTSRegisterSessionNotification could not be loaded");

	BOOL ok = loader->WTSRegisterSessionNotification()(hwnd, flags);
	if (!ok)
		return PyWin32Error("WTSRegisterSessionNotification");
	Py_INCREF(Py_None);
	return (Py_None);
}

PyObject *PyWTSUnRegisterSessionNotification(PyObject *self, PyObject *args)
{
	HWND hwnd;
	if ( !PyArg_ParseTuple(args, "i", &hwnd ) )
		return 0;

	if (!loader->WTSUnRegisterSessionNotification())
		return PyWin32Error("WTSUnRegisterSessionNotification could not be loaded",0);

	BOOL ok = loader->WTSUnRegisterSessionNotification()( hwnd );
	if (!ok)
		return PyWin32Error("WTSUnRegisterSessionNotification",0);

	Py_INCREF(Py_None);
	return (Py_None);
}
/****************************
 * affinity maska
 */
PyObject *PyGetProcessAffinityMask(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetProcessAffinityMask"))
		return 0;
	DWORD_PTR proc, sys;
	BOOL ok = GetProcessAffinityMask(GetCurrentProcess(), &proc, &sys);
	if (!ok)
		return PyWin32Error("GetProcessAffinityMask");
	return Py_BuildValue("NN", PyLong_FromLongLong(proc), PyLong_FromLongLong(sys));
}

PyObject *PySetProcessAffinityMask(PyObject *self, PyObject *args)
{
	PyObject *mask;
	if (!PyArg_ParseTuple(args, "O:SetProcessAffinityMask", &mask))
		return 0;
	if (!PyLong_Check(mask) && !PyInt_Check(mask))
		return PyErr_SetString(PyExc_TypeError, "expected int or long"), 0;

	DWORD_PTR m;
	if (PyLong_Check(mask))
		m = (DWORD_PTR)PyLong_AsLongLong(mask);
	else
		m = (DWORD_PTR)PyInt_AsLong(mask);
	if (m == -1 && PyErr_Occurred())
		return 0;

	BOOL ok = SetProcessAffinityMask(GetCurrentProcess(), m);
	if (!ok)
		return PyWin32Error("SetProcessAffinityMask");
	Py_INCREF(Py_None);
	return Py_None;
}

PyObject *PySetThreadAffinityMask(PyObject *self, PyObject *args)
{
	PyObject *mask;
	if (!PyArg_ParseTuple(args, "O:SetThreadAffinityMask", &mask))
		return 0;
	if (!PyLong_Check(mask) && !PyInt_Check(mask))
		return PyErr_SetString(PyExc_TypeError, "expected int or long"), 0;

	DWORD_PTR m;
	if (PyLong_Check(mask))
		m = (DWORD_PTR)PyLong_AsLongLong(mask);
	else
		m = (DWORD_PTR)PyInt_AsLong(mask);
	if (m == -1 && PyErr_Occurred())
		return 0;

	DWORD_PTR old = SetThreadAffinityMask(GetCurrentThread(), m);
	if (!old)
		return PyWin32Error("SetThreadAffinityMask");
	return PyLong_FromLongLong(old);
}


PyObject *PySetThreadIdealProcessor(PyObject *self, PyObject *args)
{
	DWORD p = MAXIMUM_PROCESSORS;
	if (!PyArg_ParseTuple(args, "|i:SetThreadIdealProcessor", &p))
		return 0;

	if (!loader->SetThreadIdealProcessor())
		return PyErr_SetString(PyExc_NotImplementedError, "not supported on this machine"), 0;
	
	DWORD old = loader->SetThreadIdealProcessor()(GetCurrentThread(), p);
	if (old == -1)
		return PyWin32Error("SetThreadIdealProcessor");
	return PyInt_FromLong(old);
}


PyObject *PyIsProcessorFeaturePresent(PyObject *self, PyObject *args)
{
	DWORD f;
	if (!PyArg_ParseTuple(args, "|i:IsProcessorFeaturePresent", &f))
		return 0;
	BOOL r = IsProcessorFeaturePresent(f);
	PyObject *res = r?Py_True:Py_False;
	Py_INCREF(res);
	return res;
}


static PyObject *ParseIP_ADDR_STRING(const IP_ADDR_STRING *ias)
{
	BluePyList list(0); if (!list) return 0;
	if (!ias)
		return list.Detach();
	for(const IP_ADDR_STRING *l = ias; l; l=l->Next) {
		BluePy e(Py_BuildValue("{sssssi}", 
			"IpAddress", l->IpAddress.String,
			"IpMask", l->IpMask.String,
			"Context", l->Context));
		if (!e) return 0;
		if (!list.Append(e)) return 0;
	}
	return list.Detach();
}


PyObject *PyGetAdaptersInfo(PyObject *self, PyObject *args)
{
	ULONG l=0;
	DWORD res = ::GetAdaptersInfo(0, &l);
	if (res != ERROR_BUFFER_OVERFLOW)
		return PyWin32Error("GetAdaptersInfo");
	std::vector<char> buf(l);
	res = ::GetAdaptersInfo((IP_ADAPTER_INFO*)&buf[0], &l);
	if (res != ERROR_SUCCESS) {
		if (res == ERROR_NO_DATA || res == ERROR_NOT_SUPPORTED) {
			Py_INCREF(Py_None);
			return Py_None;
		}
		return PyWin32Error("GetAdaptersInfo");
	}
	IP_ADAPTER_INFO *pi = (IP_ADAPTER_INFO*)&buf[0];
	BluePyList r(0);
	for(; pi; pi = pi->Next) {
		time_t obtained, expires;
#ifndef _WIN64
		//fudge to get 32 bit time.  The struct has 4 bytes, but time_t is 8 bytes
		//we could also use #define _USE_32BIT_TIME_T 
		unsigned __int32 * t = (unsigned __int32*)&pi->LeaseObtained;
		obtained = t[0];
		expires = t[1];
#else
		obtained = pi->LeaseObtained;
		expires = pi->LeaseExpires;
#endif
		BluePy Address(PyString_FromStringAndSize((char*)pi->Address, pi->AddressLength)); if (!Address) return 0;
		BluePy IpAddressList(ParseIP_ADDR_STRING(&pi->IpAddressList)); if (!IpAddressList) return 0;
		BluePy GatewayList(ParseIP_ADDR_STRING(&pi->GatewayList)); if (!GatewayList) return 0;
		BluePy DhcpServer(ParseIP_ADDR_STRING(pi->DhcpEnabled ? &pi->DhcpServer : 0)); if (!DhcpServer) return 0;
		BluePy PrimaryWinsServer(ParseIP_ADDR_STRING(pi->HaveWins ? &pi->PrimaryWinsServer : 0)); if (!PrimaryWinsServer) return 0;
		BluePy SecondaryWinsServer(ParseIP_ADDR_STRING(pi->HaveWins ? &pi->SecondaryWinsServer : 0)); if (!SecondaryWinsServer) return 0;
		BluePy e(Py_BuildValue("{ss ss sO si si sO sO sO sO sO sO sO sK sK}",
			"AdapterName", pi->AdapterName,
			"Description", pi->Description,
			"Address", Address.o,
			"Index", pi->Index,
			"Type", pi->Type,
			"DhcpEnabled", pi->DhcpEnabled ? Py_True : Py_False,
			"IpAddressList", IpAddressList.o,
			"GatewayList", GatewayList.o,
			"DhcpServer", DhcpServer.o,
			"HaveWins", pi->HaveWins ? Py_True : Py_False,
			"PrimaryWinsServer", PrimaryWinsServer.o,
			"SecondaryWinsServer", SecondaryWinsServer.o,
			"LeaseObtained", pi->DhcpEnabled ? obtained : (time_t)0,
			"LeaseExpires", pi->DhcpEnabled ? expires : (time_t)0));
		if (!e) return 0;
		if (!r.Append(e)) return 0;
	}
	return r.Detach();
}

//////////////////////////////////////////////////////////////////////
// CRT Debug Hooks
static PyObject *PyAllocHook = 0;
static bool allowDeny = false;
typedef int (* oldHook_t)(int, void*, size_t, int, long, const unsigned char*, int);
size_t sizeLimit = 0;
static oldHook_t oldHook = 0;
static DWORD threadID = 0;

//This function is exported!!!
BLUEIMPORT int BlueCrtAllocHook( int allocType, void *userData, size_t size, int 
	blockType, long requestNumber, const unsigned char *filename, int lineNumber)
{
	if (GetCurrentThreadId() != threadID)
		return TRUE; //we can only operate on the main thread.

	static bool inThere = false; //reentrancy guard since python may do stuff.
	if (!PyAllocHook || inThere || blockType == _CRT_BLOCK )
		return TRUE;
	if (size  < sizeLimit)
		return TRUE;

	inThere = true;
	PyObject *r;
	const char *fn = (const char*)filename;
	if (!fn)
		fn = "";
	r = PyObject_CallFunction(PyAllocHook, "iiiisi", allocType, size, blockType, requestNumber, fn, lineNumber);
	if (!r) {
		PyOS->PyError();
		inThere = false;
		return TRUE;
	}
	int result = PyObject_IsTrue(r) ? TRUE : FALSE;
	if (!allowDeny)
		result = TRUE;
	Py_DECREF(r);
	inThere = false;
	return result;
}


PyObject *Py_CrtSetAllocHook(PyObject *self, PyObject *args)
{
	PyObject *callable = Py_None;
	PyObject *allowDenyO = Py_False;
	if (!PyArg_ParseTuple(args, "|OiO:_CrtSetAllocHook", &callable, &sizeLimit, &allowDenyO))
		return 0;
	
	PyObject *old = PyAllocHook;
	if (!old) {
		old = Py_None;
		Py_INCREF(old);
	}
	PyAllocHook = 0;
	if (callable != Py_None) {
		PyAllocHook = callable;
		allowDeny = !!PyObject_IsTrue(allowDenyO);
		Py_INCREF(callable);
		if (!oldHook)
			oldHook = _CrtSetAllocHook(BlueCrtAllocHook);
		threadID = GetCurrentThreadId();
	} else {
		if (oldHook) {
			_CrtSetAllocHook(oldHook);
			oldHook = 0;
		}
	}
	return old;
}


PyObject *PySHGetFolderPath(PyObject *self, PyObject *args)
{
	int csidl;
	if (!PyArg_ParseTuple(args, "i:SHGetFolderPath", &csidl))
		return 0;

	wchar_t path[MAX_PATH];
	HRESULT hr = SHGetFolderPathW(NULL, csidl, NULL, SHGFP_TYPE_CURRENT, path);
	if (SUCCEEDED(hr))
		return PyUnicode_FromWideChar(path, wcslen(path));
	if (hr == E_FAIL) {
		Py_INCREF(Py_None);
		return Py_None; //folder doesn't exist;
	}
	if (hr == E_INVALIDARG)
		return PyErr_SetString(PyExc_ValueError, "invalid CSIDL value"), 0;
	if (HRESULT_FACILITY(hr) == FACILITY_WIN32) {
		SetLastError(HRESULT_CODE(hr));
	return PyWin32Error("SHGetFolderPath");
}
	PyErr_Format(PyExc_RuntimeError, "unexpected return from SHGetFolderPath: %x", hr);
	return 0;
}

PyObject *PyInitializeCom(PyObject *self, PyObject *args)
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED  | COINIT_DISABLE_OLE1DDE );
	if ( SUCCEEDED(hr) )
	{
		Py_RETURN_TRUE;
	}
	Py_RETURN_FALSE;
}

PyObject *PyUnInitializeCom(PyObject *self, PyObject *args)
{
	CoUninitialize();
	Py_RETURN_NONE;
}

PyObject *PyBitsQueueDownload(PyObject *self, PyObject *args)
{
	LPWSTR name;
	LPWSTR url;
	LPWSTR destination;

	if (!PyArg_ParseTuple(args, "uuu:QueueDownload", &name, &url, &destination))
		return NULL;

	CComPtr<IBackgroundCopyManager> XferManager;  
	HRESULT hr = CoCreateInstance(__uuidof(BackgroundCopyManager), NULL, CLSCTX_LOCAL_SERVER, __uuidof(IBackgroundCopyManager),(void**) &XferManager);	
	if( FAILED(hr))
	{	
		return PyErr_SetString(PyExc_RuntimeError, "CoCreateInstance"), 0;	
	}

	GUID JobID;
	CComPtr<IBackgroundCopyJob> job;
	hr = XferManager->CreateJob( name, BG_JOB_TYPE_DOWNLOAD, &JobID, &job);	
	if( FAILED(hr))
	{
		return PyErr_SetString(PyExc_RuntimeError, "IBackgroundCopyManager::CreateJob"), 0;
	}

	hr = job->SetPriority( BG_JOB_PRIORITY_FOREGROUND );
	if( FAILED(hr))
	{
		return PyErr_SetString(PyExc_RuntimeError, "IBackgroundCopyJob::SetPriority"), 0;
	}

	hr =  job->AddFile( url, destination );
	if (FAILED(hr))
	{
		return PyErr_SetString(PyExc_RuntimeError, "IBackgroundCopyJob::AddFile"), 0;
	}

	hr = job->Resume();
	if ( FAILED(hr) )
	{
		return PyErr_SetString(PyExc_RuntimeError, "IBackgroundCopyJob::Resume"), 0;
	}	

	Py_RETURN_NONE;
}

PyObject *PyBitsGetDownloadStatus(PyObject *self, PyObject *args)
{
	LPWSTR jobname;

	if (!PyArg_ParseTuple(args, "u:GetDownloadStatus", &jobname))
		return NULL;

	CComPtr<IBackgroundCopyManager> XferManager;
	HRESULT hr = CoCreateInstance(__uuidof(BackgroundCopyManager), NULL, CLSCTX_LOCAL_SERVER, __uuidof(IBackgroundCopyManager),(void**) &XferManager);
	if(FAILED(hr))
	{
		return PyErr_SetString(PyExc_RuntimeError, "CoCreateInstance"), 0;
	}

	CComPtr<IEnumBackgroundCopyJobs> jobs;
	hr = XferManager->EnumJobs(0, &jobs);
	if(FAILED(hr))
	{
		return PyErr_SetString(PyExc_RuntimeError, "EnumJobs"), 0;
	}

	ULONG jobCount = 0;
	jobs->GetCount( &jobCount );
	BluePy dict(PyDict_New());

	for(ULONG i=0; i<jobCount; i++)
	{
		CComPtr<IBackgroundCopyJob> job;
		hr = jobs->Next(1, &job, NULL);
		if(FAILED(hr))
		{
			return PyErr_SetString(PyExc_RuntimeError, "IEnumBackgroundCopyJobs::Next"), 0;
		}

		CComHeapPtr<wchar_t> displayName;
		job->GetDisplayName( &displayName );

		if (  wcscmp( displayName, jobname ) == 0 )
		{
			BG_JOB_PROGRESS progress;			
			hr = job->GetProgress( &progress );
			if(FAILED(hr))
			{
				return PyErr_SetString(PyExc_RuntimeError, "IBackgroundCopyJob::GetProgress"), 0;
			}

			PyDict_SetItemString( dict,  "BytesTransfered", BluePy(PyLong_FromLongLong( progress.BytesTransferred)));
			PyDict_SetItemString( dict,  "BytesTotal", BluePy(PyLong_FromLongLong( progress.BytesTotal)));
			PyDict_SetItemString( dict,  "FilesTotal", BluePy(PyInt_FromLong( progress.FilesTotal)));
			PyDict_SetItemString( dict,  "FilesTransferred", BluePy(PyInt_FromLong( progress.FilesTransferred)));

			BG_JOB_STATE state;
			hr = job->GetState( &state );

			if(FAILED(hr))
			{
				return PyErr_SetString(PyExc_RuntimeError, "IBackgroundCopyJob::GetState"), 0;
			}
			PyDict_SetItemString( dict, "State", BluePy(PyInt_FromLong( state )));
			
			BG_JOB_PRIORITY priority;
			hr = job->GetPriority( &priority );
			if(FAILED(hr))
			{
				return PyErr_SetString(PyExc_RuntimeError, "IBackgroundCopyJob::GetPriority"), 0;
			}
			PyDict_SetItemString( dict, "Priority", BluePy(PyInt_FromLong( priority )));
		}
	}

	return dict.Detach();
}

PyObject *PyBitsAction(PyObject *self, PyObject *args)
{
	LPWSTR jobname;
	LPWSTR action;

	if (!PyArg_ParseTuple(args, "uu:BitsAction", &jobname, &action))
		return NULL;

	CComPtr<IBackgroundCopyManager> XferManager;
	HRESULT hr = CoCreateInstance(__uuidof(BackgroundCopyManager), NULL, CLSCTX_LOCAL_SERVER, __uuidof(IBackgroundCopyManager),(void**) &XferManager);
	if(FAILED(hr))
	{
		return PyErr_SetString(PyExc_RuntimeError, "CoCreateInstance"), 0;	
	}

	CComPtr<IEnumBackgroundCopyJobs> jobs;

	hr = XferManager->EnumJobs(0, &jobs);
	if(FAILED(hr))
	{
		return PyErr_SetString(PyExc_RuntimeError, "IBackgroundCopyManager::EnumJobs"), 0;	
	}

	ULONG jobCount = 0;
	jobs->GetCount( &jobCount );
	long completed = 0;
	
	for(ULONG i=0; i<jobCount; i++)
	{
		CComPtr<IBackgroundCopyJob> job;
		hr = jobs->Next(1, &job, NULL);
		if(hr != S_OK)
		{
			return PyErr_SetString(PyExc_RuntimeError, "IEnumBackgroundCopyJobs::Next"), 0;	
		}

		CComHeapPtr<wchar_t> displayName;
		job->GetDisplayName( &displayName );

		if (  wcscmp( displayName, jobname ) == 0 )
		{
			// Start with checking the state
			BG_JOB_STATE state;
			hr = job->GetState( &state );

			if(FAILED(hr))
			{
				return PyErr_SetString(PyExc_RuntimeError, "IBackgroundCopyJob::GetState"), 0;	
			}

			if ( wcscmp( action, L"complete" ) == 0)
			{
				if ( BG_JOB_STATE_TRANSFERRED == state )
				{
					hr = job->Complete();
					if(FAILED(hr))
					{
						return PyErr_SetString(PyExc_RuntimeError, "IBackgroundCopyJob::Complete"), 0;	
					}
					if (hr == S_OK)
						completed++;
				}
			}

			if ( wcscmp( action, L"cancel" ) == 0)
			{
				if ( BG_JOB_STATE_CANCELLED != state )
				{
					hr = job->Cancel();
					if(FAILED(hr))
					{
						return PyErr_SetString(PyExc_RuntimeError, "IBackgroundCopyJob::Cancel"), 0;	
					}
					completed++;
				}
			}

			if ( wcscmp( action, L"suspend" ) == 0)
			{
				if ( BG_JOB_STATE_TRANSFERRING == state || BG_JOB_STATE_CONNECTING == state )
				{
					hr = job->Suspend();
					if(FAILED(hr))
					{
						return PyErr_SetString(PyExc_RuntimeError, "IBackgroundCopyJob::Suspend"), 0;	
					}
					completed++;
				}
			}

			if ( wcscmp( action, L"resume" ) == 0)
			{
				if ( BG_JOB_STATE_SUSPENDED == state)
				{
					hr = job->Resume();
					if(FAILED(hr))
					{
						return PyErr_SetString(PyExc_RuntimeError, "IBackgroundCopyJob::Resume"), 0;	
					}
					completed++;
				}
			}

			if ( wcscmp( action, L"foregroundPriority" ) == 0)
			{
				hr = job->SetPriority(BG_JOB_PRIORITY_FOREGROUND);
				if(FAILED(hr))
				{
					return PyErr_SetString(PyExc_RuntimeError, "IBackgroundCopyJob::SetPriority"), 0;	
				}
				completed++;
			}

			if ( wcscmp( action, L"highPriority" ) == 0)
			{
				hr = job->SetPriority(BG_JOB_PRIORITY_HIGH);
				if(FAILED(hr))
				{
					return PyErr_SetString(PyExc_RuntimeError, "IBackgroundCopyJob::SetPriority"), 0;	
				}
				completed++;
			}

			if ( wcscmp( action, L"mediumPriority" ) == 0)
			{
				hr = job->SetPriority(BG_JOB_PRIORITY_NORMAL);
				if(FAILED(hr))
				{
					return PyErr_SetString(PyExc_RuntimeError, "IBackgroundCopyJob::SetPriority"), 0;	
				}
				completed++;
			}

			if ( wcscmp( action, L"lowPriority" ) == 0)
			{
				hr = job->SetPriority(BG_JOB_PRIORITY_LOW);
				if(FAILED(hr))
				{
					return PyErr_SetString(PyExc_RuntimeError, "IBackgroundCopyJob::SetPriority"), 0;	
				}
				completed++;
			}
		}
	}

	return PyInt_FromLong(completed);
}

PyObject *PyGetWindowsServiceStatus(PyObject *self, PyObject *args)
{
	LPSTR servicename;
	if (!PyArg_ParseTuple(args, "s:GetWindowsServiceStatus", &servicename))
		return NULL;

	SC_HANDLE SCManager = OpenSCManager( NULL, NULL, SERVICE_QUERY_CONFIG );
	if (NULL==SCManager)
	{
		return PyWin32Error("OpenSCManager");	
	}

	SC_HANDLE service = OpenService( SCManager, servicename, SERVICE_QUERY_CONFIG); 
	if (NULL==service)
	{ 
		if ( ERROR_SERVICE_DOES_NOT_EXIST == GetLastError() )
		{
			CloseServiceHandle(SCManager);
			Py_RETURN_NONE;
		}

		CloseServiceHandle(SCManager);
		return PyWin32Error("OpenService");	
    }

	DWORD dwBytesNeeded = 0;
	DWORD cbBufSize = 0;
	CHeapPtr<char> ptr;
	LPQUERY_SERVICE_CONFIG config = 0; 
	
	if( !QueryServiceConfig( service, NULL,0, &dwBytesNeeded))
    {
        if( ERROR_INSUFFICIENT_BUFFER == GetLastError() )
        {
			cbBufSize = dwBytesNeeded;
			if (!ptr.Allocate(cbBufSize)) {
				CloseServiceHandle(service);
				CloseServiceHandle(SCManager);
				return PyErr_NoMemory();
			}
			config = (LPQUERY_SERVICE_CONFIG)(char*)ptr;
        }
        else
        {
			CloseServiceHandle(service);
			CloseServiceHandle(SCManager);
			return PyWin32Error("QueryServiceConfig");
        }
    }

	if( !QueryServiceConfig(service,config,cbBufSize,&dwBytesNeeded) ) 
    {
		CloseServiceHandle(service);
		CloseServiceHandle(SCManager);
		return PyWin32Error("QueryServiceConfig");
    }

	CloseServiceHandle(service);
	CloseServiceHandle(SCManager);

	return PyInt_FromSize_t(config->dwStartType);
}


/*******************************************************************************
 * Minidump generation
 */
PyObject *PyMiniDumpWriteDump(PyObject *self, PyObject *args)
{
	PyObject *fnO;
	MINIDUMP_TYPE dt = MiniDumpNormal;
	if (!PyArg_ParseTuple(args, "O|i:MiniDumpWriteDump", &fnO, &dt))
		return 0;
	HMODULE hm = LoadLibrary("DbgHelp.dll");
	if (!hm)
		return PyWin32Error("LoadLibrary");
	PyObject *fnu = PyUnicode_FromObject(fnO);
	if (!fnu) {
		FreeLibrary(hm);
		return 0;
	}
	typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(
		HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
		CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
		CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
		CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam
		);
	MINIDUMPWRITEDUMP dump = (MINIDUMPWRITEDUMP)GetProcAddress(hm, "MiniDumpWriteDump" );
	if (!dump) {
		FreeLibrary(hm);
		return PyWin32Error("GetProcAddress");
	}

	HANDLE file = CreateFileW(PyUnicode_AS_UNICODE(fnu), GENERIC_READ|GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		FreeLibrary(hm);
		return PyWin32Error("CreateFile");
	}

	BOOL ok = dump(GetCurrentProcess(), GetCurrentProcessId(), file, dt, 0, 0, 0);
	FreeLibrary(hm);
	CloseHandle(file);
	if (!ok)
		return PyWin32Error("MiniDumpWriteDump");
	Py_INCREF(Py_None);
	return Py_None;
}



/*******************************************************************************
 * Win32 Error handling
 */
typedef TrackableStdMap<DWORD, PyObject *> exceptions_t;
static exceptions_t exceptions( "exceptions" );

PyObject *DefineError(PyObject *module, PyObject *base, const char *prefix, const char *bname, DWORD code)
{
	const std::string name = prefix + std::string(bname);
	const std::string fname = "blue.win32." + name;
	PyObject *ex = PyErr_NewException((char*)fname.c_str(), base, 0);
	if (!ex) return 0;
	if (PyModule_AddObject(module, (char*)name.c_str(), ex)) return 0;
	exceptions.insert(exceptions_t::value_type(code, ex)); //store a weak ref to it here, so that crt exit doesn't cause trouble
	return ex; //returns borrowed ref
}

bool DefineErrors(PyObject* module)
{
	PyObject *base = PyExc_WindowsError;
#define ADD(_E) if (!DefineError(module, base, "ERROR_", #_E, ERROR_ ## _E)) return false
	ADD(INVALID_FUNCTION);
	ADD(FILE_NOT_FOUND);
	ADD(PATH_NOT_FOUND);
	ADD(ACCESS_DENIED);
	ADD(INVALID_HANDLE);
	ADD(ARENA_TRASHED);
	ADD(NOT_ENOUGH_MEMORY);
	ADD(INVALID_BLOCK);
	ADD(BAD_ENVIRONMENT);
	ADD(BAD_FORMAT);
	ADD(INVALID_ACCESS);
	ADD(INVALID_DATA);
	ADD(OUTOFMEMORY);
	ADD(INVALID_DRIVE);
	ADD(CURRENT_DIRECTORY);
	ADD(WRITE_PROTECT);
	ADD(NOT_SUPPORTED);
	ADD(INVALID_PARAMETER);
	ADD(OPEN_FAILED);
	ADD(DISK_FULL);
	ADD(CALL_NOT_IMPLEMENTED);
	ADD(INVALID_NAME);
	ADD(MOD_NOT_FOUND);
	ADD(PROC_NOT_FOUND);
	ADD(BAD_ARGUMENTS);
	ADD(BUSY);
	ADD(MORE_DATA);

	//Stuff from CryptoAPI
#undef ADD
	#define ADD(_E) if (!DefineError(module, base, "NTE_", #_E, NTE_ ## _E)) return false
	ADD(BAD_ALGID);
	ADD(BAD_DATA);
	ADD(BAD_FLAGS);
	ADD(BAD_HASH);
	ADD(BAD_HASH_STATE);
	ADD(BAD_KEY);
	ADD(BAD_KEY_STATE);
	ADD(BAD_KEYSET);
	ADD(BAD_KEYSET_PARAM);
	ADD(BAD_LEN);
	ADD(BAD_UID);
	ADD(BAD_VER);
	ADD(BAD_TYPE);
	ADD(BAD_PROV_TYPE);
	ADD(BAD_SIGNATURE);
	ADD(BAD_PUBLIC_KEY);
	ADD(DOUBLE_ENCRYPT);
	ADD(FAIL);
	ADD(NO_MEMORY);
	ADD(NO_KEY);
	ADD(EXISTS);
	ADD(KEYSET_ENTRY_BAD);
	ADD(KEYSET_NOT_DEF);
	ADD(PROV_DLL_NOT_FOUND);
	ADD(PROV_TYPE_ENTRY_BAD);
	ADD(PROV_TYPE_NO_MATCH);
	ADD(PROV_TYPE_NOT_DEF);
	ADD(PROVIDER_DLL_FAIL);
	ADD(SIGNATURE_FILE_BAD);
	ADD(SILENT_CONTEXT);

	return true;
}

bool DefineConsts(PyObject *m) {
#define I(c) if (PyModule_AddIntConstant(m, #c, c)) return false
	I(_HOOK_ALLOC);
	I(_HOOK_REALLOC);
	I(_HOOK_FREE);
	I(_FREE_BLOCK);
	I(_NORMAL_BLOCK);
	I(_CRT_BLOCK);
	I(_IGNORE_BLOCK);
	I(_CLIENT_BLOCK);
	//for Minidumps
	I(MiniDumpNormal);
	I(MiniDumpWithDataSegs);
	I(MiniDumpWithFullMemory);
	I(MiniDumpWithUnloadedModules);
	I(MiniDumpWithIndirectlyReferencedMemory);
	I(MiniDumpWithProcessThreadData);
	I(MiniDumpWithIndirectlyReferencedMemory);
	I(MiniDumpWithPrivateReadWriteMemory);
	//I(MiniDumpWithoutOptionalData);
	//I(MiniDumpWithFullMemoryInfo);
	//I(MiniDumpWithThreadInfo);
	//I(MiniDumpWithCodeSegs);

	//shell api
	I(CSIDL_FLAG_CREATE);	
	I(CSIDL_ADMINTOOLS);
	I(CSIDL_COMMON_ADMINTOOLS);
	I(CSIDL_APPDATA);
	I(CSIDL_COMMON_APPDATA);
	I(CSIDL_COMMON_DOCUMENTS);
	I(CSIDL_COOKIES);
	I(CSIDL_FLAG_CREATE);
	I(CSIDL_HISTORY);
	I(CSIDL_INTERNET_CACHE);
	I(CSIDL_LOCAL_APPDATA);
	I(CSIDL_MYPICTURES);
	I(CSIDL_PERSONAL);
	I(CSIDL_PROGRAM_FILES);
	I(CSIDL_PROGRAM_FILES_COMMON);
	I(CSIDL_SYSTEM);
	I(CSIDL_WINDOWS);
	I(CSIDL_FONTS);
	I(CSIDL_MYDOCUMENTS);
	I(CSIDL_MYMUSIC);
	I(CSIDL_MYVIDEO);

	//processor features, see IsProcessorFeaturePresent()
	I(PF_3DNOW_INSTRUCTIONS_AVAILABLE);
	I(PF_COMPARE_EXCHANGE_DOUBLE);
	I(PF_FLOATING_POINT_EMULATED);
	I(PF_FLOATING_POINT_PRECISION_ERRATA);
	I(PF_MMX_INSTRUCTIONS_AVAILABLE);
	I(PF_PAE_ENABLED);
	I(PF_RDTSC_INSTRUCTION_AVAILABLE);
	I(PF_XMMI_INSTRUCTIONS_AVAILABLE);
	I(PF_XMMI64_INSTRUCTIONS_AVAILABLE);

#if _MSC_VER >= 1400 //following only supported in VC8
	// I(PF_CHANNELS_ENABLED);
	// I(PF_COMPARE_EXCHANGE128);
	// I(PF_COMPARE64_EXCHANGE128);
	I(PF_NX_ENABLED);
	// I(PF_SSE3_INSTRUCTIONS_AVAILABLE);
#endif
	
	//for GetVersionInfoEx
	I(VER_SUITE_BACKOFFICE);
	I(VER_SUITE_BLADE);
	I(VER_SUITE_DATACENTER);
	I(VER_SUITE_ENTERPRISE);
	I(VER_SUITE_EMBEDDEDNT);
	I(VER_SUITE_PERSONAL);
	I(VER_SUITE_SINGLEUSERTS);
	I(VER_SUITE_SMALLBUSINESS);
	I(VER_SUITE_SMALLBUSINESS_RESTRICTED);
	I(VER_SUITE_TERMINAL);
	I(VER_NT_DOMAIN_CONTROLLER);
	I(VER_NT_SERVER);
	I(VER_NT_WORKSTATION);


	
	
	return true;
}
	

}; //namespace


PyObject *PyWin32Error(const char *msg, DWORD ierror)
{
	PyObject *cls = PyExc_WindowsError;
	if (!ierror)
		ierror = GetLastError();
	exceptions_t::iterator i = exceptions.find(ierror);
	if (i != exceptions.end())
		cls = i->second;

	if (msg)
		PyErr_SetExcFromWindowsErrWithFilename(cls, ierror, msg);
	else
		PyErr_SetExcFromWindowsErr(cls, ierror);
	return 0;
}

//initialize the module, man
void
::initwin32(void)
{
	loader = CCP_NEW("initwin32/loader") SoftLoader;
	PyObject *win32 = Py_InitModule("blue.win32", methods);
	Py_InitModule("blue.heapq", heapqmethods);
	DefineErrors(win32);
	DefineConsts(win32);
}

#endif
#endif
