// Copyright © 2015 CCP ehf.

#include "StdAfx.h"

#include "BlueSysInfo.h"

static CBlueSysInfo s_blueSysInfo;
BlueSysInfo* blueSysInfo = &s_blueSysInfo;

BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "sysinfo", blueSysInfo );

const BlueSysInfo& BlueSysInfo::GetSysInfo()
{
	return s_blueSysInfo;
}


BLUE_DEFINE_ABSTRACT( BlueSysInfo );

const Be::ClassInfo* BlueSysInfo::ExposeToBlue()
{
    EXPOSURE_BEGIN( BlueSysInfo, "" )
		MAP_INTERFACE( BlueSysInfo )

		MAP_PROPERTY_READONLY( "processBitCount", GetProcessBitCount, "Bit count for current process" );
		MAP_PROPERTY_READONLY( "systemBitCount", GetSystemBitCount, "Bit count for OS" );
		MAP_ATTRIBUTE( "cpu", m_cpu, "CPU information", Be::READ );
		MAP_ATTRIBUTE( "os", m_os, "OS information", Be::READ );
		MAP_PROPERTY_READONLY( "isRosetta", IsRosetta, "Is the process running under Rosetta" );
		MAP_PROPERTY_READONLY( "isWine", IsWine, "Is the process running under Wine" );
		MAP_PROPERTY_READONLY( "wineVersion", GetWineVersion, "Version number of Wine" );
		MAP_PROPERTY_READONLY( "wineHostOs", GetWineHostOs, "Description of the host OS when running under Wine" );

		MAP_PROPERTY_READONLY( "machineUuid", GetMachineUuid, "UUID of the machine as a string" );
		MAP_PROPERTY_READONLY( "computerName", GetMachineName, "Host machine name" );
		MAP_PROPERTY_READONLY( "domainName", GetDomainName, "Host machine domain name" );

		MAP_METHOD_AND_WRAP( "GetNetworkAdapters", GetNetworkAdapters, "Get a list of all network adapters on this machine" );
		MAP_METHOD_AND_WRAP( "GetMemory", GetMemory, "Returns BlueSysInfoMemory object with memory status report" );
		MAP_METHOD_AND_WRAP(
			"GetUserDocumentsDirectory",
			GetUserDocumentsDirectory,
			"Documents directory path for the current user" );
		MAP_METHOD_AND_WRAP(
			"GetSharedApplicationDataDirectory",
			GetSharedApplicationDataDirectory,
			"Application data directory shared by all users on this machine" );
		MAP_METHOD_AND_WRAP(
			"GetUserApplicationDataDirectory",
			GetUserApplicationDataDirectory,
			"Application data directory for the current user" );
		MAP_METHOD_AND_WRAP(
			"GetSharedFontsDirectory",
			GetSharedFontsDirectory,
			"Font directory shared by all users on this machine" );
		MAP_METHOD_AND_WRAP(
			"GetSystemFontsDirectory",
			GetSystemFontsDirectory,
			"Font directory for system-wide installed fonts on this machine" );
		MAP_METHOD_AND_WRAP(
			"GetProcessTimes",
			GetProcessTimes,
			"Returns BlueSysInfoTaskTimes object with timing information on the current process" )
		MAP_METHOD_AND_WRAP(
			"GetThreadTimes",
			GetThreadTimes,
			"Returns BlueSysInfoTaskTimes object with timing information on the current thread" )
		MAP_PROPERTY_READONLY(
			"processStartTime",
			GetProcessStartTime,
			"Process start time as windows FILETIME (number of 100-nanosecond intervals since January 1, 1601).\n"
			"On OSX it is an approximate value" );
		MAP_METHOD_AND_WRAP(
			"GetPDMData",
			GetPDMData,
			"Returns a unicode string containing all PDM data" )
    EXPOSURE_END()
}


BLUE_DEFINE_ABSTRACT( BlueSysInfoCpu );

const Be::ClassInfo* BlueSysInfoCpu::ExposeToBlue()
{
    EXPOSURE_BEGIN( BlueSysInfoCpu, "" )
		MAP_INTERFACE( BlueSysInfoCpu )

		MAP_ATTRIBUTE( "family", m_family, "CPU family as an integer; pretty useless", Be::READ );
		MAP_ATTRIBUTE( "revision", m_revision, "CPU revision as an integer; pretty useless", Be::READ );
		MAP_ATTRIBUTE( "logicalCpuCount", m_logicalCpuCount, "Number of logical CPUs (cores)", Be::READ );
		MAP_ATTRIBUTE( "bitCount", m_bitCount, "64 or 32", Be::READ );
		MAP_ATTRIBUTE( "identifier", m_identifier, "CPU identifier string", Be::READ );
		MAP_ATTRIBUTE( "brand", m_brand, "CPU brand name", Be::READ );
		MAP_ATTRIBUTE( "architecture", m_architecture, "CPU architecture label (x64, AMD64, etc.)", Be::READ );
		MAP_ATTRIBUTE( "frequency", m_mHz, "CPU frequency in MHz", Be::READ );
		MAP_METHOD_AND_WRAP( "GetExtensions", GetExtensions, "Get CPU extensions" );
	EXPOSURE_END()
}

