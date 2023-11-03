find_package(Python REQUIRED NO_CMAKE_PATH)
find_package(OpenSSL REQUIRED NO_CMAKE_PATH)

ccp_add_library(_ssl SHARED
    _ssl.c
)
# including in `slsocketmodule.h` appears to drag in the C++ PCH file, so let's explicitly skip it.
set_source_files_properties(
        _ssl.c
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
target_link_libraries(_ssl
    PRIVATE
        OpenSSL::Crypto
        OpenSSL::SSL
        Blue
)
target_include_directories(_ssl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    install(
        TARGETS _ssl
        CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
        ARCHIVE DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
        LIBRARY DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
        RUNTIME DESTINATION bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
    )
endif()
