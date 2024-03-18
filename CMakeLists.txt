cmake_minimum_required(VERSION 3.16)

project(Blue VERSION 1.0.0)
if(NOT DEFINED ENV{CCP_EVE_PERFORCE_BRANCH_PATH})
    message(FATAL_ERROR "Missing required environment variable CCP_EVE_PERFORCE_BRANCH_PATH")
elseif(NOT IS_ABSOLUTE $ENV{CCP_EVE_PERFORCE_BRANCH_PATH})
    message(FATAL_ERROR "CCP_EVE_PERFORCE_BRANCH_PATH needs to be an absolute path")
elseif(NOT EXISTS $ENV{CCP_EVE_PERFORCE_BRANCH_PATH})
    message(FATAL_ERROR "CCP_EVE_PERFORCE_BRANCH_PATH is pointing at a non-existing location $ENV{CCP_EVE_PERFORCE_BRANCH_PATH}")
elseif(NOT IS_DIRECTORY $ENV{CCP_EVE_PERFORCE_BRANCH_PATH})
    message(FATAL_ERROR "CCP_EVE_PERFORCE_BRANCH_PATH needs to be a directory")
else()
    message(STATUS "Using CCP_EVE_PERFORCE_BRANCH_PATH located at $ENV{CCP_EVE_PERFORCE_BRANCH_PATH}")
endif()

file(TO_CMAKE_PATH "$ENV{CCP_EVE_PERFORCE_BRANCH_PATH}" BRANCH_ROOT_DIR)
list(INSERT CMAKE_MODULE_PATH 0 ${BRANCH_ROOT_DIR}/cmake)
include(CcpGlobalSettings)
include(CcpPackageConfigHelpers)

find_package(BlueExposure REQUIRED NO_CMAKE_PATH)
find_package(CURL REQUIRED NO_CMAKE_PATH)
find_package(OpenSSL REQUIRED NO_CMAKE_PATH)
find_package(PDM REQUIRED NO_CMAKE_PATH)
find_package(YAML REQUIRED NO_CMAKE_PATH)
find_package(ZLIB REQUIRED NO_CMAKE_PATH)
find_package(Scheduler REQUIRED NO_CMAKE_PATH)


set(PUBLIC_HEADER_FILES_INCLUDE
    include/Blue.h
    include/BlueAsyncRes.h
    include/BluePySwrap.h
    include/BlueStatistics.h
    include/IBlueCallbackMan.h
    include/IBlueEventListener.h
    include/IBlueObjectProxy.h
    include/IBlueObjectBuilder.h
    include/IBlueOS.h
    include/IBluePaths.h
    include/IBluePersist.h
    include/IBluePython.h
    include/IBluePlacementObserver.h
    include/IBlueResMan.h
    include/IBlueResource.h
    include/IBlueObjectMetadata.h
    include/ICacheable.h
    include/ICcpStatisticsAccumulator.h
    include/ICurveSetDriver.h
    include/ITr2DebugRenderer2.h
    include/ITaskletTimer.h
    include/ITriConstants.h
    include/ITriCurveLength.h
    include/ITriFunction.h
    include/ITriObserverLocal.h
    include/ScopedBlockTrap.h
    include/BlueObjectMetadata.h
    include/BluePyCpp.h
    include/BluePlatform.h
    include/BluePlatformPosix.h
    include/BluePlatformWin32.h
    include/Rect.h
    include/stacklessio.h
    include/SchedulerCAPI.h
)


