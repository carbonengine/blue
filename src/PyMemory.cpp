#include "StdAfx.h"

#if CCP_STACKLESS

//
// Custom memory allocators for Python
//
// These are primarily used to track statistics about memory allocated by Python.
//

CCP_STATS_DECLARE(pyMemory, "Blue/Memory/Python", false, CST_MEMORY, "The amount of memory allocated for Python");

class RawAllocator {
public:
    RawAllocator() : mStats{g_ccpStatistics_pyMemory} {}

    void *malloc(size_t size) {
        if (size == 0) {
            size = 1;
        }
        auto ret = CCP_MALLOC("PyRawMalloc", size);
        if (ret) {
#if _WIN32
			// On Windows we either get what we request, or it fails.
			int64_t tmp(size);
#elif __APPLE__
			// On macOS we receive an aligned memory block which may be larger.
			int64_t tmp = CCP_MSIZE( ret );
#endif
			mStats.Add( tmp );
        } else {
            CcpCrashOnPurpose();
        }
        return ret;
    }

    void *calloc(size_t nelem, size_t size) {
        if (nelem == 0 || size == 0) {
            nelem = 1;
            size = 1;
        }

        auto ret = CCP_CALLOC("PyRawCalloc", nelem, size);
		if ( ret )
		{
			int64_t tmp = CCP_MSIZE( ret );
			mStats.Add( tmp );
		}

        return ret;
    }

    void *realloc(void *ptr, size_t newSize) {
        if (newSize == 0) {
            newSize = 1;
        }
        if (ptr != nullptr) {
            int64_t oldSize(CCP_MSIZE(ptr));
			if ( oldSize >= 0 ) {
				mStats.Add(-oldSize);
			}
        }
        auto ret = CCP_REALLOC("Ccp_PyRawRealloc", ptr, newSize);
        if (ret != nullptr) {
			int64_t tmp = CCP_MSIZE( ret );
            mStats.Add( tmp );
        } else {
            CcpCrashOnPurpose();
        }
        return ret;
    }

    void free(void *ptr) {
        if (ptr != nullptr) {
			int64_t tmp = CCP_MSIZE( ptr );
			if ( tmp >= 0 ) {
				mStats.Add( -tmp );
			}
        }
        CCP_FREE(ptr);
    }

private:
    CcpStaticStatisticsEntry &mStats;
};

namespace Ccp {
    void *PyRawMalloc(void *ctx, size_t size) {
        auto _this = reinterpret_cast<RawAllocator *>( ctx );
        return _this->malloc(size);
    }

    void *PyRawCalloc(void *ctx, size_t nelem, size_t size) {
        auto _this = reinterpret_cast<RawAllocator *>( ctx );
        return _this->calloc(nelem, size);
    }

    void *PyRawRealloc(void *ctx, void *ptr, size_t newSize) {
        auto _this = reinterpret_cast<RawAllocator *>( ctx );
        return _this->realloc(ptr, newSize);
    }

    void PyRawFree(void *ctx, void *ptr) {
        auto _this = reinterpret_cast<RawAllocator *>( ctx );
        _this->free(ptr);
    }

    void *PyMallocWithGIL(void *ctx, size_t size) {
        PyGILState_STATE gil = PyGILState_UNLOCKED;
        bool ensure_release = ( PyGILState_Check() == 0 && !_Py_IsFinalizing() );
        if (ensure_release) {
            gil = PyGILState_Ensure();
        }
        auto ret = Ccp::PyRawMalloc(ctx, size);
        if (ensure_release) {
            PyGILState_Release(gil);
        }
        return ret;
    }

    void *PyCallocWithGIL(void *ctx, size_t nelem, size_t size) {
        PyGILState_STATE gil = PyGILState_UNLOCKED;
        bool ensure_release = ( PyGILState_Check() == 0 && !_Py_IsFinalizing() );
        if (ensure_release) {
            gil = PyGILState_Ensure();
        }
        auto ret = Ccp::PyRawCalloc(ctx, nelem, size);
        if (ensure_release) {
            PyGILState_Release(gil);
        }
        return ret;
    }

    void *PyReallocWithGIL(void *ctx, void *ptr, size_t size) {
        PyGILState_STATE gil = PyGILState_UNLOCKED;
        bool ensure_release = ( PyGILState_Check() == 0 && !_Py_IsFinalizing() );
        if (ensure_release) {
            gil = PyGILState_Ensure();
        }
        auto ret = Ccp::PyRawRealloc(ctx, ptr, size);
        if (ensure_release) {
            PyGILState_Release(gil);
        }
        return ret;
    }

    void PyFreeWithGIL(void *ctx, void *ptr) {
        PyGILState_STATE gil = PyGILState_UNLOCKED;
        bool ensure_release = ( PyGILState_Check() == 0 && !_Py_IsFinalizing() );
        if (ensure_release) {
            gil = PyGILState_Ensure();
        }
        Ccp::PyRawFree(ctx, ptr);
        if (ensure_release) {
            PyGILState_Release(gil);
        }
    }
}

extern "C" BLUEIMPORT void BlueInstallPythonMemoryHooks()
{
    static RawAllocator rawAllocator;

    static PyMemAllocatorEx rawAllocatorEx = {
            /* .ctx = */ &rawAllocator,
            /* .malloc = */ Ccp::PyRawMalloc,
            /* .calloc = */ Ccp::PyRawCalloc,
            /* .realloc = */ Ccp::PyRawRealloc,
            /* .free = */ Ccp::PyRawFree,
    };
	PyMemAllocatorEx alloc;

	PyMem_GetAllocator(PYMEM_DOMAIN_RAW, &alloc);
	if ( alloc.ctx != &rawAllocator ) {
		PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &rawAllocatorEx);
	}

    static PyMemAllocatorEx objAllocatorEx = {
            /* .ctx = */ &rawAllocator,
            /* .malloc = */ Ccp::PyMallocWithGIL,
            /* .calloc = */ Ccp::PyCallocWithGIL,
            /* .realloc = */ Ccp::PyReallocWithGIL,
            /* .free = */ Ccp::PyFreeWithGIL,
    };

	PyMem_GetAllocator(PYMEM_DOMAIN_OBJ, &alloc);
	if ( alloc.ctx != &rawAllocator ) {
		PyMem_SetAllocator(PYMEM_DOMAIN_OBJ, &objAllocatorEx);
	}

    static PyMemAllocatorEx memAllocatorEx = {
            /* .ctx = */ &rawAllocator,
            /* .malloc = */ Ccp::PyMallocWithGIL,
            /* .calloc = */ Ccp::PyCallocWithGIL,
            /* .realloc = */ Ccp::PyReallocWithGIL,
            /* .free = */ Ccp::PyFreeWithGIL,
    };

	PyMem_GetAllocator(PYMEM_DOMAIN_MEM, &alloc);
	if ( alloc.ctx != &rawAllocator ) {
		PyMem_SetAllocator(PYMEM_DOMAIN_MEM, &memAllocatorEx);
	}

}

#endif
