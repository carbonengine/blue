// Copyright © 2014 CCP ehf.

//This file defines the blue.win32 module which exposes some quite useful win32 apis to the app


#include "StdAfx.h"

#ifdef _WIN32

#include <winsock2.h>
#include <ntverp.h>
#include <Iphlpapi.h>

#include <shellapi.h>

#include <atlbase.h>
#include <psapi.h>
#include <shlobj.h> //shell32 api

#include <string>
#include <map>
#include <vector>

#include <DbgHelp.h>
#include <bits.h>



bool GetWindowsVersionFromFile( OSVERSIONINFOEX& info )
{
	wchar_t folder[MAX_PATH];
	if( FAILED( SHGetFolderPathW( nullptr, CSIDL_SYSTEM, nullptr, SHGFP_TYPE_CURRENT, folder ) ) )
	{
		return false;
	}
	std::wstring path( folder );
	path += L"\\kernel32.dll";
	DWORD handle = 0;
	auto size = GetFileVersionInfoSizeW( path.c_str(), &handle );
	if( !size )
	{
		return false;
	}
	std::unique_ptr<char[]> versionInfo( new char[size] );
	if( !GetFileVersionInfoW( path.c_str(), handle, size, versionInfo.get() ) )
	{
		return false;
	}


	struct LANGANDCODEPAGE 
	{
		  WORD language;
		  WORD codePage;
	} *translate;
	UINT translateSize;

	if( !VerQueryValueW( versionInfo.get(), L"\\VarFileInfo\\Translation", reinterpret_cast<LPVOID*>( &translate ), &translateSize ) )
	{
		return false;
	}

	for( UINT i = 0; i < translateSize / sizeof( LANGANDCODEPAGE ); ++i )
	{
		wchar_t subBlock[512];
		swprintf_s( subBlock, L"\\StringFileInfo\\%04x%04x\\ProductVersion", translate[i].language, translate[i].codePage );
		wchar_t *version;
		UINT size;
		if( VerQueryValueW( versionInfo.get(), subBlock, reinterpret_cast<LPVOID*>( &version ), &size ) )
		{
			unsigned major, minor, build;
			if( swscanf_s( version, L"%u.%u.%u", &major, &minor, &build ) != 3 )
			{
				return false;
			}
			info.dwMajorVersion = major;
			info.dwMinorVersion = minor;
			info.dwBuildNumber = build;
			info.dwPlatformId = VER_PLATFORM_WIN32_NT;
			info.szCSDVersion[0] = 0;
			info.wServicePackMajor = 0;
			info.wServicePackMinor = 0;
			info.wSuiteMask = 0;
			info.wProductType = VER_NT_WORKSTATION;
			return true;
		}
	}
	return false;
}

void GetWindowsVersionFromApi( OSVERSIONINFOEX &info )
{
#pragma warning( push )
	// Mute deprecation warning for GetVersionEx - there's no clear alternative for it for Windows 7+
#pragma warning( disable : 4996 )
	memset( &info, 0, sizeof( info ) );
	info.dwOSVersionInfoSize = sizeof( info );
	if( !GetVersionEx( reinterpret_cast<OSVERSIONINFO*>( &info ) ) )
	{
		info.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
		if( !GetVersionEx( reinterpret_cast<OSVERSIONINFO*>( &info ) ) )
		{
			auto ver = GetVersion();
			info.dwMajorVersion = LOBYTE( LOWORD( ver ) );
			info.dwMinorVersion = HIBYTE( LOWORD( ver ) );
			if( ver < 0x80000000 )
			{
				info.dwBuildNumber = HIWORD( ver );
			}
		}
	}
#pragma warning( pop )
}

#if BLUE_WITH_PYTHON

#include "win32.h"

#include "Blue.h"
#include "IBluePython.h"


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
	LOAD("kernel32.dll", GetProcessWorkingSetSize);
	LOAD("kernel32.dll", SetProcessWorkingSetSize);
	LOAD("kernel32.dll", GetProcessIoCounters);
	LOAD("kernel32.dll", GetNativeSystemInfo);
	LOAD("kernel32.dll", GlobalMemoryStatusEx);
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
		if( !h )
			return 0; // cppcheck-suppress resourceLeak
		return GetProcAddress( h, function ); // cppcheck-suppress resourceLeak
	}

