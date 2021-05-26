#include "StdAfx.h"
#include "BlueSysInfo.h"
#include "pdm.h"
#ifdef _WIN32
#include "win32.h"
#include <ShlObj.h>
#include <intrin.h>
#endif

BlueSysInfoTaskTimesPtr BlueSysInfo::GetProcessTimes() const
{
	BlueSysInfoTaskTimesPtr times;
	times.CreateInstance();

	int64_t kernelTime, userTime;
	if( CcpGetProcessTimes( kernelTime, userTime ) )
	{
		times->m_systemTime = double( kernelTime ) / 10000000.0;
		times->m_userTime = double( userTime ) / 10000000.0;
	}
	else
	{
		times->m_systemTime = 0;
		times->m_userTime = 0;
	}
	return times;
}

BlueSysInfoTaskTimesPtr BlueSysInfo::GetThreadTimes() const
{
	BlueSysInfoTaskTimesPtr times;
	times.CreateInstance();

	int64_t kernelTime, userTime;
	if( CcpGetThreadTimes( kernelTime, userTime ) )
	{
		times->m_systemTime = double( kernelTime ) / 10000000.0;
		times->m_userTime = double( userTime ) / 10000000.0;
	}
	else
	{
		times->m_systemTime = 0;
		times->m_userTime = 0;
	}
	return times;
}

BlueSysInfoMemoryPtr BlueSysInfo::GetMemory() const
{
	BlueSysInfoMemoryPtr memory;
	memory.CreateInstance();
	return memory;
}

#ifdef _WIN32

std::wstring BlueSysInfo::GetUserDocumentsDirectory() const
{
    wchar_t path[MAX_PATH];
    if( SUCCEEDED( SHGetFolderPathW( NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, path ) ) )
    {
        return path;
    }
    return L"";
}

std::wstring BlueSysInfo::GetSharedApplicationDataDirectory() const
{
    wchar_t path[MAX_PATH];
    if( SUCCEEDED( SHGetFolderPathW( NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, path ) ) )
    {
        return path;
    }
    return L"";
}

std::wstring BlueSysInfo::GetUserApplicationDataDirectory() const
{
    wchar_t path[MAX_PATH];
    if( SUCCEEDED( SHGetFolderPathW( NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, path ) ) )
    {
        return path;
    }
    return L"";
}

std::wstring BlueSysInfo::GetSharedFontsDirectory() const
{
    wchar_t path[MAX_PATH];
	if( SUCCEEDED( SHGetFolderPathW( NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_CURRENT, path ) ) )
    {
        return path;
    }
    return L"";
}


uint32_t BlueSysInfo::GetProcessBitCount() const
{
	return PROCESS_BIT_COUNT;
}

uint32_t BlueSysInfo::GetSystemBitCount() const
{
#ifdef _WIN64
	return 64;
#else
	BOOL isWow = FALSE;
	IsWow64Process( GetCurrentProcess(), &isWow );
	return isWow ? 64 : 32;
#endif
}

uint64_t BlueSysInfo::GetProcessStartTime() const
{
	FILETIME creationTime, exitTime, kernelTime, userTime;
	if( ::GetProcessTimes( GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime ) )
	{
		ULARGE_INTEGER lg;
		lg.HighPart = creationTime.dwHighDateTime;
		lg.LowPart = creationTime.dwLowDateTime;
		return lg.QuadPart;
	}
	else
	{
		return 0;
	}
}

bool BlueSysInfo::IsWine() const
{
	return PDM::IsWine();
}

std::wstring BlueSysInfo::GetWineVersion() const
{
	return PDM::UTF8ToWString( PDM::GetWineVersion() );
}

std::wstring BlueSysInfo::GetWineHostOs() const
{
	return PDM::UTF8ToWString( PDM::GetWineHostOs() );
}

std::vector<BlueSysInfoNetworkAdapterPtr> BlueSysInfo::GetNetworkAdapters() const
{
	std::vector<BlueSysInfoNetworkAdapterPtr> adapters;

	for( auto& adapter : PDM::GetNetworkAdapterInfo() )
	{
		BlueSysInfoNetworkAdapterPtr ptr;
		ptr.CreateInstance();
		ptr->m_name = PDM::UTF8ToWString( adapter.name );
		ptr->m_macAddress = std::string( adapter.macAddress.begin(), adapter.macAddress.end() );
		ptr->m_macAddressString = adapter.macAddressString;
		ptr->m_uuid = adapter.uuidString;

		adapters.push_back( ptr );
	}
	
	return adapters;
}

