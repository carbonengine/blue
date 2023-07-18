#ifndef BLUE_PYMEMORY_H
#define BLUE_PYMEMORY_H

void InstallPythonMemoryHooks();

// The memory allocators passed on to Python are forward declared for the tests. Do not call them directly,
// or Bad Things may happen.
namespace Ccp {
    void* PyRawMalloc( void* ctx, size_t size );
    void* PyRawCalloc( void* ctx, size_t nelem, size_t size );
    void* PyRawRealloc( void* ctx, void* ptr, size_t newSize );
    void PyRawFree( void* ctx, void* ptr );
    void* PyMallocWithGIL( void* ctx, size_t size );
    void* PyCallocWithGIL( void* ctx, size_t nelem, size_t size );
    void* PyReallocWithGIL( void* ctx, void* ptr, size_t newSize );
    void PyFreeWithGIL( void* ctx, void* ptr );
}

#endif //BLUE_PYMEMORY_H
