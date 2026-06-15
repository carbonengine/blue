// Copyright © 2023 CCP ehf.

#ifndef BLUE_PYMEMORY_H
#define BLUE_PYMEMORY_H

extern "C" BLUEIMPORT void BlueInstallPythonMemoryHooks();

// The memory allocators passed on to Python are forward declared for the tests. Do not call them directly,
// or Bad Things may happen.
namespace Ccp {
    void* MeasuredMalloc( void* ctx, size_t size );
    void* MeasuredCalloc( void* ctx, size_t nelem, size_t size );
    void* MeasuredRealloc( void* ctx, void* ptr, size_t newSize );
    void MeasuredFree( void* ctx, void* ptr );
}

#endif //BLUE_PYMEMORY_H
