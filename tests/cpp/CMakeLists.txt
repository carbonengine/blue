find_package(GTest REQUIRED)
find_package(Crashpad REQUIRED NO_CMAKE_PATH)
add_executable(BlueCppUnitTests
    main.cpp
    TestPyMemory.cpp
    ../../src/PyMemory.cpp
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
    Python
    Scheduler
)

# Setting up the test environment here is a little bit convoluted because Python and C++ tests expect binaries in
# different locations. Also, manual copying of carbon-io is necessary because it is not linked against, and thus
# it isn't included in `TARGET_RUNTIME_DLLS`.
# Additionally, this copies exefile into the expected location as well, so that it can be used for
# running the Python test suite from a folder with all required libraries.
get_target_property(CARBONIO_DLL carbonio IMPORTED_LOCATION_RELEASE)
if(CMAKE_IMPORT_LIBRARY_SUFFIX)
    add_custom_command(TARGET BlueCppUnitTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:BlueCppUnitTests> $<TARGET_FILE_DIR:BlueCppUnitTests>
            COMMAND ${CMAKE_COMMAND} -E copy ${CARBONIO_DLL} $<TARGET_FILE_DIR:BlueCppUnitTests>
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:BlueCppUnitTests> $<TARGET_FILE_DIR:Blue>
            COMMAND ${CMAKE_COMMAND} -E copy ${CARBONIO_DLL} $<TARGET_FILE_DIR:Blue>
            COMMAND ${CMAKE_COMMAND} -E copy ${Exefile_ROOT}/${CCP_VENDOR_BIN_PATH}/${EXEFILE_NAME} $<TARGET_FILE_DIR:Blue>
            COMMAND_EXPAND_LISTS
            VERBATIM
    )
endif()
if(APPLE)
    add_custom_command(TARGET BlueCppUnitTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${Exefile_ROOT}/${CCP_VENDOR_BIN_PATH}${EXEFILE_NAME} $<TARGET_FILE_DIR:Blue>
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_LINKER_FILE:ccp::log> $<TARGET_FILE_DIR:Blue>
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_LINKER_FILE:Python> $<TARGET_FILE_DIR:Blue>
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_LINKER_FILE:Python> $<TARGET_FILE_DIR:BlueCppUnitTests>
            COMMAND ${CMAKE_COMMAND} -E copy ${CRASHPAD_EXECUTABLE} $<TARGET_FILE_DIR:Blue>
            COMMAND_EXPAND_LISTS
            VERBATIM
    )
endif()

# C++ test discovery needs to be delayed until tests are about to be run because they need the above post-build step.
gtest_discover_tests(BlueCppUnitTests
        DISCOVERY_MODE PRE_TEST
)
