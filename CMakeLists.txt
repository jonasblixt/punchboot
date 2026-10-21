# Host build of punchboot, used for testing.
#
# This builds punchboot as a native Linux process (src/plat/host,
# src/board/host), plus C unit tests (tests/unit), and registers the unit
# and integration tests (tests/integration) with CTest. The firmware for
# real targets is still built with the top level Makefile.
#
#   cmake -S . -B build-host
#   cmake --build build-host
#   ctest --test-dir build-host --output-on-failure
#
# Options:
#   -DPB_SANITIZE=ON   Build with AddressSanitizer and UBSan
#   -DPB_COVERAGE=ON   Build with gcov instrumentation

cmake_minimum_required(VERSION 3.20)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/version.txt PB_VERSION)
string(STRIP ${PB_VERSION} PB_VERSION)

project(punchboot-host VERSION ${PB_VERSION} LANGUAGES C)

option(PB_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(PB_COVERAGE "Build with gcov instrumentation" OFF)

set(PB_DEFCONFIG ${CMAKE_CURRENT_SOURCE_DIR}/configs/host_defconfig CACHE FILEPATH
    "Kconfig defconfig for the host build")
set(MBEDTLS_DIR $ENV{MBEDTLS_DIR} CACHE PATH "Mbed TLS source directory")

if(NOT EXISTS ${MBEDTLS_DIR}/library/ecdsa.c)
    message(FATAL_ERROR "MBEDTLS_DIR (${MBEDTLS_DIR}) does not point to Mbed TLS sources")
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(BPAK bpak REQUIRED)

# Kconfig: generate config.h from the host defconfig at configure time
file(GLOB_RECURSE PB_KCONFIG_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*Kconfig)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${PB_DEFCONFIG} ${PB_KCONFIG_FILES})

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env KCONFIG_CONFIG=${PB_DEFCONFIG}
            ${Python3_EXECUTABLE} scripts/genconfig.py
            --header-path ${CMAKE_CURRENT_BINARY_DIR}/config.h
            --config-out ${CMAKE_CURRENT_BINARY_DIR}/.config
            src/Kconfig
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND_ERROR_IS_FATAL ANY
)

# Common compile settings for everything built from the punchboot tree
add_library(pb_host_flags INTERFACE)

target_compile_options(pb_host_flags INTERFACE
    -std=gnu99
    -g
    -Wall
    -Wextra
    -Wshadow
    -Wvla
    -include ${CMAKE_CURRENT_SOURCE_DIR}/src/plat/host/compat.h
)

target_compile_definitions(pb_host_flags INTERFACE
    PB_VERSION="${PB_VERSION}"
    LOGLEVEL=3
    TIMING_REPORT=0
    MBEDTLS_CONFIG_FILE="${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/crypto/mbedtls/mbedtls_config.h"
)

target_include_directories(pb_host_flags INTERFACE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/fdt
    ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/host/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plat/host/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/board/host/include
    ${MBEDTLS_DIR}/include
)

if(PB_SANITIZE)
    target_compile_options(pb_host_flags INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
    target_link_options(pb_host_flags INTERFACE -fsanitize=address,undefined)
endif()

if(PB_COVERAGE)
    target_compile_options(pb_host_flags INTERFACE --coverage)
    target_link_options(pb_host_flags INTERFACE --coverage)
endif()

# Mbed TLS, built from the same sources and config as the firmware
set(PB_MBEDTLS_SOURCES
    asn1parse.c asn1write.c bignum.c bignum_core.c constant_time.c ecdsa.c
    ecp.c ecp_curves.c error.c gcm.c hash_info.c md.c md5.c
    memory_buffer_alloc.c oid.c pk.c pk_wrap.c pkparse.c pkwrite.c
    platform.c platform_util.c sha256.c sha512.c
)
list(TRANSFORM PB_MBEDTLS_SOURCES PREPEND ${MBEDTLS_DIR}/library/)

add_library(pb_mbedtls STATIC ${PB_MBEDTLS_SOURCES})
target_link_libraries(pb_mbedtls PRIVATE pb_host_flags)
target_include_directories(pb_mbedtls PUBLIC ${MBEDTLS_DIR}/include)
target_compile_options(pb_mbedtls PRIVATE -w)

# Libraries shared by the firmware core and the unit tests
file(GLOB PB_FDT_SOURCES src/lib/fdt/*.c)
file(GLOB PB_UUID_SOURCES src/lib/uuid/*.c)

add_library(pb_lib STATIC
    src/lib/bpak.c
    src/lib/crc.c
    src/lib/der_helpers.c
    ${PB_FDT_SOURCES}
    ${PB_UUID_SOURCES}
)
target_link_libraries(pb_lib PUBLIC pb_host_flags)

# Keystore, generated from the bpak keystore like in the firmware build
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/keystore.c
    COMMAND ${BPAK} generate keystore --name pb
            ${CMAKE_CURRENT_SOURCE_DIR}/pki/internal_keystore.bpak --decorate
            > ${CMAKE_CURRENT_BINARY_DIR}/keystore.c
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/pki/internal_keystore.bpak
    VERBATIM
)

# The firmware core: everything that is not arch, platform or board specific
add_library(pb_core STATIC
    src/main.c
    src/bio.c
    src/crypto.c
    src/delay.c
    src/device_uuid.c
    src/rot.c
    src/slc.c
    src/wire.c
    src/boot/ab_state.c
    src/boot/boot.c
    src/boot/image_helpers.c
    src/boot/linux.c
    src/cm/cm_main.c
    src/drivers/crypto/mbedtls/mbedtls_pb.c
    src/drivers/fuse/test_fuse_bio.c
    src/drivers/partition/gpt.c
    ${CMAKE_CURRENT_BINARY_DIR}/keystore.c
)
target_link_libraries(pb_core PUBLIC pb_lib pb_mbedtls)
# The host platform provides the process entry point and calls pb_main
set_source_files_properties(src/main.c PROPERTIES COMPILE_DEFINITIONS main=pb_main)

# punchboot as a native process
add_executable(pb-host
    src/arch/host/arch.c
    src/plat/host/disk.c
    src/plat/host/main.c
    src/plat/host/plat.c
    src/plat/host/socket.c
    src/board/host/board.c
)
target_link_libraries(pb-host PRIVATE pb_core)

# The image loader refuses to overwrite punchboot itself, which it finds
# through linker script symbols on target. Place a fake punchboot image at the
# start of the emulated RAM. -no-pie keeps the absolute symbols absolute.
target_link_options(pb-host PRIVATE
    -no-pie
    -Wl,--defsym=_code_start=0x40000000
    -Wl,--defsym=_code_end=0x40080000
    -Wl,--defsym=_ro_data_region_start=0x40080000
    -Wl,--defsym=_ro_data_region_end=0x400a0000
    -Wl,--defsym=_data_region_start=0x400a0000
    -Wl,--defsym=_data_region_end=0x400c0000
    -Wl,--defsym=_zero_region_start=0x400c0000
    -Wl,--defsym=_zero_region_end=0x400e0000
    -Wl,--defsym=_stack_start=0x400e0000
    -Wl,--defsym=_stack_end=0x40100000
    -Wl,--defsym=_no_init_start=0x40100000
    -Wl,--defsym=_no_init_end=0x40120000
)

enable_testing()
add_subdirectory(tests/unit)
add_subdirectory(tests/integration)
