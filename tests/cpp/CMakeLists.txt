find_package(GTest REQUIRED)
find_package(CURL REQUIRED NO_CMAKE_PATH)
find_package(Crashpad REQUIRED NO_CMAKE_PATH)
find_package(libuv REQUIRED NO_CMAKE_PATH)
find_package(Tracy CONFIG REQUIRED NO_CMAKE_PATH)

find_package(Greenlet REQUIRED NO_CMAKE_PATH)

find_package(Python3 COMPONENTS Development REQUIRED)

add_executable(BlueCppUnitTests
    main.cpp
    TestPyMemory.cpp
    ../../src/PyMemory.cpp
    TestBlueResMan.cpp
)
target_precompile_headers(BlueCppUnitTests
    PRIVATE
        <Python.h>
        <gtest/gtest.h>
)
target_include_directories(BlueCppUnitTests
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../src
)
target_link_libraries(BlueCppUnitTests
    Blue
    GTest::GTest
    Python3::Python
)

# Setting up the test environment here is a little bit convoluted because Python and C++ tests expect binaries in
# different locations. This copies exefile into the expected location, so that it can be used for
# running the Python test suite from a folder with all required libraries.
add_custom_command(TARGET BlueCppUnitTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Scheduler> $<TARGET_FILE_DIR:BlueCppUnitTests>
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Greenlet> $<TARGET_FILE_DIR:BlueCppUnitTests>
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Python3::Python> $<TARGET_FILE_DIR:BlueCppUnitTests>
)
if(CMAKE_IMPORT_LIBRARY_SUFFIX)
    add_custom_command(TARGET BlueCppUnitTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:BlueCppUnitTests> $<TARGET_FILE_DIR:BlueCppUnitTests>
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:BlueCppUnitTests> $<TARGET_FILE_DIR:Blue>
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libuv::uv> $<TARGET_FILE_DIR:Blue>  # Required to import _socket module.
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libuv::uv> $<TARGET_FILE_DIR:BlueCppUnitTests>  # Required to import _socket module.
            COMMAND ${CMAKE_COMMAND} -E copy ${Exefile_ROOT}/${CCP_VENDOR_BIN_PATH}/${EXEFILE_NAME} $<TARGET_FILE_DIR:Blue>
            COMMAND ${CMAKE_COMMAND} -E copy ${CcpCore_DIR}/${CCP_VENDOR_BIN_PATH}/$<TARGET_PROPERTY:CcpCore,NAME>.dll $<TARGET_FILE_DIR:Blue>
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:CcpCore>/CcpCore.dll $<TARGET_FILE_DIR:BlueCppUnitTests>          # Needed as runtime dependency for _carbonsocket
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:carbonio>/_carbonsocket.pyd $<TARGET_FILE_DIR:BlueCppUnitTests>
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:carbonio>/_carbonssl.pyd $<TARGET_FILE_DIR:BlueCppUnitTests>
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:carbonio>/carbonselect.pyd $<TARGET_FILE_DIR:BlueCppUnitTests>
            COMMAND_EXPAND_LISTS
            VERBATIM
    )
endif()
if(APPLE)
    add_custom_command(TARGET BlueCppUnitTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${Exefile_ROOT}/${CCP_VENDOR_BIN_PATH}${EXEFILE_NAME} $<TARGET_FILE_DIR:Blue>
            COMMAND ${CMAKE_COMMAND} -E copy ${CcpCore_DIR}/${CCP_VENDOR_LIB_PATH}/$<TARGET_PROPERTY:CcpCore,NAME>.so $<TARGET_FILE_DIR:Blue>
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_LINKER_FILE:Python> $<TARGET_FILE_DIR:Blue>
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_LINKER_FILE:Python> $<TARGET_FILE_DIR:BlueCppUnitTests>
            COMMAND ${CMAKE_COMMAND} -E copy ${CRASHPAD_EXECUTABLE} $<TARGET_FILE_DIR:Blue>
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libuv::uv> $<TARGET_FILE_DIR:Blue>/libuv.1.dylib  # Required to import _socket module.
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libuv::uv> $<TARGET_FILE_DIR:BlueCppUnitTests>/libuv.1.dylib  # Required to import _socket module.
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Tracy::TracyClient> $<TARGET_FILE_DIR:Blue>/libTracyClient.0.11.1.dylib  # CcpCore runtime dependency.
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:CcpCore>/CcpCore.so $<TARGET_FILE_DIR:BlueCppUnitTests>              # Needed as runtime dependency for _carbonsocket
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:carbonio>/_carbonsocket.so $<TARGET_FILE_DIR:BlueCppUnitTests>
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:carbonio>/_carbonssl.so $<TARGET_FILE_DIR:BlueCppUnitTests>
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE_DIR:carbonio>/carbonselect.so $<TARGET_FILE_DIR:BlueCppUnitTests>
            COMMAND_EXPAND_LISTS
            VERBATIM
    )
endif()

# Provide backwards compatibility for running tests on macOS ONLY WHEN CcpLog_DIR is defined (meaning logging is done via carbon-log, not carbon-core)
if (DEFINED CcpLog_DIR)
    find_package(CcpLog REQUIRED NO_CMAKE_PATH)
    add_custom_command(TARGET BlueCppUnitTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ccp::log> $<TARGET_FILE_DIR:BlueCppUnitTests>
    )
    # Extra case for macOS
    if(APPLE)
        add_custom_command(TARGET BlueCppUnitTests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_LINKER_FILE:ccp::log> $<TARGET_FILE_DIR:Blue>
                COMMAND_EXPAND_LISTS
                VERBATIM
        )
    endif()
endif ()


# C++ test discovery needs to be delayed until tests are about to be run because they need the above post-build step.
gtest_discover_tests(BlueCppUnitTests
        DISCOVERY_MODE PRE_TEST
        PROPERTIES
            # This is relying on undocumented CMake behavior in order to pass multiple environment variables to the tests.
            # (The ENVIRONMENT keyword can be used multiple times to specify more than one variable)
            ENVIRONMENT "CCP_EVE_PERFORCE_BRANCH_PATH=$ENV{CCP_EVE_PERFORCE_BRANCH_PATH}"
            ENVIRONMENT "RES_PATH=${CMAKE_CURRENT_SOURCE_DIR}/res"
        WORKING_DIRECTORY $<TARGET_FILE_DIR:BlueCppUnitTests>
)
