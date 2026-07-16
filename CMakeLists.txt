cmake_minimum_required(VERSION 3.16)
project(aaa VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(aaa main.c)

if(WIN32)
    target_compile_definitions(aaa PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

include(GNUInstallDirs)
install(TARGETS aaa RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md README.zh-CN.md LICENSE
        DESTINATION ${CMAKE_INSTALL_DOCDIR})

include(CTest)
if(BUILD_TESTING)
    add_test(NAME aaa_help COMMAND aaa help)
endif()
