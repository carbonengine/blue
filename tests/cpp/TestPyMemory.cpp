#include <PyMemory.h>

CCP_STATS_DECLARED_ELSEWHERE( pyMemory );

class PythonMemoryAllocatorTest : public ::testing::Test {
protected:
    void SetUp() override {
		PyPreConfig preConfig;
		PyPreConfig_InitIsolatedConfig( &preConfig );
		preConfig.allocator = PYMEM_ALLOCATOR_NOT_SET;
		auto status = Py_PreInitialize( &preConfig );
		if ( PyStatus_Exception( status ) ) {
			GTEST_FAIL() << "Failed pre-initializing Python: " << status.err_msg << "\n";
		}
        BlueInstallPythonMemoryHooks();
    	// We need to initialize the Python interpreter when running tests, otherwise calls to the Python C API will fail.
    	// There are a few things that we need to take care of when initializing the interpreter for the tests. Primarily,
    	// blueExposure - just like blue - doesn't have access to our `pythonInterpreter` files. Therefore, we need to
    	// emulate quite a bit of logic that we'd otherwise get for free, specifically around constructing the `PYTHONPATH`.
    	// First of all - and this is the easy bit - we add the location of the Python standard library.
    	// We then need to configure the interpreter in such a way that it's able to import `scheduler` and `_carbonsocket`.
    	// This is done through the `IMPORT_PATH` environment variable, which points to the vcpkg installation directory
    	// where said dependencies are accessible from.
    	// The less obvious bit is adding "." to the `PYTHONPATH` as well. This is necessary for a few reasons.
    	// The primary reason is that we cannot know the path to the Python runtime and built-in C extensions (it contains a
    	// version number). By adding "." we leave it up to the caller to run the tests from a working directory that
    	// includes these runtime files.
		PyConfig config;
		static wchar_t pythonPath[2048] = {};
		const char* envPythonPath = std::getenv("PYTHON_STDLIB");
		if (!envPythonPath) {
			GTEST_FAIL() << "Could not find `PYTHON_STDLIB` environment variable, thus Python cannot be initialized correctly.\n";
		}

    	swprintf(pythonPath, sizeof(pythonPath) / sizeof(*pythonPath), L"%S", envPythonPath);

		PyConfig_InitIsolatedConfig(&config);
		PyWideStringList_Append(&config.module_search_paths, L".");
		PyWideStringList_Append(&config.module_search_paths, pythonPath);
		config.module_search_paths_set = 1;

    	if( PyStatus_Exception( Py_InitializeFromConfig( &config ) ) )
    	{
    		PyErr_Print();
    		GTEST_FAIL() << "Failed initializing Python\n";
    	}
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
