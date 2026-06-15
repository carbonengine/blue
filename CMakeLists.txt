# Copyright © 2021 CCP ehf.

cmake_minimum_required(VERSION 3.16)

project(Blue VERSION 1.0.0)

include(cmake/CcpTargetConfigurations.cmake)
include(cmake/CcpDocsGenerator.cmake)
include(cmake/CcpPackageConfigHelpers.cmake)
include(cmake/CcpBuildConfigurations.cmake)

find_package(carbon-core REQUIRED CONFIG)
find_package(carbon-blueexposure REQUIRED CONFIG)
find_package(carbon-io REQUIRED CONFIG)
find_package(CURL REQUIRED CONFIG)
find_package(YAML REQUIRED CONFIG)
find_package(zlib CONFIG REQUIRED)
find_package(carbon-scheduler REQUIRED CONFIG)
find_package(OpenSSL COMPONENTS Crypto SSL REQUIRED)
find_package(carbon-pdmprotowrapper REQUIRED CONFIG)
find_package(Python3 COMPONENTS Interpreter REQUIRED)


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
    include/stacklessio.h
)


ccp_add_library(Blue SHARED
    src/Logger/Logger.cpp
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
    src/version.h
    src/win32.cpp
    src/win32.h
)

target_sources(Blue PRIVATE
    include/Base64.h
    include/BitPacker.h
    include/BlueChoosers.h
    include/BlueFileUtil.h
    include/BlueMain.h
    include/BlueNet.h
    include/DataStructs.h
    include/IBlueObjectRecycler.h
    include/IBlueResFileSystem.h
    include/IBlueThreadMonitor.h
    include/IMotherLode.h
    include/IUnloadable.h
    include/StructPacker.h
    ${PUBLIC_HEADER_FILES_INCLUDE}
)

option(PY27_COMPATIBILITY_MODE "Enable Python2.7 compatibility mode" OFF)
target_precompile_headers(Blue PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/StdAfx.h)

target_compile_definitions(Blue
    PUBLIC
        CCP_STACKLESS
        $<$<BOOL:${PY27_COMPATIBILITY_MODE}>:PY27_COMPATIBILITY_MODE> # This define should get cleaned up when we stop supporting running the cluster with Python 27 and Python 312 nodes working together.
)

if(APPLE)
    enable_language(OBJCXX)
    target_compile_definitions(Blue PRIVATE PORTING_TO_LINUX)
    target_link_libraries(Blue PRIVATE "-framework SystemConfiguration -framework Foundation -framework IOKit -framework AppKit")
endif()

