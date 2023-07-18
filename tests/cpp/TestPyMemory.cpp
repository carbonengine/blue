#include <PyMemory.h>

CCP_STATS_DECLARED_ELSEWHERE( pyMemory );

class PythonMemoryAllocatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        InstallPythonMemoryHooks();
        Py_Initialize();
    }

    void TearDown() override {
        Py_FinalizeEx();
    }
};

TEST_F(PythonMemoryAllocatorTest, GetInstalledCorrectly)
{
    PyMemAllocatorEx observed;
    PyMem_GetAllocator( PYMEM_DOMAIN_RAW, &observed );
    ASSERT_NE( observed.ctx, nullptr );
    EXPECT_EQ( observed.malloc, &Ccp::PyRawMalloc );
    EXPECT_EQ( observed.calloc, &Ccp::PyRawCalloc );
    EXPECT_EQ( observed.realloc, &Ccp::PyRawRealloc );
    EXPECT_EQ( observed.free, &Ccp::PyRawFree );

    // mem and obj allocators require that the GIL is held, see:
    // https://docs.python.org/3/c-api/memory.html#memory-interface
    // https://docs.python.org/3/c-api/memory.html#object-allocators
    PyMem_GetAllocator( PYMEM_DOMAIN_MEM, &observed );
    ASSERT_NE( observed.ctx, nullptr );
    EXPECT_EQ( observed.malloc, &Ccp::PyMallocWithGIL );
    EXPECT_EQ( observed.calloc, &Ccp::PyCallocWithGIL );
    EXPECT_EQ( observed.realloc, &Ccp::PyReallocWithGIL );
    EXPECT_EQ( observed.free, &Ccp::PyFreeWithGIL );

    PyMem_GetAllocator( PYMEM_DOMAIN_OBJ, &observed );
    ASSERT_NE( observed.ctx, nullptr );
    EXPECT_EQ( observed.malloc, &Ccp::PyMallocWithGIL );
    EXPECT_EQ( observed.calloc, &Ccp::PyCallocWithGIL );
    EXPECT_EQ( observed.realloc, &Ccp::PyReallocWithGIL );
    EXPECT_EQ( observed.free, &Ccp::PyFreeWithGIL );
}

TEST_F(PythonMemoryAllocatorTest, TracksAllocatedAmount)
{
    CcpStatistics::Update();
    auto initial = int64_t( CCP_STATS_GET( pyMemory ) );
    PyMemAllocatorEx observed;
    PyMem_GetAllocator( PYMEM_DOMAIN_RAW, &observed );
    auto mem = observed.malloc( observed.ctx, 128 );
    CcpStatistics::Update();
    EXPECT_GE( int64_t( CCP_STATS_GET( pyMemory ) ), initial + 128 );
    observed.free( observed.ctx, mem );
    CcpStatistics::Update();
    EXPECT_EQ( int64_t( CCP_STATS_GET( pyMemory ) ), initial );
}