//typedefs
typedef BOOL (WINAPI *GetProcessMemoryInfo_t)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
typedef BOOL (WINAPI *GetProcessWorkingSetSize_t)(HANDLE, PSIZE_T, PSIZE_T);
typedef BOOL (WINAPI *SetProcessWorkingSetSize_t)(HANDLE, SIZE_T, SIZE_T);
typedef BOOL (WINAPI *GetProcessIoCounters_t)(HANDLE, PIO_COUNTERS);
typedef void (WINAPI *GetNativeSystemInfo_t)(LPSYSTEM_INFO);
typedef BOOL (WINAPI *GlobalMemoryStatusEx_t)(LPMEMORYSTATUSEX);
typedef DWORD (WINAPI *GetExtendedTcpTable_t)(PVOID, PDWORD, BOOL, ULONG, TCP_TABLE_CLASS, ULONG);    
typedef ULONG (WINAPI *SetPerTcpConnectionEStats_t)(PMIB_TCPROW, DWORD, PUCHAR, ULONG, ULONG, ULONG);
typedef ULONG (WINAPI *GetPerTcpConnectionEStats_t) (PMIB_TCPROW, DWORD, PUCHAR, ULONG, ULONG,
												     PUCHAR, ULONG, ULONG, PUCHAR, ULONG, ULONG);



#define DEF(N) \
	N##_t m##N;\
	N##_t N() {Init(); return m##N;}

DEF(GetProcessMemoryInfo)
DEF(GetProcessWorkingSetSize)
DEF(SetProcessWorkingSetSize)
DEF(GetProcessIoCounters)
DEF(GetNativeSystemInfo)
DEF(GlobalMemoryStatusEx)
DEF(GetExtendedTcpTable)
DEF(SetPerTcpConnectionEStats)
DEF(GetPerTcpConnectionEStats)
#undef DEF
};
SoftLoader *loader = 0;


//Python function prototypes:
#define PROTO(N) PyObject *Py##N(PyObject *self, PyObject *args);
PROTO(GetFileVersionInfo)

//registry stuff
PROTO(RegistryGetValue)

//memory and time queries
PROTO(GlobalMemoryStatus)
PROTO(GetProcessMemoryInfo)
PROTO(GetProcessWorkingSetSize)
PROTO(SetProcessWorkingSetSize)
PROTO(GetProcessIoCounters)
PROTO(GetSystemTimeAsFileTime)

//network queries
PROTO(ToggleTcpEStats)
PROTO(GetProcessTcpEStats)

//process info and stuff
PROTO(GetSystemInfo)
PROTO(GetNativeSystemInfo)

//IP stuff
PROTO(GetAdaptersInfo)

//a single test function
PROTO(Test)

#undef PROTO

//pythoh method definitions
PyMethodDef methods[] = {
#define DEF(N) {#N, Py##N, METH_VARARGS},
DEF(GetFileVersionInfo)
DEF(RegistryGetValue)
DEF(GlobalMemoryStatus)
DEF(GetProcessMemoryInfo)
DEF(GetProcessWorkingSetSize)
DEF(SetProcessWorkingSetSize)
DEF(GetProcessIoCounters)
DEF(GetSystemTimeAsFileTime)
DEF(ToggleTcpEStats)
DEF(GetProcessTcpEStats)
DEF(GetSystemInfo)
DEF(GetNativeSystemInfo)
DEF(GetAdaptersInfo)

DEF(Test)
#undef DEF
{0}
};


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

	const char *keys[] = {"Comments", "InternalName", "ProductName", "CompanyName", "LegalCopyright", "ProductVersion", 
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
					d.Set(key, BluePy(PyUnicode_FromString((char*)result)));
			}
		}
	}
	CCP_DELETE[] buffer;
	return d.Detach();
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
		resultO = PyBytes_FromStringAndSize(buffer, valueLen);
		break;
	case REG_DWORD:
//	case REG_DWORD_LITTLE_ENDIAN:
		resultO = PyLong_FromLong(*(DWORD*)buffer);
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
		resultO = PyLong_FromLong(r);
		break; }
	case REG_SZ:
	case REG_EXPAND_SZ:
	{
		auto str = reinterpret_cast<const wchar_t*>( buffer );
		auto size = valueLen / sizeof( wchar_t );
		if( size > 0 && str[size - 1] == 0 )
		{
			--size;
		}
		resultO = PyUnicode_FromWideChar( str, size );
		break;
	}
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
		const wchar_t *start = reinterpret_cast<const wchar_t*>( buffer ), *next;
		for(;;) {
			next = wcschr( start, 0 );
			if (next==start)
				break;
			PyObject *str = PyUnicode_FromWideChar( start, next - start - 1 );
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
		resultO = PyUnicode_FromStringAndSize(buffer, valueLen);
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
	result = RegQueryValueExW(hKey, CA2W( valueName ), 0, &valueType, (LPBYTE)buffer, &valueLen);
	if (result != ERROR_SUCCESS) {
		CCP_DELETE [] buffer;
		return PyErr_SetFromWindowsErr(result);
	}
	PyObject *resultO = ValueToPython(buffer, valueLen, valueType);
	CCP_DELETE[] buffer;
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


//process and thread query functions

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
	{
		PyErr_SetString(PyExc_NotImplementedError, "not availible on this platform");
		return nullptr;
	}

	SIZE_T min = 0, max = 0;
	BOOL ok = loader->GetProcessWorkingSetSize()(GetCurrentProcess(), &min, &max);
	if (!ok)
		return PyWin32Error();

	return Py_BuildValue("NN", PyLong_FromSize_t(min), PyLong_FromSize_t(max));
}