get_target_property(CARBONIO_INCLUDE_DIR _socket INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(Blue PRIVATE ${CARBONIO_INCLUDE_DIR})

target_link_libraries(Blue
    PUBLIC
        BlueExposure
        CcpCore
    PRIVATE
        platform_pdm_proto_wrapper
        CURL::libcurl
        yaml
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
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${SCHEDULER_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(Blue PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(Blue PROPERTIES OUTPUT_NAME "blue")

# Provide tests and an install target iff this is the top level project only
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    option(BUILD_TESTING "Build tests. Enabled by default." ON)
    if(BUILD_TESTING)
        include(CTest)
        add_subdirectory(tests)

        # discover python tests ... this may be a little bit brittle since we're working around the missing `_blueexposuretest`
        # binary. As in, test discovery happens during the configure stage at which the binary does not exist yet.
        # Discovery works with the standard python interpreter because it isn't running anything, it just needs to find all
        # the defined test cases. Therefore, just use that one, even though it cannot run the discovered test cases.
        set(ENV{PYTHONPATH} ${CMAKE_CURRENT_SOURCE_DIR}/python)
        execute_process(COMMAND ${Python3_EXECUTABLE} discover.py
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

        # Tests need to run through exefile. Exefile will attempt to load the blue library which requires blue to be
        # co-located with exefile alongside any runtime dependencies.
        # This needs to be hardened
        if (DEFINED CMAKE_BUILD_TYPE AND NOT CMAKE_BUILD_TYPE MATCHES "Release")
            set(EXEFILE_TOOL_NAME exefile_${CMAKE_BUILD_TYPE})
        else()
            set(EXEFILE_TOOL_NAME exefile)
        endif()
        find_program(exefile_tool ${EXEFILE_TOOL_NAME} NO_CACHE)

        if (WIN32)
            add_custom_command(TARGET Blue POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy ${exefile_tool} $<TARGET_FILE_DIR:Blue>
            )
            set(exefile_tool $<TARGET_FILE_DIR:Blue>/${EXEFILE_TOOL_NAME})
            if (WIN32)
                add_custom_command(
                        TARGET Blue POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:Blue> $<TARGET_FILE_DIR:Blue>
                        COMMAND_EXPAND_LISTS
                )
            endif ()
            set(TESTS_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/python)
        else()
            set(TESTS_WORKING_DIRECTORY $<TARGET_FILE_DIR:Blue>)
        endif()

        separate_arguments(PYTHON_TESTS UNIX_COMMAND ${PYTHON_TESTS_STR})
        foreach(PYTHON_TEST ${PYTHON_TESTS})
            message(STATUS "Found test ${PYTHON_TEST}")
            add_test(NAME ${PYTHON_TEST}
                COMMAND ${exefile_tool} /inherit /buildflavor=$<LOWER_CASE:$<CONFIG>> /py -m bluetests -v ${PYTHON_TEST}
                WORKING_DIRECTORY ${TESTS_WORKING_DIRECTORY}
            )
            if(WIN32)
                set_tests_properties(${PYTHON_TEST} PROPERTIES ENVIRONMENT  "PYTHONPATH=$<TARGET_FILE_DIR:Blue>\;${CMAKE_CURRENT_SOURCE_DIR}/python\;${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin\;${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib\;${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/python\;${Python3_STDLIB};BUILDFLAVOR=$<LOWER_CASE:$<CONFIG>>")
            elseif (APPLE)
                 set_tests_properties(${PYTHON_TEST} PROPERTIES ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/tests/python:$<TARGET_FILE_DIR:Blue>:${CMAKE_CURRENT_SOURCE_DIR}/python:${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin:${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib:${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/python3.12/lib-dynload:${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/python:${Python3_STDLIB};BUILDFLAVOR=$<LOWER_CASE:$<CONFIG>>")
            else()
                message(FATAL_ERROR "Unsupported platform")
            endif ()
        endforeach()
    endif()

    option(INSTALL_TO_MONOLITH "Install this package to a perforce branch vendor folder - provided for backwards compatability" OFF)
    if (INSTALL_TO_MONOLITH)
        # Install rule to ensure that our runtime and linker files are in the expected, platform-specific folders
        install(
            TARGETS Blue
            EXPORT BlueTargets
            CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
            ARCHIVE DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}
            LIBRARY DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}
            RUNTIME DESTINATION bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}
        )

        # Generate target configuration for the monolith
        configure_ccp_vendor_config_file(
                TARGET Blue
                DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/BlueConfig.cmake"
                DEPENDENCIES "BlueExposure;CURL;OpenSSL;PDM;YAML;ZLIB"
        )
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/BlueConfig.cmake DESTINATION .)
        install(DIRECTORY python DESTINATION . FILES_MATCHING PATTERN "*.py")
    else ()
        install(
                TARGETS Blue
                EXPORT BlueTargets
                CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
                ARCHIVE DESTINATION lib
                LIBRARY DESTINATION lib
                RUNTIME DESTINATION bin
        )

        install(EXPORT BlueTargets
                DESTINATION share/carbon-blue
                FILE carbon-blue.cmake
        )

        install(FILES carbon-blueConfig.cmake DESTINATION ${CMAKE_INSTALL_PREFIX}/share/carbon-blue)
        install(DIRECTORY python/bluepycore DESTINATION bin/python)
    endif ()

    install(DIRECTORY include DESTINATION . FILES_MATCHING PATTERN "*.h")

endif()
