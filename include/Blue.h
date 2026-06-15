// Copyright © 2000 CCP ehf.

/* 
	*************************************************************************

	Blue.h

	Project:   Blue

	Description:

		Inclusion of all blue's main header files

	*************************************************************************
*/

#ifndef _BLUE_H_
#define _BLUE_H_

#include <CCPMemory.h>
#include <CCPLog.h>
#include <ICrashReporter.h>

#ifndef __cplusplus
	#error Blue requires C++ compilation (use a .cpp suffix)
#endif

extern "C"
{
	BLUEIMPORT void BlueModuleStartup();
	BLUEIMPORT void BlueInitializeSocketLogger();
	BLUEIMPORT void BlueShutdownSocketLogger();
	BLUEIMPORT void BlueShowError();
	BLUEIMPORT void BlueSetCrashReporter( ICrashReporter* crashReporter );
	BLUEIMPORT void BlueLogFuncChannel( CcpLogChannel_t& logObject, CCP::LogType type, unsigned long userData, const char* format, va_list args );
	BLUEIMPORT void BlueGetInitTab( std::vector<_inittab>& inittab );
	BLUEIMPORT bool BlueConstructPathListFromManifest( std::vector<std::wstring>& pathlist, bool verifyManifest );
	BLUEIMPORT void BlueResolvePathForWritingW( const std::wstring& path, std::wstring& resolved );
	BLUEIMPORT void BlueSetStartupArgs( const std::vector<std::wstring>& args );
	BLUEIMPORT bool BlueHasStartupArg( const std::wstring& name );
	BLUEIMPORT bool BlueRunStackless();
	BLUEIMPORT void BlueTerminate( int );
	BLUEIMPORT bool BlueIsPackaged();
	BLUEIMPORT bool BlueSetSearchPaths( const std::vector<std::wstring>& searchPaths );
}

#if CCP_MEMORY_REPLACE_OPERATOR_NEW
// Regular new and delete operators for use with blue.  Use CCP_NEW instad of new
// to get full debug info in debug builds.

// For Windows overriden new/delete have to be defined per-module so their definitions are
// here. For linux-based systems, new/delete have to be in a single module, so they appear
// in ExeFile.

#if _WIN32

#pragma warning( push )
#pragma warning( disable : 4595 )

inline void* operator new( size_t size )
{
    extern const char* g_moduleName;
    return CCP_MALLOC( g_moduleName, size );
}
inline void* operator new[]( size_t size )
{
    extern const char* g_moduleName;
    return CCP_MALLOC( g_moduleName, size );
}
inline void operator delete( void *p ) noexcept
{
    CCP_FREE( p );
}
inline void operator delete[]( void *p ) noexcept
{
    CCP_FREE( p );
}

#pragma warning( pop )

#endif

#endif

#endif
