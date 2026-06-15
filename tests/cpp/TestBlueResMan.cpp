// Copyright © 2025 CCP ehf.

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
		// First of all - and this is the easy bit - we add the location of the Python standard library.
		// We then configure the interpreter in such a way that it's able to import `scheduler` and `_carbonsocket`.
		// This is done through the `IMPORT_PATH` environment variable, which should point to the vcpkg installation directory
		// where said dependencies are accessible from.
		// The less obvious bit is adding "." to the `PYTHONPATH` as well. This is necessary for a few reasons.
		// The primary reason is that we cannot know the path to the Python runtime and built-in C extensions (it contains a
		// version number). By adding "." we leave it up to the caller to run the tests from a working directory that
		// includes these runtime files.
		PyConfig config;
		static wchar_t pythonPath[2048] = {};
		static wchar_t packagePath[2048] = {};
		static wchar_t modulePath[2048] = {};
		static wchar_t extensionPath[2048] = {};
		static wchar_t resPath[2048] = {};

		const char* envPythonPath = std::getenv( "PYTHON_STDLIB" );
		if( !envPythonPath )
		{
			GTEST_FAIL() << "Could not find `PYTHON_STDLIB` environment variable, thus Python cannot be initialized correctly.\n";
		}

		const char* envResPath = std::getenv( "RES_PATH" );
		if( !envResPath )
		{
			GTEST_FAIL() << "Could not find `RES_PATH` environment variable, thus the tests can't find their resource files.\n";
		}

		const char* envImportPath = std::getenv( "IMPORT_PATH" );
		if( !envImportPath )
		{
			GTEST_FAIL() << "Could not find `IMPORT_PATH` environment variable, thus the test fixture cannot be initialized.\n";
		}

#if WIN32
		// Python package directory
		swprintf( packagePath, sizeof( packagePath ) / sizeof( *packagePath ), L"%S/bin/python", envImportPath );
		// Python extension directory
		swprintf( extensionPath, sizeof( extensionPath ) / sizeof( *extensionPath ), L"%S/bin", envImportPath );
		// Scheduler is installed as a module, meaning the scheduler extension is installed under lib/
		swprintf( modulePath, sizeof( modulePath ) / sizeof( *modulePath ), L"%S/lib", envImportPath );
		swprintf( pythonPath, sizeof( pythonPath ) / sizeof( *pythonPath ), L"%S", envPythonPath );
		swprintf( resPath, sizeof( resPath ) / sizeof( *resPath ), L"res=%S", envResPath );
#else
		// Python package directory
		swprintf( packagePath, sizeof( packagePath ) / sizeof( *packagePath ), L"%s/bin/python", envImportPath );
		// Python extension directory
		swprintf( extensionPath, sizeof( extensionPath ) / sizeof( *extensionPath ), L"%s/bin", envImportPath );
		// Scheduler is installed as a module, meaning the scheduler extension is installed under lib/
		swprintf( modulePath, sizeof( modulePath ) / sizeof( *modulePath ), L"%s/lib", envImportPath );
		swprintf( pythonPath, sizeof( pythonPath ) / sizeof( *pythonPath ), L"%s", envPythonPath );
		swprintf( resPath, sizeof( resPath ) / sizeof( *resPath ), L"res=%s", envResPath );
#endif

		PyConfig_InitIsolatedConfig( &config );
		PyWideStringList_Append( &config.module_search_paths, L"." );
		PyWideStringList_Append( &config.module_search_paths, packagePath );
		PyWideStringList_Append( &config.module_search_paths, extensionPath );
		PyWideStringList_Append( &config.module_search_paths, modulePath );
		PyWideStringList_Append( &config.module_search_paths, pythonPath );

		config.module_search_paths_set = 1;

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
			PyErr_Print();
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
