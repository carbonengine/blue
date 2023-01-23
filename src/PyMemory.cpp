#include "StdAfx.h"

#if CCP_STACKLESS

#pragma init_seg(user)

//
// This file sets up the memory allocator for Python using a static initializer.
// The #pragma init_seg(user) above ensures that static initializers from this file
// are executed ahead of regular static initializers in this project. This is
// necessary to ensure that the Python allocator is set up before any static
// initializers that may call on the Python C API are exectured.

//
// Custom memory allocators for Python
//
void* PyCCP_Malloc( size_t size, void *arg, const char *file, int line, const char *msg )
{
	return CCPMallocWithTracking( size, msg ? msg : "Python", file, line );
}

void* PyCCP_Realloc( void *ptr, size_t size, void *arg, const char *file, int line, const char *msg )
{
	return CCPReallocWithTracking( ptr, size, msg ? msg : "Python", file, line );
}

void PyCCP_Free( void *ptr, void *arg, const char *file, int line, const char *msg )
{
	CCPFreeWithTracking( ptr );
}

size_t PyCCP_Msize( void* ptr, void* arg )
{
	return CCPMSizeWithTracking( ptr );
}

PyCCP_CustomAllocator_t s_pyAllocator = { PyCCP_Malloc, PyCCP_Realloc, PyCCP_Free, PyCCP_Msize, NULL };

class PyMemoryInitializer
{
public:
	PyMemoryInitializer()
	{
		PyCCP_SetAllocator( 0, &s_pyAllocator );
		PyCCP_SetAllocator( 1, &s_pyAllocator );
	}
};

PyMemoryInitializer initPythonMemoryAllocator;

#endif
