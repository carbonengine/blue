# Copyright © 2023 CCP ehf.

find_package(GTest REQUIRED)

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
        "StdAfx.h"
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

if(CMAKE_IMPORT_LIBRARY_SUFFIX)
    add_custom_command(TARGET BlueCppUnitTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:BlueCppUnitTests> $<TARGET_FILE_DIR:BlueCppUnitTests>
            COMMAND_EXPAND_LISTS
            VERBATIM
    )
endif()

gtest_discover_tests(BlueCppUnitTests
        PROPERTIES
            # This is relying on undocumented CMake behavior in order to pass multiple environment variables to the tests.
            # (The ENVIRONMENT keyword can be used multiple times to specify more than one variable)
            ENVIRONMENT "RES_PATH=${CMAKE_CURRENT_SOURCE_DIR}/res"
            # Pass required information in order for test executable to be able to initialize a python interpreter and
            # import dependencies installed through vcpkg.
            ENVIRONMENT "PYTHON_STDLIB=${Python3_STDLIB}"
            ENVIRONMENT "IMPORT_PATH=${VCPKG_IMPORT_PREFIX}"
            ENVIRONMENT "BUILDFLAVOR=$<LOWER_CASE:$<CONFIG>>"
        WORKING_DIRECTORY $<TARGET_FILE_DIR:BlueCppUnitTests>
)