ccp_add_library(Blue SHARED
    src/Logger/Logger.cpp
    src/CarbonIO/dll_exports.h
    src/CarbonIO/protocol.h
    src/BlueSysInfo.mm
    src/Accumulator/PercentileAccumulator.cpp
    src/Accumulator/PercentileAccumulator_Blue.cpp
    src/AllReferences.cpp
    src/AllReferences.h
    src/AllReferences_Blue.cpp
    src/autoversion.h
    src/BackgroundReader.cpp
    src/BackgroundReader.h
    src/Base64.cpp
    src/BlackReader.cpp
    src/BlackReader.h
    src/BlackReader_Blue.cpp
    src/BlackWriter.cpp
    src/BlackWriter.h
    src/BlackWriter_Blue.cpp
    src/BlueAsyncRes.cpp
    src/BlueAsyncRes_Blue.cpp
    src/BlueClipboard.cpp
    src/BlueClipboard.h
    src/BlueClipboard.mm
    src/BlueClipboard_Blue.cpp
    src/BlueEventToPython.cpp
    src/BlueEventToPython.h
    src/BlueEventToPython_Blue.cpp
    src/BlueFileStream.cpp
    src/BlueFileStream.h
    src/BlueFileStream_Blue.cpp
    src/BlueFileUtil.cpp
    src/BlueHeapq.cpp
    src/BlueHeapq.h
    src/BlueLogInMemory.cpp
    src/BlueLogInMemory.h
    src/BlueLogInMemory_Blue.cpp
    src/BlueMemoryTracker.cpp
    src/BlueMemoryTracker.h
    src/BlueMemoryTracker_Blue.cpp
    src/BlueMemStream.cpp
    src/BlueMemStream.h
    src/BlueMemStream_Blue.cpp
    src/BlueNet.cpp
    src/BlueNetworkStream.cpp
    src/BlueNetworkStream.h
    src/BlueNetworkStream_Blue.cpp
    src/BlueOS.cpp
    src/BlueOS.h
    src/BlueOS.mm
    src/BlueOS_Blue.cpp
    src/BlueObjectBuilderPython.cpp
    src/BlueObjectBuilderPython.h
    src/BlueObjectBuilderPython_Blue.cpp
    src/BlueObjectMetadata.cpp
    src/BlueObjectMetadata_Blue.cpp
    src/BlueObjectProxy.cpp
    src/BlueObjectProxy.h
    src/BlueObjectProxy_Blue.cpp
    src/BlueObjectRecycler.cpp
    src/BlueObjectRecycler.h
    src/BlueObjectRecycler_Blue.cpp
    src/BluePaths.cpp
    src/BluePaths.h
    src/BluePaths_Blue.cpp
    src/BluePython.cpp
    src/BluePython.h
    src/BluePython_Blue.cpp
    src/BluePythonWeakRef.cpp
    src/BluePythonWeakRef.h
    src/BluePythonWeakRef_Blue.cpp
    src/BlueRemoteStream.cpp
    src/BlueRemoteStream.h
    src/BlueRemoteStream_Blue.cpp
    src/BlueResFile.h
    src/BlueResFile2.cpp
    src/BlueResFile2.h
    src/BlueResFile2_Blue.cpp
    src/BlueResFileSystemLocal.cpp
    src/BlueResFileSystemLocal.h
    src/BlueResFileSystemLocal_Blue.cpp
    src/BlueResFileSystemRemote.cpp
    src/BlueResFileSystemRemote.h
    src/BlueResFileSystemRemote_Blue.cpp
    src/BlueResMan.cpp
    src/BlueResMan.h
    src/BlueResMan_Blue.cpp
    src/BlueResManBackgroundCall.cpp
    src/BlueResManBackgroundCall.h
    src/BlueSocketLogger.cpp
    src/BlueSocketLogger.h
    src/BlueStatistics.cpp
    src/BlueStatistics_Blue.cpp
    src/BlueStreamMethods.cpp
    src/BlueSysInfo.cpp
    src/BlueSysInfo.h
    src/BlueSysInfo_Blue.cpp
    src/BlueTestHelpers.cpp
    src/BlueTestHelpers.h
    src/BlueTestHelpers_Blue.cpp
    src/BlueTextResource.cpp
    src/BlueTextResource.h
    src/BlueTextResource_Blue.cpp
    src/BlueThreadMonitor.cpp
    src/BlueThreadMonitor.h
    src/BlueThreadMonitor_Blue.cpp
    src/BlueTime.cpp
    src/BlueTime.h
    src/BlueTimeoutHandler.cpp
    src/BlueTimeoutHandler.h
    src/CallbackMan.cpp
    src/CallbackMan.h
    src/CallbackMan_Blue.cpp
    src/DictReader.cpp
    src/DictReader.h
    src/DictReader_Blue.cpp
    src/errormessage.cpp
    src/errormessage.h
    src/FileLogger.cpp
    src/FileLogger.h
    src/IBlueEventListener_Blue.cpp
    src/IBlueObjectBuilder_Blue.cpp
    src/IBluePlacementObserver_Blue.cpp
    src/InterfaceDefinitions.cpp
    src/IOLoop.cpp
    src/IOLoop.h
    src/IRootReader.cpp
    src/IRootReader.h
    src/IRootWriter.cpp
    src/IRootWriter.h
    src/IRootWriter_Blue.cpp
    src/LinkHash.h
    src/LogControl.cpp
    src/LogControl.h
    src/LogToPython.cpp
    src/LogToPython.h
    src/Marshal.cpp
    src/Marshal.h
    src/MotherLode.cpp
    src/MotherLode.h
    src/MotherLode_Blue.cpp
    src/NetworkLogger.cpp
    src/NetworkLogger.h
    src/PrettyPrint.cpp
    src/PrettyPrint.h
    src/PyLogObject.cpp
    src/PyMemory.cpp
    src/PyRowSet.cpp
    src/PyRowSet.h
    src/PyScheduler.cpp
    src/PyScheduler.h
    src/RemoteFileCache.cpp
    src/RemoteFileCache.h
    src/RemoteFileCache_Blue.cpp
    src/ResourceLoading.cpp
    src/ResourceLoading.h
    src/ResourceWatch.cpp
    src/ResourceWatch.h
    src/ResourceWatch_Blue.cpp
    src/ScopedBlockTrap.cpp
    src/SimpleJson.cpp
    src/SimpleJson.h
    src/StdAfx.cpp
    src/Synchro.cpp
    src/Synchro.h
    src/TaskletTimer.cpp
    src/TaskletTimer.h
    src/TaskletTimer_Blue.cpp
    src/WatchdogThread.cpp
    src/WatchdogThread.h
    src/XUtil.cpp
    src/YamlReader.cpp
    src/YamlReader.h
    src/YamlReader_Blue.cpp
    src/YamlWriter.cpp
    src/YamlWriter.h
    src/YamlWriter_Blue.cpp
    src/blue.cpp
    src/crypto.cpp
    src/crypto.h
    src/crypto_Blue.cpp
    src/manifest.cpp
    src/manifest.h
    src/md5.cpp
    src/md5.h
    src/RWSpinLock.h
    src/ScopedLocks.h
    src/slsocketmodule.c
    src/slsocketmodule.h
    src/slselectmodule.c
    src/slsocket.h
    src/stacklessio.cpp
    src/stacklessio_api.h
    src/schedulerCAPI.cpp
    src/version.h
    src/win32.cpp
    src/win32.h
)

