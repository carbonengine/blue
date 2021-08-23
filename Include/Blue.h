/* 
	*************************************************************************

	Blue.h

	Author:    Matthias Gudmundsson
	Created:   Oct. 2000
	OS:        Win32
	Project:   Blue

	Description:   

		Inclusion of all blue's main header files


	Dependencies:

		Blue

	(c) CCP 2000

	*************************************************************************
*/

#ifndef _BLUE_H_
#define _BLUE_H_

#include "CcpCore/include/CCPMemory.h"
#include "CcpCore/include/CCPLog.h"
#include "CcpCore/include/ICrashReporter.h"

#ifndef __cplusplus
	#error Blue requires C++ compilation (use a .cpp suffix moron)
#endif

extern "C"
{
	BLUEIMPORT void BlueModuleStartup();
	BLUEIMPORT void BlueInitializeSocketLogger();
	BLUEIMPORT void BlueSetCrashReporter( ICrashReporter* crashReporter );
	BLUEIMPORT void BlueLogFuncChannel( CcpLogChannel_t& logObject, CCP::LogType type, unsigned long userData, const char* format, va_list args );
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

