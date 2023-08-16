find_package(GTest REQUIRED)
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
)

# Unfortunately the Python runtime is required for test discovery to work. It's not possible to override the working
# directory for discovery itself - the `WORKING_DIRECTORY` attribute there is for discovered tests. Therefore, we need
# to place it in the output folder.
get_target_property(LIBPYTHON Python IMPORTED_LOCATION)
add_custom_command(TARGET BlueCppUnitTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${LIBPYTHON} $<TARGET_FILE_DIR:BlueCppUnitTests>
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Blue> $<TARGET_FILE_DIR:BlueCppUnitTests>
        COMMAND_EXPAND_LISTS
        VERBATIM
)

# Test discovery needs to be delayed until we are about to run tests as otherwise the above post-build step won't have
# run. Additionally, we set the working directory for the tests to be the folder in which all built-in Python C
# extensions are located. That way we can avoid copying them around.
cmake_path(GET LIBPYTHON ROOT_PATH PYTHON_BIN_FOLDER)
gtest_discover_tests(BlueCppUnitTests
        DISCOVERY_MODE PRE_TEST
        WORKING_DIRECTORY ${PYTHON_BIN_FOLDER}
)