if (APPLE)
    target_sources(Blue PRIVATE src/slsocket_posix.cpp)
elseif (WIN32)
    target_sources(Blue PRIVATE
        src/CarbonIO/api.cpp
        src/CarbonIO/api.h
        src/CarbonIO/CarbonIO.cpp
        src/CarbonIO/CarbonIO.h
        src/CarbonIO/dll_exports.cpp
        src/CarbonIO/ssl_pipe.cpp
        src/CarbonIO/ssl_pipe.h
        src/CarbonIO/stats.cpp
        src/CarbonIO/stats.h
        src/slsocket.cpp
    )
endif()

# These files don't play nicely with PCH, partially because they are C and not C++, so let's exclude them from
# using PCH until such a time that they are converted to C++
set_source_files_properties(
    src/slsocketmodule.c
    src/slselectmodule.c
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

target_sources(Blue PRIVATE
    include/Base64.h
    include/BitPacker.h
    include/BlueChoosers.h
    include/BlueFileUtil.h
    include/BlueMain.h
    include/BlueNet.h
    include/BlueNetTypes.h
    include/DataStructs.h
    include/IBlueObjectRecycler.h
    include/IBlueResFileSystem.h
    include/IBlueThreadMonitor.h
    include/IMotherLode.h
    include/IUnloadable.h
    include/StructPacker.h
    ${PUBLIC_HEADER_FILES_INCLUDE}
)

target_precompile_headers(Blue PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/StdAfx.h)
target_compile_definitions(Blue
    PUBLIC
        CCP_STACKLESS
    )

if(APPLE)
    enable_language(OBJCXX)
    target_compile_definitions(Blue PRIVATE PORTING_TO_LINUX NO_CARBONIO)
    target_link_libraries(Blue PRIVATE "-framework Foundation -framework IOKit -framework AppKit")
endif()

target_link_libraries(Blue
    PUBLIC
        BlueExposure
        CcpCore
    PRIVATE
        PDM::PDM
        CURL::libcurl
        YAML::YAML
        ZLIB::ZLIB
        OpenSSL::Crypto
        OpenSSL::SSL
)

if(WIN32)
    set_target_properties(Blue PROPERTIES SUFFIX ".pyd")
    target_link_libraries(Blue
        PRIVATE
            crypt32
            ws2_32
            version
            winhttp
            psapi
            iphlpapi
)
endif()

get_target_property(SCHEDULER_INCLUDE_DIR Scheduler INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(Blue
    PUBLIC
        ${SCHEDULER_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(Blue PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(Blue PROPERTIES OUTPUT_NAME "blue")

add_subdirectory(src/_ssl)

# Provide tests and an install target iff this is the top level project only
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # tests
    option(BUILD_TESTING "Build tests. Enabled by default." ON)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
    # discover python tests ... this may be a little bit brittle since we're working around the missing `_blueexposuretest`
    # binary. As in, test discovery happens during the configure stage at which the binary does not exist yet.
    if(WIN32)
        set(ENV{PYTHONPATH} ${BRANCH_ROOT_DIR}/carbon/common/stdlib\;${Python_ROOT}/${CCP_VENDOR_BIN_PATH}\;${CMAKE_CURRENT_SOURCE_DIR}/python\;${Scheduler_ROOT}/${CCP_VENDOR_BIN_PATH}\;${Scheduler_ROOT}/python)
    elseif(APPLE)
        set(ENV{PYTHONPATH} ${BRANCH_ROOT_DIR}/carbon/common/stdlib:${Python_ROOT}/${CCP_VENDOR_BIN_PATH}:${CMAKE_CURRENT_SOURCE_DIR}/python:${Scheduler_ROOT}/${CCP_VENDOR_LIB_PATH}:${Scheduler_ROOT}/python)
    endif()
    execute_process(COMMAND ${PYTHON_EXECUTABLE} discover.py
        OUTPUT_VARIABLE PYTHON_TESTS_STR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE PYTHON_TEST_DISCOVERY_RETVAL
        ERROR_VARIABLE PYTHON_TEST_DISCOVERY_ERRORS
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/python
    )
    if(${PYTHON_TEST_DISCOVERY_RETVAL})
        message(FATAL_ERROR "Error encountered during test discovery:\n=== stderr: ===\n${PYTHON_TEST_DISCOVERY_ERRORS}\n=== stdout: ===\n${PYTHON_TESTS_STR}")
    endif()
    separate_arguments(PYTHON_TESTS UNIX_COMMAND ${PYTHON_TESTS_STR})
    foreach(PYTHON_TEST ${PYTHON_TESTS})
        message(STATUS "Found test ${PYTHON_TEST}")
        add_test(NAME ${PYTHON_TEST}
            COMMAND ${PYTHON_EXECUTABLE} -m bluetests -v ${PYTHON_TEST}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/python
        )
        if(WIN32)
            set_tests_properties(${PYTHON_TEST} PROPERTIES ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:Blue>\;${BRANCH_ROOT_DIR}/carbon/common/stdlib\;${Python_ROOT}/${CCP_VENDOR_BIN_PATH}\;${Scheduler_ROOT}/${CCP_VENDOR_BIN_PATH}\;${Scheduler_ROOT}/python\;${CMAKE_CURRENT_SOURCE_DIR}/python;BUILDFLAVOR=$<LOWER_CASE:$<CONFIG>>")
        elseif (APPLE)
            set_tests_properties(${PYTHON_TEST} PROPERTIES ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:Blue>:${BRANCH_ROOT_DIR}/carbon/common/stdlib:${Python_ROOT}/${CCP_VENDOR_BIN_PATH}:${Scheduler_ROOT}/${CCP_VENDOR_LIB_PATH}:${Scheduler_ROOT}/python:${CMAKE_CURRENT_SOURCE_DIR}/python;BUILDFLAVOR=$<LOWER_CASE:$<CONFIG>>")
        else()
            message(FATAL_ERROR "Unsupported platform")
        endif ()
    endforeach()

    # Install rule to ensure that our runtime and linker files are in the expected, platform-specific folders
    install(
        TARGETS Blue
        EXPORT BlueTargets
        CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
        ARCHIVE DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
        LIBRARY DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
        RUNTIME DESTINATION bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
    )
    # Install rule for available public headers
    install(DIRECTORY include DESTINATION . FILES_MATCHING PATTERN "*.h")

    configure_ccp_vendor_config_file(
        TARGET Blue
        DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/BlueConfig.cmake"
        DEPENDENCIES "BlueExposure;CURL;OpenSSL;PDM;YAML;ZLIB"
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/BlueConfig.cmake DESTINATION .)

    # Install python package
    install(DIRECTORY python/ DESTINATION python
            FILES_MATCHING
            PATTERN "*.py"
    )
endif()
