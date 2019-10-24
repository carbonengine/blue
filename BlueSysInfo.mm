#include "StdAfx.h"
#include <string>
#include "BlueSysInfo.h"
#include "BlueExposure/include/BlueExposure.h"
#include <sys/sysctl.h>
#include <sys/times.h>
#include <mach/mach_time.h>
#include <mach/clock.h>
#include <mach/machine.h>
#import <Foundation/Foundation.h>


namespace
{
    struct BlueImportTime
    {
        BlueImportTime()
        {
            time_t t;
            time( &t );
            
            m_time = uint64_t( t ) * 10000000 + 116444736000000000;
        }
        
        uint64_t m_time;
    };
    
    const BlueImportTime s_startTime;
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
        return std::wstring( CA2W( [[url path] UTF8String] ) );
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
    return s_startTime.m_time;
}

bool BlueSysInfo::IsWine() const
{
    return false;
}

std::wstring BlueSysInfo::GetWineVersion() const
{
    return L"";
}

std::wstring BlueSysInfo::GetWineHostOs() const
{
    return L"";
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


BlueSysInfoCpu::BlueSysInfoCpu()
{
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
    if( m_bitCount == 32 )
    {
        platform = "x86";
    }
    else
    {
        if( strstr( buffer, "Intel" ) )
        {
            platform = "Intel64";
        }
        else
        {
            platform = "AMD64";
        }
    }
    
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
    size_t workingSetMemory, pageFileMemory;
    CcpGetProcessMemoryInfo( workingSetMemory, pageFileMemory );
    m_workingSet = uint64_t( workingSetMemory );
    m_pageFile = uint64_t( pageFileMemory );
    
    int64_t memsize = 0;
    size_t size = sizeof( memsize );
    sysctlbyname( "hw.memsize", &memsize, &size, nullptr, 0 );
    m_totalPhysical = uint64_t( memsize );
    // could not find a way to get it:
    m_availablePhysical = m_totalPhysical / 2;
}
