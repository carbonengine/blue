// Copyright © 2015 CCP ehf.

#include "StdAfx.h"
#include "BlueSysInfo.h"
#include "pdm.h"

#include <sstream>
#include <fstream>
#include <filesystem>

namespace
{
const int64_t s_startTime = TimeNow();
}

std::wstring BlueSysInfo::GetUserDocumentsDirectory() const
{
	const char* xdgDataHome = std::getenv( "XDG_DOCUMENTS_DIR" );


	std::string path;

	if( xdgDataHome )
	{
		path = xdgDataHome;
	}
	else
	{
		const char* home = std::getenv( "HOME" );

		if( !home )
		{
			return L"";
		}
		path = std::string( home ) + "/Documents";
	}

	if( std::filesystem::exists( path ) )
	{
		return std::wstring( path.begin(), path.end() );
	}
	else
	{
		// Fallback to home if not present
		// Tests expect a valid path
		// LINUX-TODO though this may not be a good thing
		const char* home = std::getenv( "HOME" );

		if( !home )
		{
			return L"";
		}
		std::string homeString = std::string( home );

		return std::wstring( homeString.begin(), homeString.end() );
	}
}

std::wstring BlueSysInfo::GetSharedApplicationDataDirectory() const
{
	const char* xdgDataHome = std::getenv( "XDG_DATA_HOME" );

	std::string sharedApplicationDataPath;

	if( xdgDataHome )
	{
		sharedApplicationDataPath = xdgDataHome;
	}
	else
	{
		// Fallback to ~/.local/share if XDG not found
		const char* home = std::getenv( "HOME" );
		if( !home )
		{
			return L"";
		}
		sharedApplicationDataPath = std::string( home ) + "/.local/share";
	}
	return std::wstring( sharedApplicationDataPath.begin(), sharedApplicationDataPath.end() );
}

std::wstring BlueSysInfo::GetUserApplicationDataDirectory() const
{
	return GetSharedApplicationDataDirectory();
}

std::wstring BlueSysInfo::GetSharedFontsDirectory() const
{
	return L"/usr/share/fonts";
}

std::wstring BlueSysInfo::GetSystemFontsDirectory() const
{
	return GetSharedFontsDirectory();
}

uint32_t BlueSysInfo::GetProcessBitCount() const
{
	return CcpGetProcessBitCount();
}

uint32_t BlueSysInfo::GetSystemBitCount() const
{
	return 64;
}

uint64_t BlueSysInfo::GetProcessStartTime() const
{
	return s_startTime;
}

std::string BlueSysInfo::GetMachineUuid() const
{
	std::ifstream machineId( "/etc/machine-id" );

	if( !machineId.is_open() )
	{
		return "";
	}

	std::string machineUuid = "";

	std::getline( machineId, machineUuid );

	machineId.close();

	return machineUuid;
}

std::wstring BlueSysInfo::GetMachineName() const
{
	//LINUX-TODO + Test Coverage
	return L"";
}

std::wstring BlueSysInfo::GetDomainName() const
{
	//LINUX-TODO + Test Coverage
	return L"";
}

BlueSysInfoCpu::BlueSysInfoCpu() :
	m_extensions( PDM::GetCPUInfo().extensions )
{
	auto pdmCpu = PDM::GetCPUInfo();
	m_mHz = pdmCpu.frequency;
	m_brand = pdmCpu.brand;
	m_logicalCpuCount = pdmCpu.logicalCoreCount;
	m_bitCount = static_cast<uint32_t>( pdmCpu.bitness );
	m_family = 0; //LINUX-TODO missing from pdm

	std::string vendor = pdmCpu.vendor;
	int model = pdmCpu.model;
	int stepping = pdmCpu.stepping;

	switch( pdmCpu.architecture )
	{
	case PDM::CPUArchitecture::UNKNOWN: {
		m_architecture = "UNKNOWN";
		break;
	}

	case PDM::CPUArchitecture::X86: {
		m_architecture = "x86";
		break;
	}

	case PDM::CPUArchitecture::X86_64: {
		m_architecture = "Intel64";
		break;
	}

	case PDM::CPUArchitecture::ARM: {
		m_architecture = "ARM";
		break;
	}

	case PDM::CPUArchitecture::ARM64: {
		m_architecture = "ARM64";
		break;
	}

	default:
		m_architecture = "UNKNOWN";
	}

	std::stringstream identifierStream;

	identifierStream
		<< m_architecture
		<< " Family" << m_family
		<< " Model" << model
		<< " Stepping" << stepping
		<< " , " << vendor;

	m_identifier = identifierStream.str();
}


BlueSysInfoOs::BlueSysInfoOs()
{
	m_platform = LINUX;
	m_suite = DESKTOP;

	std::ifstream osReleaseInfoFile( "/etc/os-release" );

	if( !osReleaseInfoFile.is_open() )
	{
		return;
	}

	std::string lineEntry;
	while( std::getline( osReleaseInfoFile, lineEntry ) )
	{
		auto position = lineEntry.find( "=" );
		auto key = lineEntry.substr( 0, position );
		auto value = lineEntry.substr( position + 1 );

		if( key == "VERSION" )
		{
			auto semanticVersionSize = value.find( " " );
			std::string semanticVersionString = value.substr( 1, semanticVersionSize - 1 ); // Offset by 1 to ignore leading "

			std::stringstream ss;

			ss << semanticVersionString;

			std::vector<std::string> version;

			std::string token;
			while( std::getline( ss, token, '.' ) )
			{
				version.push_back( token );
			}

			if( version.empty() )
			{
				// Failed to retrieve version info from file
				osReleaseInfoFile.close();

				return;
			}

			m_majorVersion = atoi( version.at( 0 ).c_str() );
			m_minorVersion = version.size() > 1 ? atoi( version.at( 1 ).c_str() ) : 0;
			m_buildNumber = version.size() > 2 ? atoi( version.at( 2 ).c_str() ) : 0;

			osReleaseInfoFile.close();

			return;
		}
	}

	osReleaseInfoFile.close();
}

BlueSysInfoMemory::BlueSysInfoMemory()
{
	CcpProcessMemoryInfo info;
	if( CcpGetProcessMemoryInfo( info ) )
	{
		m_workingSet = uint64_t( info.workingSetSize );
		m_pageFile = uint64_t( info.pageFileUsage );
	}

	m_totalPhysical = 0;
	m_availablePhysical = 0;

	std::ifstream memInfoFile( "/proc/meminfo" );

	if( !memInfoFile.is_open() )
	{
		return;
	}

	std::string lineEntry;
	while( std::getline( memInfoFile, lineEntry ) )
	{
		std::stringstream ss;
		ss << lineEntry;

		std::string entryId;
		ss >> entryId;

		if( entryId == "MemTotal:" )
		{
			uint64_t totalPhysicalKb;
			ss >> totalPhysicalKb;
			m_totalPhysical = totalPhysicalKb * 1024;
		}
		else if( entryId == "MemAvailable:" )
		{
			uint64_t availablePhysicalKb;
			ss >> availablePhysicalKb;
			m_availablePhysical = availablePhysicalKb * 1024;
		}
	}

	memInfoFile.close();
}