PyObject *PySetProcessWorkingSetSize(PyObject *self, PyObject *args)
{
	SIZE_T wMin = -1; // default -1, -1 trims the working set temporarily
	SIZE_T wMax = -1;

	if (!PyArg_ParseTuple(args, "|nn:SetProcessWorkingSetSize", &wMin, &wMax))
		return 0;

	if (!loader->SetProcessWorkingSetSize())
	{
		PyErr_SetString( PyExc_NotImplementedError, "not availible on this platform" );
		return nullptr;
	}
	
	if (!loader->SetProcessWorkingSetSize()(GetCurrentProcess(), wMin, wMax))
		return PyWin32Error();

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject *PyGetProcessIoCounters(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetProcessIoCounters"))
		return 0;

	if (!loader->GetProcessIoCounters())
	{
		PyErr_SetString( PyExc_NotImplementedError, "not availible on this platform" );
		return nullptr;
	}

	IO_COUNTERS counters;
	BOOL ok = loader->GetProcessIoCounters()(GetCurrentProcess(), &counters);
	if (!ok)
		return PyWin32Error();

	return Py_BuildValue("{sN sN sN sN sN sN}",
		"ReadOperationCount", PyLong_FromUnsignedLongLong(counters.ReadOperationCount),
		"WriteOperationCount", PyLong_FromUnsignedLongLong(counters.WriteOperationCount),
		"OtherOperationCount", PyLong_FromUnsignedLongLong(counters.OtherOperationCount),
		"ReadTransferCount", PyLong_FromUnsignedLongLong(counters.ReadTransferCount),
		"WriteTransferCount", PyLong_FromUnsignedLongLong(counters.WriteTransferCount),
		"OtherTransferCount", PyLong_FromUnsignedLongLong(counters.OtherTransferCount));
}

PyObject *PyGetSystemTimeAsFileTime(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetSystemTimeAsFileTime"))
		return 0;

	ULARGE_INTEGER ft;
	GetSystemTimeAsFileTime((LPFILETIME)&ft);
	return PyLong_FromUnsignedLongLong(ft.QuadPart);
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
		
#pragma warning( push )
		// Mute deprecation warning for inet_ntoa - there's no clear alternative for it for Windows 7+
#pragma warning( disable : 4996 )
		struct in_addr ipAddr;
		ipAddr.S_un.S_addr = ( u_long ) tcpTable->table[i].dwLocalAddr;
		PyDict_SetItemString( valueDictionary, "localAddr", tmp = PyUnicode_FromString( inet_ntoa( ipAddr ) ) );  Py_DECREF( tmp );
		PyDict_SetItemString( valueDictionary, "localPort", tmp = PyLong_FromLong( ntohs( ( u_short ) tcpTable->table[i].dwLocalPort ) ) );  Py_DECREF( tmp );

		ipAddr.S_un.S_addr = ( u_long ) tcpTable->table[i].dwRemoteAddr;
		PyDict_SetItemString( valueDictionary, "remoteAddr", tmp = PyUnicode_FromString( inet_ntoa( ipAddr ) ) );  Py_DECREF( tmp );
		PyDict_SetItemString( valueDictionary, "remotePort", tmp = PyLong_FromLong( ntohs( ( u_short ) tcpTable->table[i].dwRemotePort ) ) );  Py_DECREF( tmp );
#pragma warning( pop )

		TCP_ESTATS_DATA_ROD_v0 connData = {};
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

		TCP_ESTATS_SND_CONG_ROD_v0 congData = {};
		retValue = loader->GetPerTcpConnectionEStats()( ( MIB_TCPROW* ) &tcpTable->table[i], TcpConnectionEstatsSndCong,
			                                            NULL, 0, 0, NULL, 0, 0, ( PUCHAR ) &congData, 0, sizeof( congData ) );
		if( retValue == NO_ERROR )
		{
			PyObject* congDataDict = PyDict_New();
			PyDict_SetItemString( valueDictionary, "congData", congDataDict );

			PyDict_SetItemString( congDataDict, "SndLimTransRwin", tmp = PyLong_FromUnsignedLong( congData.SndLimTransRwin ) );  Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "SndLimTimeRwin",  tmp = PyLong_FromUnsignedLong( congData.SndLimTimeRwin ) );   Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "SndLimBytesRwin", tmp = PyLong_FromSize_t( congData.SndLimBytesRwin ) );       Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "SndLimTransCwnd", tmp = PyLong_FromUnsignedLong( congData.SndLimTransCwnd ) );  Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "SndLimTimeCwnd",  tmp = PyLong_FromUnsignedLong( congData.SndLimTimeCwnd ) );   Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "SndLimBytesCwnd", tmp = PyLong_FromSize_t( congData.SndLimBytesCwnd ) );       Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "SndLimTransSnd",  tmp = PyLong_FromUnsignedLong( congData.SndLimTransSnd ) );   Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "SndLimTimeSnd",   tmp = PyLong_FromUnsignedLong( congData.SndLimTimeSnd ) );    Py_DECREF( tmp );
			PyDict_SetItemString( congDataDict, "SndLimBytesSnd",  tmp = PyLong_FromSize_t( congData.SndLimBytesSnd ) );        Py_DECREF( tmp );
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

		TCP_ESTATS_PATH_ROD_v0 pathData = {};
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

		TCP_ESTATS_REC_ROD_v0 recStats = {};
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
			PyDict_SetItemString( recDataDict, "CurAppRQueue",   tmp = PyLong_FromSize_t( recStats.CurAppRQueue ) );        Py_DECREF( tmp );
			PyDict_SetItemString( recDataDict, "MaxAppRQueue",   tmp = PyLong_FromSize_t( recStats.MaxAppRQueue ) );        Py_DECREF( tmp );
			PyDict_SetItemString( recDataDict, "WinScaleSent",   tmp = PyLong_FromUnsignedLong( recStats.WinScaleSent ) );   Py_DECREF( tmp );
			
			Py_DECREF( recDataDict );
		}

		TCP_ESTATS_BANDWIDTH_ROD_v0 bwidthStats = {};
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
		BluePy Address(PyUnicode_FromStringAndSize((char*)pi->Address, pi->AddressLength)); if (!Address) return 0;
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
	r = PyObject_CallFunction(PyAllocHook, (char*)"iiiisi", allocType, size, blockType, requestNumber, fn, lineNumber);
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
bool initwin32(PyObject* blueModule)
{
	loader = CCP_NEW("initwin32/loader") SoftLoader;
	static struct PyModuleDef moduleDef {
		PyModuleDef_HEAD_INIT,
		"blue.win32",
		"",
		-1,
		methods
	};
	PyObject *win32 = PyModule_Create( &moduleDef );
	if ( !win32 ) {
		CCP_LOGERR( "Failed creation blue.win32 module" );
		return false;
	}
	DefineErrors( win32 );
	DefineConsts( win32 );
	if ( PyModule_AddObject( blueModule, "win32", win32 ) ) {
		CCP_LOGERR( "Failed adding win32 submodule to blue" );
		return false;
	}

	return true;
}


