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

if(WIN32)
    get_target_property(Python_DLL_PATH Python IMPORTED_LOCATION)
    cmake_path(GET Python_DLL_PATH PARENT_PATH Python_DLL_PATH)

    file(GLOB PYTHON_RUNTIME_FILES "${Python_DLL_PATH}/*.pyd")
    list(APPEND PYTHON_RUNTIME_FILES "${Python_DLL_PATH}/Python38.zip")
    add_custom_command(TARGET BlueCppUnitTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:BlueCppUnitTests> $<TARGET_FILE_DIR:BlueCppUnitTests>
        COMMAND ${CMAKE_COMMAND} -E copy ${PYTHON_RUNTIME_FILES} $<TARGET_FILE_DIR:BlueCppUnitTests>
        COMMAND_EXPAND_LISTS
    )
endif()

gtest_discover_tests(BlueCppUnitTests)
