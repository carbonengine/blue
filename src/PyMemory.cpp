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
            mStats.Add(int64_t(size));
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

        return ret;
    }

    void *realloc(void *ptr, size_t newSize) {
        if (newSize == 0) {
            newSize = 1;
        }
        if (ptr != nullptr) {
            int64_t oldSize(CCP_MSIZE(ptr));
            mStats.Add(-oldSize);
        }
        auto ret = CCP_REALLOC("Ccp_PyRawRealloc", ptr, newSize);
        if (ret != nullptr) {
            mStats.Add(int64_t(newSize));
        } else {
            CcpCrashOnPurpose();
        }
        return ret;
    }

    void free(void *ptr) {
        if (ptr != nullptr) {
            mStats.Add(-int64_t(CCP_MSIZE(ptr)));
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

void InstallPythonMemoryHooks() {
    static RawAllocator *rawAllocator{nullptr};

    if (rawAllocator != nullptr) {
        return;
    }

    rawAllocator = new RawAllocator();

    PyMemAllocatorEx rawAllocatorEx = {
            /* .ctx = */ rawAllocator,
            /* .malloc = */ Ccp::PyRawMalloc,
            /* .calloc = */ Ccp::PyRawCalloc,
            /* .realloc = */ Ccp::PyRawRealloc,
            /* .free = */ Ccp::PyRawFree,
    };
    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &rawAllocatorEx);

    PyMemAllocatorEx objAllocatorEx = {
            /* .ctx = */ rawAllocator,
            /* .malloc = */ Ccp::PyMallocWithGIL,
            /* .calloc = */ Ccp::PyCallocWithGIL,
            /* .realloc = */ Ccp::PyReallocWithGIL,
            /* .free = */ Ccp::PyFreeWithGIL,
    };
    PyMem_SetAllocator(PYMEM_DOMAIN_OBJ, &objAllocatorEx);

    PyMemAllocatorEx memAllocatorEx = {
            /* .ctx = */ rawAllocator,
            /* .malloc = */ Ccp::PyMallocWithGIL,
            /* .calloc = */ Ccp::PyCallocWithGIL,
            /* .realloc = */ Ccp::PyReallocWithGIL,
            /* .free = */ Ccp::PyFreeWithGIL,
    };
    PyMem_SetAllocator(PYMEM_DOMAIN_MEM, &memAllocatorEx);
}

#endif
