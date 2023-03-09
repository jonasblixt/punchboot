cmake_minimum_required(VERSION 3.10)

# Extract version info from pb-tool.h
file(READ "include/pb-tools/pb-tools.h" ver)

string(REGEX MATCH ".+PB_TOOLS_VERSION_MAJOR ([0-9]*)" _ ${ver})
set(ver_major ${CMAKE_MATCH_1})

string(REGEX MATCH ".+PB_TOOLS_VERSION_MINOR ([0-9]*)" _ ${ver})
set(ver_minor ${CMAKE_MATCH_1})

string(REGEX MATCH ".+PB_TOOLS_VERSION_PATCH ([0-9]*)" _ ${ver})
set(ver_patch ${CMAKE_MATCH_1})

message(STATUS "Version: ${ver_major}.${ver_minor}.${ver_patch}")

project(punchboot
    VERSION ${ver_major}.${ver_minor}.${ver_patch}
    DESCRIPTION "Punchboot tools"
    HOMEPAGE_URL https://github.com/jonasblixt/punchboot-tools
)

enable_language(C)
enable_testing()
set(CMAKE_C_STANDARD 99)

add_compile_options(-Wall -Werror)
include_directories(include/)

option(PB_TOOLS_BUILD_TOOL "Build the tool" ON)
option(PB_TOOLS_BUILD_SOCKET "Support testing targets through domain sockets" OFF)
option(PB_TOOLS_BUILD_TESTS "Build test cases" OFF)
option(PB_TOOLS_BUILD_STATIC "Build a static versoin of the tool" OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
pkg_check_modules(BPAK REQUIRED bpak)

# @ONLY "Restrict variable replacement to references of the form @VAR@"
configure_file(dist/punchboot-tools-x64.iss.in punchboot-tools-x64.iss @ONLY)

if (PB_TOOLS_BUILD_STATIC)
    add_compile_options(-static)
endif()

if (PB_TOOLS_BUILD_TESTS)
    set(PB_TOOLS_BUILD_SOCKET ON)
    add_subdirectory("python")
    add_subdirectory("test")
    add_compile_options(-g -fprofile-arcs -ftest-coverage)
    link_libraries(gcov)
endif()

add_subdirectory("lib")

if (PB_TOOLS_BUILD_TOOL)
    add_subdirectory("src")
endif()