const Be::VarChooser BlueSysInfoOs_Platform_Chooser[] =
{
	{ "WINDOWS", BeCast( BlueSysInfoOs::WINDOWS ), "Windows" },
	{ "OSX", BeCast( BlueSysInfoOs::OSX ), "OSX" },
	{ 0 }
};

BLUE_REGISTER_ENUM_EX(
    "OsPlatform",
	BlueSysInfoOs::Platform,
    BlueSysInfoOs_Platform_Chooser,
    ENUM_REG_ENUM_OBJECT_ON_MODULE
);

const Be::VarChooser BlueSysInfoOs_Suite_Chooser[] =
{
	{ "DESKTOP", BeCast( BlueSysInfoOs::DESKTOP ), "Windows Home editions" },
	{ "WORKSTATION", BeCast( BlueSysInfoOs::WINDOWS ), "Windows Pro editions" },
	{ "SERVER", BeCast( BlueSysInfoOs::WINDOWS ), "Windows server editions" },
	{ 0 }
};

BLUE_REGISTER_ENUM_EX(
    "OsSuite",
	BlueSysInfoOs::Suite,
    BlueSysInfoOs_Suite_Chooser,
    ENUM_REG_ENUM_OBJECT_ON_MODULE
);


BLUE_DEFINE_ABSTRACT( BlueSysInfoOs );

const Be::ClassInfo* BlueSysInfoOs::ExposeToBlue()
{
    EXPOSURE_BEGIN( BlueSysInfoOs, "" )
		MAP_INTERFACE( BlueSysInfoOs )

		MAP_ATTRIBUTE_WITH_CHOOSER( "platform", m_platform, "Generic OS type as a member of blue.OsPlatform", Be::READ | Be::ENUM, BlueSysInfoOs_Platform_Chooser );
		MAP_ATTRIBUTE( "majorVersion", m_majorVersion, "OS major version", Be::READ );
		MAP_ATTRIBUTE( "minorVersion", m_minorVersion, "OS minor version", Be::READ );
		MAP_ATTRIBUTE( "buildNumber", m_buildNumber, "OS build number", Be::READ );
		MAP_ATTRIBUTE( "patch", m_patch, "OS patch name", Be::READ );
		MAP_ATTRIBUTE( "suite", m_suite, "OS suite as a member of blue.OsSuite", Be::READ );
    EXPOSURE_END()
}


BLUE_DEFINE( BlueSysInfoTaskTimes );

const Be::ClassInfo* BlueSysInfoTaskTimes::ExposeToBlue()
{
    EXPOSURE_BEGIN( BlueSysInfoTaskTimes, "" )
		MAP_INTERFACE( BlueSysInfoTaskTimes )

		MAP_ATTRIBUTE( "userTime", m_userTime, "Time spent in user mode in seconds", Be::READ );
		MAP_ATTRIBUTE( "systemTime", m_systemTime, "Time spent in system (kernel) mode in seconds", Be::READ );
    EXPOSURE_END()
}


BLUE_DEFINE( BlueSysInfoMemory );

const Be::ClassInfo* BlueSysInfoMemory::ExposeToBlue()
{
    EXPOSURE_BEGIN( BlueSysInfoMemory, "" )
		MAP_INTERFACE( BlueSysInfoMemory )

		MAP_ATTRIBUTE( "workingSet", m_workingSet, "Process working set size in bytes", Be::READ );
		MAP_ATTRIBUTE( "pageFile", m_pageFile, "Process page file usage in bytes", Be::READ );
		MAP_ATTRIBUTE( "totalPhysical", m_totalPhysical, "Total physical memory machine has in bytes", Be::READ );
		MAP_ATTRIBUTE( "availablePhysical", m_availablePhysical, "Available physical memory in bytes", Be::READ );
    EXPOSURE_END()
}


BLUE_DEFINE( BlueSysInfoNetworkAdapter );

const Be::ClassInfo* BlueSysInfoNetworkAdapter::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueSysInfoNetworkAdapter, "" )
		MAP_INTERFACE( BlueSysInfoNetworkAdapter )

		MAP_ATTRIBUTE( "name", m_name, "Name given to the adapter", Be::READ );
		MAP_ATTRIBUTE( "macAddress", m_macAddress, "Adapter MAC address in binary format", Be::READ );
		MAP_ATTRIBUTE( "macAddressString", m_macAddressString, "Adapter MAC address in human readable format", Be::READ );
		MAP_ATTRIBUTE( "uuid", m_uuid, "Adapter UUID, which can be empty", Be::READ );
	EXPOSURE_END()
}

MAP_FUNCTION_AND_WRAP(
	"GetPDMByteData",
	GetPDMByteData,
	"Returns a stream for reading pdm byte data"
	":raises IOError: on failure to serialise pdm data"
);
