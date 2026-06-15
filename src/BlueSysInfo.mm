// Copyright © 2015 CCP ehf.

#include "StdAfx.h"

#if __APPLE__

#include <string>
#include "BlueSysInfo.h"
#include <BlueExposure.h>
#include "pdm.h"
#include <sys/sysctl.h>
#include <sys/times.h>
#include <mach/mach_time.h>
#include <mach/clock.h>
#include <mach/machine.h>
#import <Foundation/Foundation.h>


namespace
{
    const int64_t s_startTime = TimeNow();
}


std::wstring BlueSysInfo::GetUserDocumentsDirectory() const
{
    NSFileManager* fileManager = [NSFileManager defaultManager];
    NSURL* url = [fileManager URLForDirectory:NSDocumentDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:false error:nil];
    if( url )
    {
        return std::wstring( CA2W( [[url path] UTF8String] ) );
    }
    return L"";
}

std::wstring BlueSysInfo::GetSharedApplicationDataDirectory() const
{
    NSFileManager* fileManager = [NSFileManager defaultManager];
    // We can't use /Library/Application Support directory as it requires elevated privileges to write to it
    // so we fall back to ~/Library/Application Support
    NSURL* url = [fileManager URLForDirectory:NSApplicationSupportDirectory inDomain:NSLocalDomainMask appropriateForURL:nil create:false error:nil];
    if( url )
    {
        return std::wstring( CA2W( [[url path] UTF8String] ) );
    }
    return L"";
}

std::wstring BlueSysInfo::GetUserApplicationDataDirectory() const
{
    NSFileManager* fileManager = [NSFileManager defaultManager];
    NSURL* url = [fileManager URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:false error:nil];
    if( url )
    {
        return std::wstring( CA2W( [[url path] UTF8String] ) );
    }
    return L"";
}

std::wstring BlueSysInfo::GetSharedFontsDirectory() const
{
    NSFileManager* fileManager = [NSFileManager defaultManager];
    NSURL* url = [fileManager URLForDirectory:NSLibraryDirectory inDomain:NSLocalDomainMask appropriateForURL:nil create:false error:nil];
    if( url )
    {
        return std::wstring( CA2W( [[url path] UTF8String] ) ).append( L"/Fonts" );
    }
    return L"";
}

std::wstring BlueSysInfo::GetSystemFontsDirectory() const
{
    NSFileManager* fileManager = [NSFileManager defaultManager];
    NSURL* url = [fileManager URLForDirectory:NSLibraryDirectory inDomain:NSSystemDomainMask appropriateForURL:nil create:false error:nil];
    if( url )
    {
        return std::wstring( CA2W( [[url path] UTF8String] ) ).append( L"/Fonts" );
    }
    return L"";
}

uint32_t BlueSysInfo::GetProcessBitCount() const
{
#if __LP64__
    return 64;
#else
    return 32;
#endif
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
    char buffer[128];
    io_registry_entry_t ioRegistryRoot = IORegistryEntryFromPath(kIOMasterPortDefault, "IOService:/");
    CFStringRef uuidCf = (CFStringRef) IORegistryEntryCreateCFProperty(ioRegistryRoot, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);
    IOObjectRelease(ioRegistryRoot);
    CFStringGetCString(uuidCf, buffer, sizeof( buffer ), kCFStringEncodingMacRoman);
    CFRelease(uuidCf);
    return buffer;
}

std::wstring BlueSysInfo::GetMachineName() const
{
    char buffer[256];
    buffer[0] = 0;
    gethostname( buffer, 256 );
    if( auto dot = strchr( buffer, '.' ) )
    {
        *dot = 0;
    }
    std::string str( buffer );
    return std::wstring( std::begin( str ), std::end( str ) );
}

std::wstring BlueSysInfo::GetDomainName() const
{
    char buffer[256];
    buffer[0] = 0;
    gethostname( buffer, 256 );
    char* start = buffer;
    if( auto dot = strchr( buffer, '.' ) )
    {
        start = dot + 1;
        dot = strchr( start, '.' );
        if( dot )
        {
            *dot = 0;
        }
    }

    std::string str( start );
    return std::wstring( std::begin( str ), std::end( str ));
}


BlueSysInfoCpu::BlueSysInfoCpu() :
	m_extensions( PDM::GetCPUInfo().extensions )
{
	auto pdmCpu = PDM::GetCPUInfo();
	m_mHz = pdmCpu.frequency;
    char buffer[512] = { 0 };
    size_t size = sizeof( buffer );
    sysctlbyname( "machdep.cpu.brand_string", buffer, &size, nullptr, 0 );
    m_brand = buffer;

    int family = 0;
    size = sizeof( family );
    sysctlbyname( "machdep.cpu.family", &family, &size, nullptr, 0 );
    m_family = family;

    int model = 0;
    int stepping = 0;
    size = sizeof( model );
    sysctlbyname( "machdep.cpu.model", &model, &size, nullptr, 0 );
    sysctlbyname( "machdep.cpu.stepping", &stepping, &size, nullptr, 0 );
    m_revision = ( model << 8 ) | stepping;

    int count = 0;
    size = sizeof( count );
    sysctlbyname( "hw.logicalcpu", &count, &size, nullptr, 0 );
    m_logicalCpuCount = count;

    int arch = 0;
    size = sizeof( arch );
    sysctlbyname( "hw.cpu64bit_capable", &arch, &size, nullptr, 0 );
    m_bitCount = arch ? 64 : 32;

    size = sizeof( buffer );
    sysctlbyname( "machdep.cpu.vendor", buffer, &size, nullptr, 0 );

    const char* platform;
#if __i386__ || __x86_64__
    if( m_bitCount == 32 )
    {
        m_architecture = "x86";
        platform = "x86";
    }
    else
    {
        m_architecture = "AMD64";
        if( strstr( buffer, "Intel" ) )
        {
            platform = "Intel64";
        }
        else
        {
            platform = "AMD64";
        }
    }
#elif __aarch64__
    m_architecture = "ARM64";
    platform = "Apple";
#else
#error "Unsupported architecture"
#endif

    sprintf_s( buffer, "%s Family %i Model %i Stepping %i, %s", platform, family, model, stepping, buffer );
    m_identifier = buffer;
}


BlueSysInfoOs::BlueSysInfoOs()
{
    m_platform = OSX;


    NSProcessInfo* processInfo = [NSProcessInfo processInfo];

    NSOperatingSystemVersion osVersion = [processInfo operatingSystemVersion];
    m_majorVersion = int32_t( osVersion.majorVersion );
    m_minorVersion = int32_t( osVersion.minorVersion );
    m_buildNumber  = int32_t( osVersion.patchVersion );
    m_suite = DESKTOP;
}


BlueSysInfoMemory::BlueSysInfoMemory()
{
    CcpProcessMemoryInfo memInfo;
    CcpGetProcessMemoryInfo( memInfo);
    m_workingSet = uint64_t( memInfo.workingSetSize );
    m_pageFile = uint64_t( memInfo.pageFileUsage );

    int64_t memsize = 0;
    size_t size = sizeof( memsize );
    sysctlbyname( "hw.memsize", &memsize, &size, nullptr, 0 );
    m_totalPhysical = uint64_t( memsize );
    // could not find a way to get it:
    m_availablePhysical = m_totalPhysical / 2;
}

#endif