#endif

void GetWindowsVersion( OSVERSIONINFOEX &info )
{
	OSVERSIONINFOEX fileInfo;
	bool hasFileInfo = GetWindowsVersionFromFile( fileInfo );
	OSVERSIONINFOEX osInfo;
	GetWindowsVersionFromApi( osInfo );
	if( !hasFileInfo || ( fileInfo.dwMajorVersion == osInfo.dwMajorVersion && fileInfo.dwMinorVersion == osInfo.dwMinorVersion && fileInfo.dwBuildNumber == osInfo.dwBuildNumber ) )
	{
		info = osInfo;
	}
	else
	{
		info = fileInfo;
	}
}

void GetWindowsVersion( OSVERSIONINFO &info )
{
	OSVERSIONINFOEX fileInfo;
	bool hasFileInfo = GetWindowsVersionFromFile( fileInfo );
	OSVERSIONINFOEX osInfo;
	GetWindowsVersionFromApi( osInfo );
	if( !hasFileInfo || ( fileInfo.dwMajorVersion == osInfo.dwMajorVersion && fileInfo.dwMinorVersion == osInfo.dwMinorVersion && fileInfo.dwBuildNumber == osInfo.dwBuildNumber ) )
	{
		memcpy( &info, &osInfo, sizeof( info ) );
	}
	else
	{
		memcpy( &info, &fileInfo, sizeof( info ) );
	}
}
#endif