std::string BlueSysInfo::GetMachineUuid() const
{
	REGSAM access = KEY_READ;
#ifndef _WIN64
	if( GetSystemBitCount() != 32 )
	{
		access |= KEY_WOW64_64KEY;
	}
#endif
	HKEY key;
	if( RegOpenKeyEx( HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, access, &key ) != ERROR_SUCCESS )
	{
		return "";
	}
	ON_BLOCK_EXIT( [&] { RegCloseKey( key ); } );

	DWORD type;
	char guid[256];
	DWORD size = DWORD( sizeof( guid ) );
	if( RegQueryValueEx( key, "MachineGuid", nullptr, &type, reinterpret_cast<LPBYTE>( guid ), &size ) != ERROR_SUCCESS )
	{
		return "";
	}
	if( type != REG_SZ )
	{
		return "";
	}
	return guid;
}



BlueSysInfoCpu::BlueSysInfoCpu()
{
	m_extensions = PDM::GetCPUInfo().extensions;

	SYSTEM_INFO info;
	GetSystemInfo( &info );

	m_family = info.wProcessorLevel;
	m_revision = info.wProcessorRevision;
	m_logicalCpuCount = info.dwNumberOfProcessors;

	int cpuInfo[4] = { 0 };
	__cpuid( cpuInfo, 0x80000000 );
	int idMax = cpuInfo[0];
	if ( idMax >= 0x80000001 )
	{
		__cpuidex( cpuInfo, 0x80000001, 0 );
		m_bitCount = cpuInfo[3] & ( 1 << 29 ) ? 64 : 32;
	}
	else
	{
		m_bitCount = 32;
	}

	char brand[0x40] = { 0 };

	if ( idMax >= 0x80000004 )
	{
		__cpuidex( reinterpret_cast<int*>( brand ), 0x80000002, 0 );
		__cpuidex( reinterpret_cast<int*>( brand + 16 ), 0x80000003, 0 );
		__cpuidex( reinterpret_cast<int*>( brand + 32 ), 0x80000004, 0 );
	}
	m_brand = brand;

	char vendor[0x20] = { 0 };
	__cpuidex( cpuInfo, 0, 0 );
	*reinterpret_cast<int*>(vendor) = cpuInfo[1];
    *reinterpret_cast<int*>(vendor + 4) = cpuInfo[3];
    *reinterpret_cast<int*>(vendor + 8) = cpuInfo[2];

	int model = 0;
	int stepping = 0;

	__cpuid( cpuInfo, 0 );
	idMax = cpuInfo[0];
	if( idMax > 0 )
	{
		__cpuidex( cpuInfo, 1, 0 );
		model = ( cpuInfo[0] >> 4 ) & 0xf;
		stepping = cpuInfo[0] & 0xf;
	}


	const char* platform;
	switch( info.wProcessorArchitecture )
	{
	case PROCESSOR_ARCHITECTURE_AMD64:
		platform = strstr( vendor, "Intel" ) ? "Intel64" : "AMD64";
		break;
	case PROCESSOR_ARCHITECTURE_ARM:
		platform = "ARM";
		break;
	case PROCESSOR_ARCHITECTURE_IA64:
		platform = "IA64";
		break;
	case PROCESSOR_ARCHITECTURE_INTEL:
		platform = "x86";
		break;
	default:
		platform = "Unknown";
	}
	char identifier[128];
	sprintf_s( identifier, "%s Family %i Model %i Stepping %i, %s", platform, m_family, model, stepping, vendor );
	m_identifier = identifier;
}


BlueSysInfoOs::BlueSysInfoOs()
{
	m_platform = WINDOWS;

	OSVERSIONINFOEX info = { 0 };
	info.dwOSVersionInfoSize = sizeof( info );
	GetWindowsVersion( info );

	m_majorVersion = info.dwMajorVersion;
	m_minorVersion = info.dwMinorVersion;
	m_buildNumber = info.dwBuildNumber;
	m_patch = info.szCSDVersion;
	if( info.wProductType > 1 )
	{
		m_suite = SERVER;
	}
	else if( ( info.wSuiteMask & 0x00000200 ) != 0 )
	{
		m_suite = DESKTOP;
	}
	else
	{
		m_suite = WORKSTATION;
	}
}

BlueSysInfoMemory::BlueSysInfoMemory()
{
    size_t workingSetMemory, pageFileMemory;
    CcpGetProcessMemoryInfo( workingSetMemory, pageFileMemory );
	m_workingSet = uint64_t( workingSetMemory );
	m_pageFile = uint64_t( pageFileMemory );

	MEMORYSTATUSEX status;
	status.dwLength = DWORD( sizeof( status ) );
	GlobalMemoryStatusEx( &status );
	m_totalPhysical = uint64_t( status.ullTotalPhys );
	m_availablePhysical = uint64_t( status.ullAvailPhys );
}

BlueSysInfoNetworkAdapter::BlueSysInfoNetworkAdapter() {}

#endif
