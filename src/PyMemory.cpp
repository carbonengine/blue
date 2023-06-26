#include "StdAfx.h"

#if CCP_STACKLESS

#pragma init_seg(user)

//
// Custom memory allocators for Python
//
// This file sets up the memory allocator for Python using a static initializer.
// The #pragma init_seg(user) above ensures that static initializers from this file
// are executed ahead of regular static initializers in this project. This is
// necessary to ensure that the Python allocator is set up before any static
// initializers that may call on the Python C API are executed.

CCP_STATS_DECLARE( pyMemory,					"Blue/Memory/Python", false, CST_MEMORY, "The amount of memory allocated for Python" );

class PyMemoryInitializer;

void* Ccp_PyMalloc( void* ctx, size_t size ) {
	auto _this = reinterpret_cast<PyMemoryInitializer*>(ctx);
	return PyMem_Malloc( size );
}

void* Ccp_PyCalloc( void* ctx, size_t nelem, size_t size ) {
	auto _this = reinterpret_cast<PyMemoryInitializer*>(ctx);
	return PyMem_Calloc( nelem, size );
}

void* Ccp_PyRealloc( void* ctx, void* ptr, size_t newSize ) {
	auto _this = reinterpret_cast<PyMemoryInitializer*>(ctx);
	return PyMem_Realloc( ptr, newSize );
}

void Ccp_PyFree( void* ctx, void* ptr ) {
	auto _this = reinterpret_cast<PyMemoryInitializer*>(ctx);
	PyMem_Free( ptr );
}

void* Ccp_ObjectMalloc( void* ctx, size_t size ) {
	return PyObject_Malloc( size );
}

void* Ccp_ObjectCalloc( void* ctx, size_t nelem, size_t size ) {
	return PyObject_Calloc( nelem, size );
}

void* Ccp_ObjectRealloc( void* ctx, void* ptr, size_t newSize ) {
	return PyObject_Realloc( ptr, newSize );
}

void Ccp_ObjectFree( void* ctx, void* ptr ) {
	return PyObject_Free( ptr );
}

void* Ccp_PyRawMalloc( void* ctx, size_t size ) {
	auto _this = reinterpret_cast<PyMemoryInitializer*>(ctx);
	return PyMem_RawMalloc( size );
}

void* Ccp_PyRawCalloc( void* ctx, size_t nelem, size_t size ) {
	auto _this = reinterpret_cast<PyMemoryInitializer*>(ctx);
	return PyMem_RawCalloc( nelem, size );
}

void* Ccp_PyRawRealloc( void* ctx, void* ptr, size_t newSize ) {
	auto _this = reinterpret_cast<PyMemoryInitializer*>(ctx);
	return PyMem_RawRealloc( ptr, newSize );
}

void Ccp_PyRawFree( void* ctx, void* ptr ) {
	auto _this = reinterpret_cast<PyMemoryInitializer*>(ctx);
	PyMem_RawFree( ptr );
}

class PyMemoryInitializer
{
public:
	PyMemoryInitializer()
	{
          PyMemAllocatorEx memAllocatorEx = {
            /* .ctx = */ this,
            /* .malloc = */ Ccp_PyMalloc,
            /* .calloc = */ Ccp_PyCalloc,
            /* .realloc = */ Ccp_PyRealloc,
            /* .free = */ Ccp_PyFree,
          };

//          PyMem_SetAllocator(PYMEM_DOMAIN_MEM, &memAllocatorEx);

          PyMemAllocatorEx objAllocatorEx = {
              /* .ctx = */ this,
			  /* .malloc = */ Ccp_ObjectMalloc,
			  /* .calloc = */ Ccp_ObjectCalloc,
			  /* .realloc = */ Ccp_ObjectRealloc,
			  /* .free = */ Ccp_ObjectFree,
          };
//          PyMem_SetAllocator(PYMEM_DOMAIN_OBJ, &objAllocatorEx);

          PyMemAllocatorEx rawAllocatorEx = {
              /* .ctx = */ this,
			  /* .malloc = */ Ccp_PyRawMalloc,
			  /* .calloc = */ Ccp_PyRawCalloc,
			  /* .realloc = */ Ccp_PyRawRealloc,
			  /* .free = */ Ccp_PyRawFree,
          };
//          PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &rawAllocatorEx);
	}
};

PyMemoryInitializer initPythonMemoryAllocator;

#endif
