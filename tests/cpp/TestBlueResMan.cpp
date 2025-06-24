#include <BluePyCpp.h>
#include <BlueResMan.h>
#include <IBluePaths.h>
#include <ResourceLoading.h>


class BlueResManTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		PyPreConfig preConfig;
		PyPreConfig_InitIsolatedConfig( &preConfig );
		auto status = Py_PreInitialize( &preConfig );
		if( PyStatus_Exception( status ) )
		{
			GTEST_FAIL() << "Failed pre-initializing Python: " << status.err_msg << "\n";
		}

		// We need to initialize the Python interpreter when running tests, otherwise calls to the Python C API will fail.
		// There are a few things that we need to take care of when initializing the interpreter for the tests. Primarily,
		// blueExposure - just like blue - doesn't have access to our `pythonInterpreter` files. Therefore, we need to
		// emulate quite a bit of logic that we'd otherwise get for free, specifically around constructing the `PYTHONPATH`.
		// First of all - and this is the easy bit - we add the location of the Python standard library inside the Perforce
		// branch to it. It's straight forward, and doesn't depend on any magic - just on the environment variable that is
		// already required to exist. We can then construct the path based on the variable and the well-known location.
		// The less obvious bit is adding "." to the `PYTHONPATH` as well. This is necessary for a few reasons.
		// The primary reason is that we cannot know the path to the Python runtime and built-in C extensions (it contains a
		// version number). By adding "." we leave it up to the caller to run the tests from a working directory that
		// includes these runtime files.
		PyConfig config;
		static wchar_t pythonPath[2048] = {};
		static wchar_t packagesPath[2048] = {};
		static wchar_t resPath[2048] = {};
		const char* envBranchPath = std::getenv( "CCP_EVE_PERFORCE_BRANCH_PATH" );
		if( !envBranchPath )
		{
			GTEST_FAIL() << "Could not find `CCP_EVE_PERFORCE_BRANCH_PATH` environment variable, thus Python cannot be initialized correctly.\n";
		}

		const char* envResPath = std::getenv( "RES_PATH" );
		if( !envResPath )
		{
			GTEST_FAIL() << "Could not find `RES_PATH` environment variable, thus the tests can't find their resource files.";
		}
#if _MSC_VER
		swprintf( pythonPath, sizeof( pythonPath ) / sizeof( *pythonPath ), L"%S/carbon/common/stdlib", envBranchPath );
		swprintf( packagesPath, sizeof( packagesPath ) / sizeof( *packagesPath ), L"%S/packages", envBranchPath );
		swprintf( resPath, sizeof( resPath ) / sizeof( *resPath ), L"res=%S", envResPath );
#else
		swprintf( pythonPath, sizeof( pythonPath ) / sizeof( *pythonPath ), L"%s/carbon/common/stdlib", envBranchPath );
		swprintf( packagesPath, sizeof( packagesPath ) / sizeof( *packagesPath ), L"%s/packages", envBranchPath );
		swprintf( resPath, sizeof( resPath ) / sizeof( *resPath ), L"res=%s", envResPath );
#endif


		PyConfig_InitIsolatedConfig( &config );
		PyWideStringList_Append( &config.module_search_paths, L"." );
		PyWideStringList_Append( &config.module_search_paths, pythonPath );
		PyWideStringList_Append( &config.module_search_paths, packagesPath );

		config.module_search_paths_set = 1;
		config.use_environment = 0;

		if( PyStatus_Exception( Py_InitializeFromConfig( &config ) ) )
		{
			PyErr_Print();
			GTEST_FAIL() << "Failed initializing Python\n";
		}

		PyConfig_Clear( &config );
		if( Py_IsInitialized() == 0 )
		{
			GTEST_FAIL() << "Failed initializing Python interpreter\n";
		}

		if( !InstallImportHook() )
		{
			GTEST_FAIL() << "Failed installing import hook\n";
		}

		auto schedulerModule = PyImport_ImportModule( "scheduler" );
		if( !schedulerModule )
		{
			GTEST_FAIL() << "Failed to import 'scheduler' module";
		}

		auto carbonSocketModule = PyImport_ImportModule( "_carbonsocket" );
		if( !carbonSocketModule )
		{
			PyErr_WriteUnraisable( nullptr );
			GTEST_FAIL() << "Failed to import `_carbonsocket` module";
		}

		BlueModuleStartup();

		std::wstring initialResourcePath = L".";
		ASSERT_EQ( BlueInitializePaths( initialResourcePath ), true );

		BlueInitializeResourceLoading();

		ASSERT_NE( BlueGetBeOS(), nullptr );
		bool startupOk = BlueGetBeOS()->Startup( 0 );
		ASSERT_TRUE( startupOk );

		std::vector<std::wstring> searchPaths;
		searchPaths.push_back( resPath );
		BlueSetSearchPaths( searchPaths );

		::testing::Test::SetUp();
	}

	void TearDown() override
	{
		Py_FinalizeEx();
		::testing::Test::TearDown();
	}
};

void loadPath( std::string path )
{
	auto beResMan = GetBeResMan();
	auto res = beResMan->LoadObject( path.c_str() );
	ASSERT_NE( res, nullptr );
}

TEST_F( BlueResManTest, CallLoadObjectOnEmptyPath )
{
	auto beResMan = GetBeResMan();
	ASSERT_EQ( beResMan->LoadObject( "" ), nullptr );
}

TEST_F( BlueResManTest, CallLoadObjectWithGil )
{
	auto beResMan = GetBeResMan();

	Ccp::PyGilEnsure gil;
	ASSERT_NE( beResMan->LoadObject( "res:\\TestCases\\stringattribute.txt" ), nullptr );
}

TEST_F( BlueResManTest, CallLoadObjectWithoutGil )
{
	auto beResMan = GetBeResMan();
	ASSERT_NE( beResMan->LoadObject( "res:\\TestCases\\stringattribute.txt" ), nullptr );
}

TEST_F( BlueResManTest, CallLoadObjectFromAnotherThread )
{
	std::thread t( loadPath, "res:\\TestCases\\stringattribute.txt" );
	t.join();
}
