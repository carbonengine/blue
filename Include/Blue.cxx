/* 
	*************************************************************************

	Blue.cxx

	Author:    Matthias Gudmundsson
	Created:   Nov. 2000
	OS:        Win32
	Project:   Blue

	Description:   

		Inclusion of all blue's .cxx files


	Dependencies:

		Blue

	(c) CCP 2000

	*************************************************************************
*/

#ifndef _BLUE_CXX_
#define _BLUE_CXX_

#include "BlueId.cxx"
#include "CcpCore/include/CCPMemory.h"

#if CCP_MEMORY_REPLACE_OPERATOR_NEW

	// Regular new and delete operators for use with blue.  Use CCP_NEW instad of new
	// to get full debug info in debug builds.

#ifdef _MSC_VER
	void* operator new( size_t size )
	{
		extern const char* g_moduleName;
		return CCP_MALLOC( g_moduleName, size );
	}
	void* operator new[]( size_t size )
	{
		extern const char* g_moduleName;
		return CCP_MALLOC( g_moduleName, size );
	}
	void operator delete( void *p )
	{
		CCP_FREE( p );
	}
	void operator delete[]( void *p )
	{
		CCP_FREE( p );
	}
#else
	void* operator new( size_t size ) throw (std::bad_alloc)
	{
		extern const char* g_moduleName;
		return CCP_MALLOC( g_moduleName, size );
	}
	void* operator new[]( size_t size ) throw (std::bad_alloc)
	{
		extern const char* g_moduleName;
		return CCP_MALLOC( g_moduleName, size );
	}
	void operator delete( void *p ) throw()
	{
		CCP_FREE( p );
	}
	void operator delete[]( void *p ) throw()
	{
		CCP_FREE( p );
	}
#endif

#endif

#endif

