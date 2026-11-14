# C unit tests (CMocka). Each test links the modules under test, the shared
# libraries and the fakes it needs.

find_package(PkgConfig REQUIRED)
pkg_check_modules(CMOCKA REQUIRED IMPORTED_TARGET cmocka)

add_library(pb_unit_fakes STATIC
    fakes/assert.c
    fakes/plat.c
    ${PROJECT_SOURCE_DIR}/src/log.c
    fakes/ramdisk.c
)
target_link_libraries(pb_unit_fakes PUBLIC pb_lib PRIVATE PkgConfig::CMOCKA)

# pb_unit_test(name SOURCES <modules under test> LIBS <extra libraries>)
function(pb_unit_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    add_executable(${name} ${name}.c ${ARG_SOURCES})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE ${ARG_LIBS} pb_unit_fakes PkgConfig::CMOCKA)
    add_test(NAME unit.${name} COMMAND ${name})
    # The unit tests take milliseconds, a hang should not block CI
    set_tests_properties(unit.${name} PROPERTIES LABELS unit TIMEOUT 60)
endfunction()

# The DMA and cache rule (drivers.md 4): pb/dma.h on a model of the D-cache,
# which is the arch's cache functions for the tests of the drivers with DMA
set(PB_DMA_MODEL
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/dcache.c
    ${PROJECT_SOURCE_DIR}/src/dma.c
)

pb_unit_test(test_dma SOURCES ${PB_DMA_MODEL})

pb_unit_test(test_bio SOURCES
    ${PROJECT_SOURCE_DIR}/src/bio.c
)

# The block device conformance tests (bio.md 8), linked by every test program
# that has a device to run them against
set(PB_BIO_CONFORMANCE ${CMAKE_CURRENT_SOURCE_DIR}/conformance/bio_conformance.c)

# Against the RAM disk and the host platform's disk and flash
pb_unit_test(test_bio_conformance SOURCES
    ${PB_BIO_CONFORMANCE}
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/plat/host/disk.c
    ${PROJECT_SOURCE_DIR}/src/plat/host/nor.c
)

pb_unit_test(test_gpt SOURCES
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/drivers/partition/gpt.c
)

pb_unit_test(test_static_ptbl SOURCES
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/drivers/partition/static.c
)

pb_unit_test(test_bpak)

pb_unit_test(test_delay SOURCES
    ${PROJECT_SOURCE_DIR}/src/delay.c
)

pb_unit_test(test_wire SOURCES
    ${PROJECT_SOURCE_DIR}/src/wire.c
)

# With the internal keystore, whose keys the parser must take
pb_unit_test(test_der LIBS pb_keystore)

pb_unit_test(test_tzc380 SOURCES
    ${PROJECT_SOURCE_DIR}/src/drivers/tzc/tzc380.c
)

# test_ramlog.c includes src/ramlog.c, to read the log's header. CONFIG_RAMLOG
# and the log size come from configs/host_defconfig
pb_unit_test(test_ramlog SOURCES)

pb_unit_test(test_console SOURCES
    ${PROJECT_SOURCE_DIR}/src/console.c
    ${PROJECT_SOURCE_DIR}/src/ramlog.c
)
# fopencookie(), to send printf() through the console
target_compile_definitions(test_console PRIVATE _GNU_SOURCE)

# The test's fake ROM log pointer is 32 bits: -no-pie keeps the log it
# points to at a low address
pb_unit_test(test_imx_rom_log SOURCES
    ${PROJECT_SOURCE_DIR}/src/drivers/rom_log/imx_rom_log.c
    ${PROJECT_SOURCE_DIR}/src/log_source.c
    ${PROJECT_SOURCE_DIR}/src/ramlog.c
)
target_compile_options(test_imx_rom_log PRIVATE -fno-pie)
target_link_options(test_imx_rom_log PRIVATE -no-pie)

pb_unit_test(test_imx_hab_log SOURCES
    ${PROJECT_SOURCE_DIR}/src/drivers/rom_log/imx_hab_log.c
    ${PROJECT_SOURCE_DIR}/src/log_source.c
    ${PROJECT_SOURCE_DIR}/src/ramlog.c
)

pb_unit_test(test_linux_ramoops SOURCES
    ${PROJECT_SOURCE_DIR}/src/boot/linux_ramoops.c
    ${PROJECT_SOURCE_DIR}/src/log_source.c
    ${PROJECT_SOURCE_DIR}/src/ramlog.c
)

pb_unit_test(test_crypto
    SOURCES
        ${PROJECT_SOURCE_DIR}/src/crypto.c
        ${PROJECT_SOURCE_DIR}/src/drivers/crypto/mbedtls/mbedtls_pb.c
    LIBS pb_mbedtls
)

# The crypto backends' conformance tests (crypto.md 9), linked by every test
# program that has a backend to run them against
set(PB_CRYPTO_CONFORMANCE ${CMAKE_CURRENT_SOURCE_DIR}/conformance/crypto_conformance.c)

pb_unit_test(test_mbedtls
    SOURCES
        ${PB_CRYPTO_CONFORMANCE}
        ${PROJECT_SOURCE_DIR}/src/crypto.c
        ${PROJECT_SOURCE_DIR}/src/drivers/crypto/mbedtls/mbedtls_pb.c
    LIBS pb_mbedtls pb_keystore
)
target_compile_definitions(test_mbedtls PRIVATE
    PB_AUTH_TOKEN_PATH="${PROJECT_SOURCE_DIR}/tests/02e49231-756e-35ee-a982-378e5ba866a9.token"
)
# The library's hash calls go through the test, which can fail them
foreach(call md5_starts md5_update md5_finish sha256_starts sha256_update sha256_finish
             sha512_starts sha512_update sha512_finish)
    target_link_options(test_mbedtls PRIVATE -Wl,--wrap=mbedtls_${call})
endforeach()

pb_unit_test(test_rot
    SOURCES
        ${PROJECT_SOURCE_DIR}/src/rot.c
        ${PROJECT_SOURCE_DIR}/src/slc.c
    LIBS pb_keystore
)

# EOL is the product's option, CONFIG_SLC_EOL (system.md 4, F-263): off by
# default and on in configs/host_defconfig, for the tests. The build without
# it is the host's configuration less that line, first on the include path
file(READ ${PROJECT_BINARY_DIR}/config.h PB_CONFIG_H)
string(REPLACE "#define CONFIG_SLC_EOL 1\n" "" PB_CONFIG_H_NO_SLC_EOL "${PB_CONFIG_H}")
if(PB_CONFIG_H_NO_SLC_EOL STREQUAL PB_CONFIG_H)
    message(FATAL_ERROR "configs/host_defconfig: CONFIG_SLC_EOL is not set, the EOL tests need it")
endif()
set(PB_NO_SLC_EOL_DIR ${CMAKE_CURRENT_BINARY_DIR}/no_slc_eol)
file(CONFIGURE OUTPUT ${PB_NO_SLC_EOL_DIR}/config.h CONTENT "${PB_CONFIG_H_NO_SLC_EOL}" @ONLY)

# pb_no_slc_eol_test(name <test file> <modules under test>...)
function(pb_no_slc_eol_test name)
    add_executable(${name} ${ARGN})
    target_include_directories(${name} BEFORE PRIVATE ${PB_NO_SLC_EOL_DIR})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE pb_unit_fakes PkgConfig::CMOCKA)
    add_test(NAME unit.${name} COMMAND ${name})
    set_tests_properties(unit.${name} PROPERTIES LABELS unit TIMEOUT 60)
endfunction()

# slc.c without the option: the device can't enter EOL
pb_no_slc_eol_test(test_slc_no_eol test_slc_no_eol.c
    ${PROJECT_SOURCE_DIR}/src/slc.c
)

pb_unit_test(test_imx_iomuxc SOURCES
    ${PROJECT_SOURCE_DIR}/src/drivers/pinctrl/imx_iomuxc.c
)
target_include_directories(test_imx_iomuxc BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/mmio
)

# With a keystore of its own, with an empty slot
pb_unit_test(test_rot_slots SOURCES ${PROJECT_SOURCE_DIR}/src/rot.c)

pb_unit_test(test_device_uuid
    SOURCES
        ${PROJECT_SOURCE_DIR}/src/device_uuid.c
        ${PROJECT_SOURCE_DIR}/src/crypto.c
        ${PROJECT_SOURCE_DIR}/src/drivers/crypto/mbedtls/mbedtls_pb.c
    LIBS pb_mbedtls
)

pb_unit_test(test_fuse_bio SOURCES
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/drivers/fuse/test_fuse_bio.c
)

pb_unit_test(test_ab_state SOURCES
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/boot/ab_state.c
)

# A/B's speculative mode (boot.md 3.6) isn't in pb-host: host_defconfig has no
# watchdog, which CONFIG_BOOT_AB_SPECULATIVE depends on (pb-host-speculative,
# from host_speculative_defconfig, has the host's watchdog fake and the mode).
# Its test is the same file and module built with the symbol, which changes
# nothing else in the build; what A/B does with a viable candidate runs in both
add_executable(test_ab_state_speculative test_ab_state.c
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/boot/ab_state.c
)
target_compile_definitions(test_ab_state_speculative PRIVATE CONFIG_BOOT_AB_SPECULATIVE=1)
target_include_directories(test_ab_state_speculative PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_ab_state_speculative PRIVATE pb_unit_fakes PkgConfig::CMOCKA)
add_test(NAME unit.test_ab_state_speculative COMMAND test_ab_state_speculative)
set_tests_properties(unit.test_ab_state_speculative PROPERTIES LABELS unit TIMEOUT 60)

# The symbol's dependencies are Kconfig's (rule 6: the watchdog), so their
# test reads Kconfig
add_test(
    NAME unit.ab_speculative_kconfig
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_ab_speculative_kconfig.py
            ${PROJECT_SOURCE_DIR}
)
set_tests_properties(unit.ab_speculative_kconfig PROPERTIES LABELS unit TIMEOUT 60)

# libpbstate, the OS's side of the A/B state (boot.md 3.7), on files
pb_unit_test(test_pbstate SOURCES
    ${PROJECT_SOURCE_DIR}/tools/pbstate/src/pbstate.c
)
target_include_directories(test_pbstate PRIVATE ${PROJECT_SOURCE_DIR}/tools/pbstate/src)

pb_unit_test(test_policy SOURCES
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/boot/policy.c
    ${PROJECT_SOURCE_DIR}/src/boot/policy_chain.c
    ${PROJECT_SOURCE_DIR}/src/boot/policy_fixed.c
)

pb_unit_test(test_board SOURCES
    ${PROJECT_SOURCE_DIR}/src/board.c
    ${PROJECT_SOURCE_DIR}/src/board/password.c
    ${PROJECT_SOURCE_DIR}/src/boot/policy.c
)

# The same with the password method, which pb-host is built without: the
# board's password_auth is then mandatory (board.md 3)
add_executable(test_board_password test_board.c
    ${PROJECT_SOURCE_DIR}/src/board.c
    ${PROJECT_SOURCE_DIR}/src/board/password.c
    ${PROJECT_SOURCE_DIR}/src/boot/policy.c
)
target_compile_definitions(test_board_password PRIVATE CONFIG_CM_AUTH_PASSWORD=1)
target_include_directories(test_board_password PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_board_password PRIVATE pb_unit_fakes PkgConfig::CMOCKA)
add_test(NAME unit.test_board_password COMMAND test_board_password)
set_tests_properties(unit.test_board_password PROPERTIES LABELS unit TIMEOUT 60)

pb_unit_test(test_boot SOURCES
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/boot/boot.c
    ${PROJECT_SOURCE_DIR}/src/boot/policy.c
    ${PROJECT_SOURCE_DIR}/src/drivers/partition/static.c
)

# The same with a watchdog, which pb-host is built without. The boot's
# preconditions check it, and the crypto engine of a platform that sets one
# up, which the host is (platform.md 2.3)
add_executable(test_boot_preconditions test_boot.c
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/boot/boot.c
    ${PROJECT_SOURCE_DIR}/src/boot/policy.c
    ${PROJECT_SOURCE_DIR}/src/drivers/partition/static.c
)
target_compile_definitions(test_boot_preconditions PRIVATE
    CONFIG_ENABLE_WATCHDOG=1
    CONFIG_PLAT_CRYPTO_ENGINE=1
)
target_include_directories(test_boot_preconditions PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_boot_preconditions PRIVATE pb_unit_fakes PkgConfig::CMOCKA)
add_test(NAME unit.test_boot_preconditions COMMAND test_boot_preconditions)
set_tests_properties(unit.test_boot_preconditions PROPERTIES LABELS unit TIMEOUT 60)

# And for a platform that sets no crypto engine up, which no platform in the
# tree is since F-247: the core doesn't ask for one (platform.md 2.3). The
# build is the host's configuration less CONFIG_PLAT_CRYPTO_ENGINE
string(REPLACE "#define CONFIG_PLAT_CRYPTO_ENGINE 1\n" ""
    PB_CONFIG_H_NO_CRYPTO_ENGINE "${PB_CONFIG_H}")
if(PB_CONFIG_H_NO_CRYPTO_ENGINE STREQUAL PB_CONFIG_H)
    message(FATAL_ERROR "The host platform doesn't select PLAT_CRYPTO_ENGINE")
endif()
set(PB_NO_CRYPTO_ENGINE_DIR ${CMAKE_CURRENT_BINARY_DIR}/no_crypto_engine)
file(CONFIGURE OUTPUT ${PB_NO_CRYPTO_ENGINE_DIR}/config.h
    CONTENT "${PB_CONFIG_H_NO_CRYPTO_ENGINE}" @ONLY)
add_executable(test_boot_no_crypto_engine test_boot.c
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/boot/boot.c
    ${PROJECT_SOURCE_DIR}/src/boot/policy.c
    ${PROJECT_SOURCE_DIR}/src/drivers/partition/static.c
)
target_include_directories(test_boot_no_crypto_engine BEFORE PRIVATE ${PB_NO_CRYPTO_ENGINE_DIR})
target_include_directories(test_boot_no_crypto_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_boot_no_crypto_engine PRIVATE pb_unit_fakes PkgConfig::CMOCKA)
add_test(NAME unit.test_boot_no_crypto_engine COMMAND test_boot_no_crypto_engine)
set_tests_properties(unit.test_boot_no_crypto_engine PROPERTIES LABELS unit TIMEOUT 60)

pb_unit_test(test_image_helpers SOURCES
    ${PROJECT_SOURCE_DIR}/src/boot/image_helpers.c
    ${PROJECT_SOURCE_DIR}/src/crypto.c
)
# 'pb' and 'buffers', which no image data region may overlap, same as for
# pb-host
set(PB_TEST_MEMORY
    -no-pie
    -Wl,--defsym=_pb_start=0x40000000
    -Wl,--defsym=_pb_end=0x40100000
    -Wl,--defsym=_buffers_start=0x40100000
    -Wl,--defsym=_buffers_end=0x40120000
)
target_link_options(test_image_helpers PRIVATE ${PB_TEST_MEMORY})

# The same with what a handover writes outside the parts, which pb-host has
# none of: the Firmware Handoff region (boot.md 5.4) and a platform's region
# for bl_params (platform.md 5.3), both between the suite's two image data
# regions
add_executable(test_image_helpers_handover test_image_helpers.c
    ${PROJECT_SOURCE_DIR}/src/boot/image_helpers.c
    ${PROJECT_SOURCE_DIR}/src/crypto.c
)
target_compile_definitions(test_image_helpers_handover PRIVATE
    CONFIG_BOOT_FH=1
    CONFIG_BOOT_FH_REGION_BASE=0x52000000
    CONFIG_BOOT_FH_REGION_SIZE=0x10000
    PLAT_BL_PARAMS_BASE=0x56000000
    PLAT_BL_PARAMS_SIZE=512
)
target_include_directories(test_image_helpers_handover PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_image_helpers_handover PRIVATE pb_unit_fakes PkgConfig::CMOCKA)
target_link_options(test_image_helpers_handover PRIVATE ${PB_TEST_MEMORY})
add_test(NAME unit.test_image_helpers_handover COMMAND test_image_helpers_handover)
set_tests_properties(unit.test_image_helpers_handover PROPERTIES LABELS unit TIMEOUT 60)

pb_unit_test(test_image SOURCES
    ${PROJECT_SOURCE_DIR}/src/boot/dtb.c
    ${PROJECT_SOURCE_DIR}/src/boot/image.c
)

# Again with AArch32's registers, which the host's configuration doesn't have
add_executable(test_image_armv7 test_image.c
    ${PROJECT_SOURCE_DIR}/src/boot/dtb.c
    ${PROJECT_SOURCE_DIR}/src/boot/image.c
)
target_compile_definitions(test_image_armv7 PRIVATE CONFIG_ARCH_ARMV7=1)
target_include_directories(test_image_armv7 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_image_armv7 PRIVATE pb_unit_fakes PkgConfig::CMOCKA)
add_test(NAME unit.test_image_armv7 COMMAND test_image_armv7)
set_tests_properties(unit.test_image_armv7 PROPERTIES LABELS unit TIMEOUT 60)

# The generic handover, Firmware Handoff (boot.md 5.4), isn't in pb-host. Its
# test gets the option and a region, which the test maps where the build has
# it. Built once per register convention: AArch64's as the host is
# configured, AArch32's (ARMv7-A) and pb's extension of it to ARMv7-M
function(pb_fh_test name)
    add_executable(${name} test_fh.c
        ${PROJECT_SOURCE_DIR}/src/boot/dtb.c
        ${PROJECT_SOURCE_DIR}/src/boot/fh.c
    )
    target_compile_definitions(${name} PRIVATE
        CONFIG_BOOT_FH=1
        CONFIG_BOOT_FH_REGION_BASE=0x52000000
        CONFIG_BOOT_FH_REGION_SIZE=0x10000
        ${ARGN}
    )
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE pb_unit_fakes PkgConfig::CMOCKA)
    add_test(NAME unit.${name} COMMAND ${name})
    set_tests_properties(unit.${name} PROPERTIES LABELS unit TIMEOUT 60)
endfunction()

pb_fh_test(test_fh)
pb_fh_test(test_fh_armv7 CONFIG_ARCH_ARMV7=1)
pb_fh_test(test_fh_armv7m CONFIG_ARCH_ARMV7M=1)

# A build whose region or version the transfer list can't have doesn't
# compile (boot.md 5.4): fh.c with the values in question, which the compiler
# refuses with the rule's message (without the source line, which has the
# message whether its rule failed or not)
function(pb_fh_refuses name message)
    add_test(NAME unit.${name}
        COMMAND ${CMAKE_C_COMPILER} -std=gnu99 -fsyntax-only -fno-diagnostics-show-caret
                -include ${PROJECT_SOURCE_DIR}/src/plat/host/compat.h
                -I${PROJECT_BINARY_DIR} -I${PROJECT_SOURCE_DIR}/src
                -I${PROJECT_SOURCE_DIR}/include -I${PROJECT_SOURCE_DIR}/include/fdt
                -I${PROJECT_SOURCE_DIR}/src/arch/host/include
                -DLOGLEVEL=3 -DCONFIG_BOOT_FH=1 ${ARGN}
                ${PROJECT_SOURCE_DIR}/src/boot/fh.c
    )
    set_tests_properties(unit.${name} PROPERTIES LABELS unit TIMEOUT 60
        PASS_REGULAR_EXPRESSION "${message}")
endfunction()

set(PB_FH_VERSION -DPB_VERSION=\"${PB_VERSION}\")
set(PB_FH_BASE -DCONFIG_BOOT_FH_REGION_BASE=0x52000000)
set(PB_FH_SIZE -DCONFIG_BOOT_FH_REGION_SIZE=0x10000)

pb_fh_refuses(fh_region_is_configured "CONFIG_BOOT_FH needs the transfer list's region"
    ${PB_FH_VERSION} ${PB_FH_BASE})
pb_fh_refuses(fh_region_base_is_not_zero "CONFIG_BOOT_FH_REGION_BASE is 0 or not 8-byte aligned"
    ${PB_FH_VERSION} -DCONFIG_BOOT_FH_REGION_BASE=0x0 ${PB_FH_SIZE})
pb_fh_refuses(fh_region_base_is_8_byte_aligned
    "CONFIG_BOOT_FH_REGION_BASE is 0 or not 8-byte aligned"
    ${PB_FH_VERSION} -DCONFIG_BOOT_FH_REGION_BASE=0x52000004 ${PB_FH_SIZE})
pb_fh_refuses(fh_region_size_is_a_multiple_of_8
    "CONFIG_BOOT_FH_REGION_SIZE is not a multiple of 8"
    ${PB_FH_VERSION} ${PB_FH_BASE} -DCONFIG_BOOT_FH_REGION_SIZE=0x10004)
# 192 bytes hold pb's structure, 184 don't
pb_fh_refuses(fh_region_holds_pbs_structure
    "CONFIG_BOOT_FH_REGION_SIZE has no room for struct pb_fh_handover"
    ${PB_FH_VERSION} ${PB_FH_BASE} -DCONFIG_BOOT_FH_REGION_SIZE=0xb8)
pb_fh_refuses(fh_region_does_not_wrap
    "The Firmware Handoff region wraps or is above what the arch can address"
    ${PB_FH_VERSION} -DCONFIG_BOOT_FH_REGION_BASE=0xfffffffffffffff8 ${PB_FH_SIZE})
# 31 characters fit pb's structure with their NUL, 32 don't
pb_fh_refuses(fh_version_fits_pbs_structure
    "PB_VERSION has no room in struct pb_fh_handover"
    -DPB_VERSION=\"3.0.0-01234567890123456789012345\" ${PB_FH_BASE} ${PB_FH_SIZE})

# The same command with values the list can have compiles: the tests above
# fail for their rule and for nothing else
add_test(NAME unit.fh_compiles
    COMMAND ${CMAKE_C_COMPILER} -std=gnu99 -fsyntax-only -Werror
            -include ${PROJECT_SOURCE_DIR}/src/plat/host/compat.h
            -I${PROJECT_BINARY_DIR} -I${PROJECT_SOURCE_DIR}/src
            -I${PROJECT_SOURCE_DIR}/include -I${PROJECT_SOURCE_DIR}/include/fdt
            -I${PROJECT_SOURCE_DIR}/src/arch/host/include
            -DLOGLEVEL=3 -DCONFIG_BOOT_FH=1
            -DPB_VERSION=\"3.0.0-0123456789012345678901234\" ${PB_FH_BASE}
            -DCONFIG_BOOT_FH_REGION_SIZE=0xc0
            ${PROJECT_SOURCE_DIR}/src/boot/fh.c
)
set_tests_properties(unit.fh_compiles PROPERTIES LABELS unit TIMEOUT 60)

# boot/pb_fh.h is shared with the next stage, which may be C++: the header
# alone, with nothing else of pb's, as C++ and as C
find_program(PB_CXX_COMPILER NAMES g++ c++ REQUIRED)
add_test(NAME unit.pb_fh_is_cxx
    COMMAND ${PB_CXX_COMPILER} -std=c++11 -fsyntax-only -Wall -Wextra -Werror
            -x c++ ${PROJECT_SOURCE_DIR}/include/boot/pb_fh.h
)
set_tests_properties(unit.pb_fh_is_cxx PROPERTIES LABELS unit TIMEOUT 60)
add_test(NAME unit.pb_fh_is_c
    COMMAND ${CMAKE_C_COMPILER} -std=c11 -ffreestanding -fsyntax-only -Wall -Wextra -Werror
            -x c ${PROJECT_SOURCE_DIR}/include/boot/pb_fh.h
)
set_tests_properties(unit.pb_fh_is_c PROPERTIES LABELS unit TIMEOUT 60)

# The delivery through TF-A is ARMv8-A's and isn't in pb-host. Its test gets
# that arch's names for the registers' fields, the one register it reads from
# a fake, and TF-A's headers. Built twice: with the bl_params list in pb's own
# data, and in the platform's memory for it (platform.md 5.3, the i.MX8M)
function(pb_tfa_test name)
    target_include_directories(${name} BEFORE PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/fakes/armv8a
        ${PROJECT_SOURCE_DIR}/src/arch/armv8a/include
    )
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/src/boot)
endfunction()

pb_unit_test(test_tfa SOURCES
    ${PROJECT_SOURCE_DIR}/src/boot/atf.c
)
pb_tfa_test(test_tfa)

add_executable(test_tfa_plat test_tfa.c ${PROJECT_SOURCE_DIR}/src/boot/atf.c)
target_compile_definitions(test_tfa_plat PRIVATE PLAT_BL_PARAMS_SIZE=512)
target_include_directories(test_tfa_plat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_tfa_plat PRIVATE pb_unit_fakes PkgConfig::CMOCKA)
pb_tfa_test(test_tfa_plat)
add_test(NAME unit.test_tfa_plat COMMAND test_tfa_plat)
set_tests_properties(unit.test_tfa_plat PROPERTIES LABELS unit TIMEOUT 60)

# The image handover into the delivery, as the 8MP EVK and the MEK boot
pb_unit_test(test_tfa_image SOURCES
    ${PROJECT_SOURCE_DIR}/src/boot/atf.c
    ${PROJECT_SOURCE_DIR}/src/boot/dtb.c
    ${PROJECT_SOURCE_DIR}/src/boot/image.c
)
pb_tfa_test(test_tfa_image)

pb_unit_test(test_cm SOURCES
    ${PROJECT_SOURCE_DIR}/src/cm/cm_main.c
    ${PROJECT_SOURCE_DIR}/src/cm/cm_auth.c
    ${PROJECT_SOURCE_DIR}/src/cm/cm_board.c
    ${PROJECT_SOURCE_DIR}/src/cm/cm_boot.c
    ${PROJECT_SOURCE_DIR}/src/cm/cm_device.c
    ${PROJECT_SOURCE_DIR}/src/cm/cm_part.c
    ${PROJECT_SOURCE_DIR}/src/cm/cm_slc.c
    ${PROJECT_SOURCE_DIR}/src/cm/commands.c
    ${PROJECT_SOURCE_DIR}/src/log_source.c
    ${PROJECT_SOURCE_DIR}/src/ramlog.c
    ${PROJECT_SOURCE_DIR}/src/wire.c
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/crypto.c
    ${PROJECT_SOURCE_DIR}/src/delay.c
)
# Both methods, whatever pb-host is built with: its board has no password
target_compile_definitions(test_cm PRIVATE CONFIG_CM_AUTH_PASSWORD=1)

# Command mode without CONFIG_SLC_EOL: the same tests, SLC_SET_EOL answers
# NOT_SUPPORTED (command-mode.md 4.5, 5.2)
pb_no_slc_eol_test(test_cm_no_eol test_cm.c
    ${PROJECT_SOURCE_DIR}/src/cm/cm_main.c
    ${PROJECT_SOURCE_DIR}/src/cm/cm_auth.c
    ${PROJECT_SOURCE_DIR}/src/cm/cm_board.c
    ${PROJECT_SOURCE_DIR}/src/cm/cm_boot.c
    ${PROJECT_SOURCE_DIR}/src/cm/cm_device.c
    ${PROJECT_SOURCE_DIR}/src/cm/cm_part.c
    ${PROJECT_SOURCE_DIR}/src/cm/cm_slc.c
    ${PROJECT_SOURCE_DIR}/src/cm/commands.c
    ${PROJECT_SOURCE_DIR}/src/log_source.c
    ${PROJECT_SOURCE_DIR}/src/ramlog.c
    ${PROJECT_SOURCE_DIR}/src/wire.c
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/crypto.c
    ${PROJECT_SOURCE_DIR}/src/delay.c
)
target_compile_definitions(test_cm_no_eol PRIVATE CONFIG_CM_AUTH_PASSWORD=1)

# A build with CONFIG_CM_AUTH and no method doesn't build (command-mode.md
# 3.2, D-059): pb/cm.h with a configuration that has the gate and no method
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/cm_auth_no_method/config.h
     "#define CONFIG_CM 1\n#define CONFIG_CM_AUTH 1\n")
add_test(NAME unit.cm_auth_needs_a_method
    COMMAND ${CMAKE_C_COMPILER} -fsyntax-only
            -I${CMAKE_CURRENT_BINARY_DIR}/cm_auth_no_method -I${PROJECT_SOURCE_DIR}/include
            -x c ${PROJECT_SOURCE_DIR}/include/pb/cm.h
)
set_tests_properties(unit.cm_auth_needs_a_method PROPERTIES LABELS unit TIMEOUT 60
    PASS_REGULAR_EXPRESSION "CONFIG_CM_AUTH needs a method")

# A LOG macro whose level isn't compiled in is a statement (F-109): pb/log_def.h
# at LOGLEVEL 0 without the RAM log, after an if and an else, with -Werror
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/log_disabled/config.h "")
add_test(NAME unit.log_disabled_is_a_statement
    COMMAND ${CMAKE_C_COMPILER} -fsyntax-only -Wall -Wextra -Werror -DLOGLEVEL=0
            -I${CMAKE_CURRENT_BINARY_DIR}/log_disabled -I${PROJECT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/log_disabled.c
)
set_tests_properties(unit.log_disabled_is_a_statement PROPERTIES LABELS unit TIMEOUT 60)

# Command mode's USB port is the platform's Kconfig choice, in the board's
# defconfig (platform.md 3.10): its test reads Kconfig
add_test(
    NAME unit.cm_usb_kconfig
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_cm_usb_kconfig.py
            ${PROJECT_SOURCE_DIR}
)
set_tests_properties(unit.cm_usb_kconfig PROPERTIES LABELS unit TIMEOUT 60)

# The watchdog's time-out has a range every platform's watchdog takes, and
# every defconfig's value is inside it (F-416): its test reads Kconfig
add_test(
    NAME unit.watchdog_kconfig
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_watchdog_kconfig.py
            ${PROJECT_SOURCE_DIR}
)
set_tests_properties(unit.watchdog_kconfig PROPERTIES LABELS unit TIMEOUT 60)

# main() and its loop over the tasks (board.md 4.5). main.c's entry point is
# renamed, as for pb-host
set_source_files_properties(${PROJECT_SOURCE_DIR}/src/main.c
    PROPERTIES COMPILE_DEFINITIONS main=pb_main)
pb_unit_test(test_main SOURCES
    ${PROJECT_SOURCE_DIR}/src/main.c
)

# The same for a build without a task. The linker provides the list's start
# and end only when something is in it, so they are set here: an empty list
add_executable(test_main_no_tasks test_main.c ${PROJECT_SOURCE_DIR}/src/main.c)
target_compile_definitions(test_main_no_tasks PRIVATE TEST_NO_TASKS=1)
target_include_directories(test_main_no_tasks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_main_no_tasks PRIVATE pb_unit_fakes PkgConfig::CMOCKA)
target_link_options(test_main_no_tasks PRIVATE
    -no-pie
    -Wl,--defsym=__start_pb_tasks=0
    -Wl,--defsym=__stop_pb_tasks=0
)
add_test(NAME unit.test_main_no_tasks COMMAND test_main_no_tasks)
set_tests_properties(unit.test_main_no_tasks PROPERTIES LABELS unit TIMEOUT 60)

# The ports against the platform contract (platform.md). A port's plat.c is
# built for the host with the SoC's registers in a register file behind the
# fake pb/mmio.h (fakes/regs.h), the drivers its test names and the port's
# Kconfig symbols, which the host's config.h doesn't have.
# A port with an MMU gets fakes/xlat.c for its set up.
# pb_plat_test(name SOURCES <the port and its drivers> DEFINITIONS <symbols>
#              INCLUDES <directories, before the others> OPTIONS <for the compiler>
#              TEST <the test's source, when it isn't name.c: a second build of it>)
function(pb_plat_test name)
    cmake_parse_arguments(ARG "" "TEST" "SOURCES;DEFINITIONS;INCLUDES;OPTIONS" ${ARGN})
    if(NOT ARG_TEST)
        set(ARG_TEST ${name}.c)
    endif()
    add_executable(${name} ${ARG_TEST} fakes/regs.c ${ARG_SOURCES})
    target_include_directories(${name} BEFORE PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/fakes/mmio ${ARG_INCLUDES})
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include/xlat_tables)
    target_compile_definitions(${name} PRIVATE ${ARG_DEFINITIONS})
    target_compile_options(${name} PRIVATE ${ARG_OPTIONS})
    target_link_libraries(${name} PRIVATE pb_lib PkgConfig::CMOCKA)
    add_test(NAME unit.${name} COMMAND ${name})
    set_tests_properties(unit.${name} PROPERTIES LABELS unit TIMEOUT 60)
endfunction()

pb_plat_test(test_plat_imxrt
    SOURCES
        ${PROJECT_SOURCE_DIR}/src/plat/imxrt/plat.c
        ${PROJECT_SOURCE_DIR}/src/drivers/timer/imx_gpt.c
        ${PROJECT_SOURCE_DIR}/src/drivers/wdog/imx_wdog.c
    DEFINITIONS CONFIG_ENABLE_WATCHDOG=1 CONFIG_WATCHDOG_TIMEOUT=5
)

# The same without CONFIG_DRIVERS_CRYPTO_MBEDTLS: the platform sets a crypto
# backend up behind its option (platform.md 2.2, F-247). The build without it
# is the host's configuration less that line, first on the include path
string(REPLACE "#define CONFIG_DRIVERS_CRYPTO_MBEDTLS 1\n" ""
    PB_CONFIG_H_NO_MBEDTLS "${PB_CONFIG_H}")
if(PB_CONFIG_H_NO_MBEDTLS STREQUAL PB_CONFIG_H)
    message(FATAL_ERROR "configs/host_defconfig: CONFIG_DRIVERS_CRYPTO_MBEDTLS is not set")
endif()
set(PB_NO_MBEDTLS_DIR ${CMAKE_CURRENT_BINARY_DIR}/no_mbedtls)
file(CONFIGURE OUTPUT ${PB_NO_MBEDTLS_DIR}/config.h CONTENT "${PB_CONFIG_H_NO_MBEDTLS}" @ONLY)

pb_plat_test(test_plat_imxrt_no_mbedtls
    TEST test_plat_imxrt.c
    SOURCES
        ${PROJECT_SOURCE_DIR}/src/plat/imxrt/plat.c
        ${PROJECT_SOURCE_DIR}/src/drivers/timer/imx_gpt.c
        ${PROJECT_SOURCE_DIR}/src/drivers/wdog/imx_wdog.c
    DEFINITIONS CONFIG_ENABLE_WATCHDOG=1 CONFIG_WATCHDOG_TIMEOUT=5
    INCLUDES ${PB_NO_MBEDTLS_DIR}
)

# The i.MX8QXP, with the port's SCFW client: the test is the SCFW. xlat.h
# declares the ARMv8-A enable of the MMU for plat.c
set(PB_IMX8X_SCI ${PROJECT_SOURCE_DIR}/src/plat/imx8x/sci)
pb_plat_test(test_plat_imx8x
    SOURCES
        fakes/xlat.c
        ${PROJECT_SOURCE_DIR}/src/plat/imx8x/plat.c
        ${PB_IMX8X_SCI}/ipc.c
        ${PB_IMX8X_SCI}/imx8_mu.c
        ${PB_IMX8X_SCI}/svc/misc/misc_rpc_clnt.c
        ${PB_IMX8X_SCI}/svc/pad/pad_rpc_clnt.c
        ${PB_IMX8X_SCI}/svc/pm/pm_rpc_clnt.c
        ${PB_IMX8X_SCI}/svc/rm/rm_rpc_clnt.c
        ${PB_IMX8X_SCI}/svc/seco/seco_rpc_clnt.c
        ${PB_IMX8X_SCI}/svc/timer/timer_rpc_clnt.c
        ${PROJECT_SOURCE_DIR}/src/drivers/timer/imx_gpt.c
    DEFINITIONS
        CONFIG_PLAT_IMX8X=1 CONFIG_ENABLE_WATCHDOG=1 CONFIG_WATCHDOG_TIMEOUT=5
        CONFIG_DRIVERS_IMX_CAAM=1
        BOARD_RAM_BASE=0x80000000 BOARD_RAM_END=0xc0000000
    INCLUDES ${PROJECT_SOURCE_DIR}/include/plat/imx8x
    OPTIONS "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/fakes/xlat.h"
)

# The i.MX8X's fuse dry run: the fuse, RoT and SLC helpers with the SCFW's
# calls faked by the test
pb_unit_test(test_imx8x_dry_run SOURCES
    ${PROJECT_SOURCE_DIR}/src/plat/imx8x/fusebox.c
    ${PROJECT_SOURCE_DIR}/src/plat/imx8x/rot_helpers.c
    ${PROJECT_SOURCE_DIR}/src/plat/imx8x/slc_helpers.c
)
target_compile_definitions(test_imx8x_dry_run PRIVATE
    CONFIG_PLAT_IMX8X=1 CONFIG_IMX8X_FUSE_DRY_RUN=1)
target_include_directories(test_imx8x_dry_run PRIVATE ${PROJECT_SOURCE_DIR}/include/plat/imx8x)

# The i.MX8X's SECO log, with the SCFW's two calls faked by the test
pb_unit_test(test_imx8x_seco_log SOURCES
    ${PROJECT_SOURCE_DIR}/src/plat/imx8x/seco_log.c
    ${PROJECT_SOURCE_DIR}/src/log_source.c
    ${PROJECT_SOURCE_DIR}/src/ramlog.c
)
target_compile_definitions(test_imx8x_seco_log PRIVATE CONFIG_PLAT_IMX8X=1)
target_include_directories(test_imx8x_seco_log PRIVATE ${PROJECT_SOURCE_DIR}/include/plat/imx8x)

# The same for the i.MX8DXL, whose plat_slc has no lock
add_executable(test_imx8xl_dry_run test_imx8x_dry_run.c
    ${PROJECT_SOURCE_DIR}/src/plat/imx8x/fusebox.c
    ${PROJECT_SOURCE_DIR}/src/plat/imx8x/rot_helpers.c
    ${PROJECT_SOURCE_DIR}/src/plat/imx8x/slc_helpers.c
)
target_compile_definitions(test_imx8xl_dry_run PRIVATE
    CONFIG_PLAT_IMX8XL=1 CONFIG_IMX8X_FUSE_DRY_RUN=1)
target_include_directories(test_imx8xl_dry_run PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include/plat/imx8x)
target_link_libraries(test_imx8xl_dry_run PRIVATE pb_unit_fakes PkgConfig::CMOCKA)
add_test(NAME unit.test_imx8xl_dry_run COMMAND test_imx8xl_dry_run)
set_tests_properties(unit.test_imx8xl_dry_run PROPERTIES LABELS unit TIMEOUT 60)
# The WDOG's driver alone, on the register file
pb_plat_test(test_imx_wdog
    SOURCES ${PROJECT_SOURCE_DIR}/src/drivers/wdog/imx_wdog.c
)

# The i.MX6UL, with the RAM that jiffy has
pb_plat_test(test_plat_imx6ul
    SOURCES
        fakes/xlat.c
        ${PROJECT_SOURCE_DIR}/src/plat/imx6ul/plat.c
        ${PROJECT_SOURCE_DIR}/src/drivers/timer/imx_gpt.c
        ${PROJECT_SOURCE_DIR}/src/drivers/wdog/imx_wdog.c
    DEFINITIONS
        CONFIG_ENABLE_WATCHDOG=1 CONFIG_WATCHDOG_TIMEOUT=5 CONFIG_DRIVERS_IMX_CAAM=1
        BOARD_RAM_BASE=0x80000000 BOARD_RAM_END=0xc0000000
)

# The same without CONFIG_ENABLE_WATCHDOG, which every defconfig has on: the
# reset, the kick and the power-down counters don't depend on it
pb_plat_test(test_plat_imx6ul_no_wdog
    TEST test_plat_imx6ul.c
    SOURCES
        fakes/xlat.c
        ${PROJECT_SOURCE_DIR}/src/plat/imx6ul/plat.c
        ${PROJECT_SOURCE_DIR}/src/drivers/timer/imx_gpt.c
        ${PROJECT_SOURCE_DIR}/src/drivers/wdog/imx_wdog.c
    DEFINITIONS
        CONFIG_DRIVERS_IMX_CAAM=1 BOARD_RAM_BASE=0x80000000 BOARD_RAM_END=0xc0000000
)

# The i.MX8M Plus. The arch's system registers are the test's
# (fakes/armv8a); the SoC's sizes are the port's makefile's
pb_plat_test(test_plat_imx8m
    SOURCES
        fakes/xlat.c
        ${PROJECT_SOURCE_DIR}/src/plat/imx8m/plat.c
        ${PROJECT_SOURCE_DIR}/src/drivers/timer/imx_gpt.c
        ${PROJECT_SOURCE_DIR}/src/drivers/wdog/imx_wdog.c
    DEFINITIONS
        CONFIG_PLAT_IMX8MP=1 CONFIG_ENABLE_WATCHDOG=1 CONFIG_WATCHDOG_TIMEOUT=5
        CONFIG_DRIVERS_IMX_CAAM=1
        IMX8M_OCRAM_SIZE=0x90000 IMX8M_ROM_SIZE=0x3f000
        BOARD_RAM_BASE=0x40000000 BOARD_RAM_END=0x80000000
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/fakes/armv8a
    OPTIONS "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/fakes/xlat.h"
)

# The transport that a build for a SoC links (usb.md 2): pb's USB class and
# the stack, over a fake controller. pb-host links the socket instead
pb_unit_test(test_usb_transport SOURCES
    ${PROJECT_SOURCE_DIR}/src/drivers/usb/pb_dev_cls.c
    ${PROJECT_SOURCE_DIR}/src/drivers/usb/usbd.c
)

# The same with a host on the bus: a fake controller that plays control
# requests, bulk transfers, a bus reset and a replug (fakes/usb_host.h)
pb_unit_test(test_usbd SOURCES
    fakes/usb_host.c
    ${PROJECT_SOURCE_DIR}/src/drivers/usb/pb_dev_cls.c
    ${PROJECT_SOURCE_DIR}/src/drivers/usb/usbd.c
)

# The Cadence USB3 controller's driver on a register file. Its TRBs hold 32 bit
# addresses: -no-pie keeps the ring below 4 GiB
pb_unit_test(test_cdns3_udc SOURCES
    ${PB_DMA_MODEL}
    fakes/regs.c
    ${PROJECT_SOURCE_DIR}/src/delay.c
    ${PROJECT_SOURCE_DIR}/src/drivers/usb/cdns3_udc_core.c
)
target_include_directories(test_cdns3_udc BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/mmio
)
target_compile_options(test_cdns3_udc PRIVATE -fno-pie)
target_link_options(test_cdns3_udc PRIVATE -no-pie)

# The DWC3 controller's driver on a register file with a few lines of a
# model, and the D-cache's (see the test). -no-pie keeps what the model
# reads from 32 bit registers below 4 GiB
pb_unit_test(test_dwc3_udc SOURCES
    ${PB_DMA_MODEL}
    fakes/regs.c
    ${PROJECT_SOURCE_DIR}/src/delay.c
    ${PROJECT_SOURCE_DIR}/src/drivers/usb/dwc3_udc.c
)
target_include_directories(test_dwc3_udc BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/mmio
)
target_include_directories(test_dwc3_udc PRIVATE
    ${PROJECT_SOURCE_DIR}/src/drivers/usb
)
target_compile_options(test_dwc3_udc PRIVATE -fno-pie)
target_link_options(test_dwc3_udc PRIVATE -no-pie)

# The ChipIdea controller's driver, with a model of the controller's side of
# a transfer and of the cache (see the test). Its descriptors hold 32 bit
# addresses: -no-pie keeps them and the buffers below 4 GiB. The driver's
# number of descriptors follows the command mode buffer's size, which
# fakes/imx_ci_udc/config.h sets
pb_unit_test(test_imx_ci_udc SOURCES
    ${PB_DMA_MODEL}
    fakes/regs.c
    ${PROJECT_SOURCE_DIR}/src/drivers/usb/imx_ci_udc.c
)
target_include_directories(test_imx_ci_udc BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/mmio
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/imx_ci_udc
)
target_include_directories(test_imx_ci_udc PRIVATE
    ${PROJECT_SOURCE_DIR}/src/drivers/usb
)
target_compile_options(test_imx_ci_udc PRIVATE -fno-pie)
target_link_options(test_imx_ci_udc PRIVATE -no-pie)

pb_unit_test(test_mmc_core SOURCES
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/drivers/mmc/mmc_core.c
    ${PROJECT_SOURCE_DIR}/src/drivers/mmc/mmc_emmc.c
    ${PROJECT_SOURCE_DIR}/src/drivers/mmc/mmc_sd.c
)
target_compile_definitions(test_mmc_core PRIVATE CONFIG_MMC_CORE_MAX_DEVICES=1)

# The same with CONFIG_MMC_CORE_DEBUG_IOS: test_debug_ios checks its log
add_executable(test_mmc_core_debug_ios test_mmc_core.c
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/drivers/mmc/mmc_core.c
    ${PROJECT_SOURCE_DIR}/src/drivers/mmc/mmc_emmc.c
    ${PROJECT_SOURCE_DIR}/src/drivers/mmc/mmc_sd.c
)
target_compile_definitions(test_mmc_core_debug_ios PRIVATE
    CONFIG_MMC_CORE_MAX_DEVICES=1 CONFIG_MMC_CORE_DEBUG_IOS=1)
target_include_directories(test_mmc_core_debug_ios PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_mmc_core_debug_ios PRIVATE pb_unit_fakes PkgConfig::CMOCKA)
add_test(NAME unit.test_mmc_core_debug_ios COMMAND test_mmc_core_debug_ios)
set_tests_properties(unit.test_mmc_core_debug_ios PROPERTIES LABELS unit TIMEOUT 60)

# The i.MX8M LPDDR4 init with the imx8mevk timing, against a device model
# behind a fake pb/mmio.h (see the test)
pb_unit_test(test_imx8m_ddr SOURCES
    ${PROJECT_SOURCE_DIR}/src/plat/imx8m/ddr.c
    ${PROJECT_SOURCE_DIR}/src/plat/imx8m/ddr_imx8mq.c
    ${PROJECT_SOURCE_DIR}/src/drivers/ddr/umctl2.c
    ${PROJECT_SOURCE_DIR}/src/drivers/ddr/dwc_ddrphy.c
    ${PROJECT_SOURCE_DIR}/src/board/imx8mevk/lpddr4_timing.c
)
target_include_directories(test_imx8m_ddr BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/mmio
)
target_compile_definitions(test_imx8m_ddr PRIVATE
    CONFIG_PLAT_IMX8MQ=1
    IMX8M_DDR_GOLDEN="${CMAKE_CURRENT_SOURCE_DIR}/data/imx8m_ddr_init.trace"
)

pb_unit_test(test_imx8mp_ddr SOURCES
    ${PROJECT_SOURCE_DIR}/src/plat/imx8m/ddr.c
    ${PROJECT_SOURCE_DIR}/src/plat/imx8m/ddr_imx8mp.c
    ${PROJECT_SOURCE_DIR}/src/drivers/ddr/umctl2.c
    ${PROJECT_SOURCE_DIR}/src/drivers/ddr/dwc_ddrphy.c
    ${PROJECT_SOURCE_DIR}/src/board/imx8mpevk/lpddr4_timing.c
)
target_include_directories(test_imx8mp_ddr BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/mmio
)
target_compile_definitions(test_imx8mp_ddr PRIVATE
    CONFIG_PLAT_IMX8MP=1
    IMX8M_DDR_GOLDEN="${CMAKE_CURRENT_SOURCE_DIR}/data/imx8mp_ddr_init.trace"
)

# Device models for driver tests (see models/bench.h)
add_library(pb_mmc_models STATIC
    models/bench.c
    models/emmc_model.c
    models/hal_model.c
    models/sd_model.c
    models/usdhc_model.c
)
target_include_directories(pb_mmc_models PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Boards with several cards and controllers
target_compile_definitions(pb_mmc_models PUBLIC
    CONFIG_MMC_CORE_MAX_DEVICES=4
    CONFIG_IMX_USDHC_MAX_INSTANCES=4
)
target_link_libraries(pb_mmc_models PUBLIC pb_lib PkgConfig::CMOCKA)

# Self test of the card models
pb_unit_test(test_mmc_models LIBS pb_mmc_models)

# mmc_core against the SD card model
pb_unit_test(test_mmc_sd
    SOURCES
        ${PB_BIO_CONFORMANCE}
        ${PROJECT_SOURCE_DIR}/src/bio.c
        ${PROJECT_SOURCE_DIR}/src/delay.c
        ${PROJECT_SOURCE_DIR}/src/drivers/mmc/mmc_core.c
        ${PROJECT_SOURCE_DIR}/src/drivers/mmc/mmc_emmc.c
        ${PROJECT_SOURCE_DIR}/src/drivers/mmc/mmc_sd.c
    LIBS pb_mmc_models
)

# mmc_core against the e.MMC model, JESD84-B51
pb_unit_test(test_mmc_emmc
    SOURCES
        ${PB_BIO_CONFORMANCE}
        ${PROJECT_SOURCE_DIR}/src/bio.c
        ${PROJECT_SOURCE_DIR}/src/delay.c
        ${PROJECT_SOURCE_DIR}/src/drivers/mmc/mmc_core.c
        ${PROJECT_SOURCE_DIR}/src/drivers/mmc/mmc_emmc.c
        ${PROJECT_SOURCE_DIR}/src/drivers/mmc/mmc_sd.c
    LIBS pb_mmc_models
)
target_compile_definitions(test_mmc_emmc PRIVATE CONFIG_MMC_CORE_HS200_TUNE=1)

# The uSDHC driver and mmc_core against a uSDHC register model with the
# e.MMC model on its bus. DMA addresses are 32 bits: -no-pie keeps the
# buffers below 4 GiB.
pb_unit_test(test_imx_caam SOURCES
    ${PB_DMA_MODEL}
    ${PROJECT_SOURCE_DIR}/src/crypto.c
    ${PROJECT_SOURCE_DIR}/src/delay.c
    ${PROJECT_SOURCE_DIR}/src/drivers/crypto/caam/imx_caam.c
)
target_include_directories(test_imx_caam BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/mmio
)
# The job ring holds 32 bit addresses
target_compile_options(test_imx_caam PRIVATE -fno-pie)
target_link_options(test_imx_caam PRIVATE -no-pie)

# A model of the CAAM's job ring, hashing and signature verification (see
# models/caam_model.h). It computes with Mbed TLS
add_library(pb_caam_model STATIC
    models/bench.c
    models/caam_model.c
)
target_include_directories(pb_caam_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pb_caam_model PUBLIC pb_lib pb_mbedtls PkgConfig::CMOCKA)

# The CAAM driver's hash and verification through the crypto API, against
# crypto.md and the model, which reads and writes RAM behind the D-cache's
# model (drivers.md 4). The rings and the descriptors hold 32 bit addresses
pb_unit_test(test_caam_crypto
    SOURCES
        ${PB_CRYPTO_CONFORMANCE}
        ${PB_DMA_MODEL}
        ${PROJECT_SOURCE_DIR}/src/crypto.c
        ${PROJECT_SOURCE_DIR}/src/delay.c
        ${PROJECT_SOURCE_DIR}/src/drivers/crypto/caam/imx_caam.c
    LIBS pb_caam_model pb_keystore
)
target_include_directories(test_caam_crypto BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/mmio
)
target_compile_options(test_caam_crypto PRIVATE -fno-pie)
target_link_options(test_caam_crypto PRIVATE -no-pie)

pb_unit_test(test_imx_usdhc
    SOURCES
        ${PB_BIO_CONFORMANCE}
        ${PB_DMA_MODEL}
        ${PROJECT_SOURCE_DIR}/src/bio.c
        ${PROJECT_SOURCE_DIR}/src/delay.c
        ${PROJECT_SOURCE_DIR}/src/drivers/mmc/mmc_core.c
        ${PROJECT_SOURCE_DIR}/src/drivers/mmc/mmc_emmc.c
        ${PROJECT_SOURCE_DIR}/src/drivers/mmc/mmc_sd.c
        ${PROJECT_SOURCE_DIR}/src/drivers/mmc/imx_usdhc.c
    LIBS pb_mmc_models
)
target_include_directories(test_imx_usdhc BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/mmio
)
target_compile_options(test_imx_usdhc PRIVATE -fno-pie)
target_compile_definitions(test_imx_usdhc PRIVATE CONFIG_MMC_CORE_HS200_TUNE=1)
target_link_options(test_imx_usdhc PRIVATE -no-pie)

# The FlexSPI NOR driver and bio against a NOR flash modelled behind the
# controller's registers (see the test)
pb_unit_test(test_imx_flexspi SOURCES
    fakes/regs.c
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/delay.c
    ${PROJECT_SOURCE_DIR}/src/drivers/memc/imx_flexspi.c
)
target_include_directories(test_imx_flexspi BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/mmio
)
# The MMC drivers with no log level compiled in (LOGLEVEL 0, no RAM log): no
# variable is left that only a log line uses (F-397), with -Werror
foreach(src mmc_core mmc_emmc mmc_sd imx_usdhc)
    add_test(NAME unit.mmc_log_disabled_${src}
        COMMAND ${CMAKE_C_COMPILER} -std=gnu99 -fsyntax-only -Wall -Wextra -Werror
                -include ${PROJECT_SOURCE_DIR}/src/plat/host/compat.h
                -DLOGLEVEL=0 -DCONFIG_MMC_CORE_HS200_TUNE=1 -DCONFIG_MMC_CORE_MAX_DEVICES=1
                -DCONFIG_IMX_USDHC_MAX_INSTANCES=1
                -I${CMAKE_CURRENT_BINARY_DIR}/log_disabled -I${PROJECT_SOURCE_DIR}/include
                -I${PROJECT_SOURCE_DIR}/src/arch/host/include
                ${PROJECT_SOURCE_DIR}/src/drivers/mmc/${src}.c
    )
    set_tests_properties(unit.mmc_log_disabled_${src} PROPERTIES LABELS unit TIMEOUT 60)
endforeach()

pb_unit_test(test_imx_hab_fuses SOURCES
    ${PROJECT_SOURCE_DIR}/src/drivers/fuse/imx_hab_fuses.c
)

# The OCOTP driver against a model of the controller (see the test)
pb_unit_test(test_imx_ocotp SOURCES
    ${PROJECT_SOURCE_DIR}/src/delay.c
    ${PROJECT_SOURCE_DIR}/src/drivers/fuse/imx_ocotp.c
)
target_include_directories(test_imx_ocotp BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/mmio
)

# The virtio block driver against a model of a legacy virtio-mmio device,
# without PIE: the driver hands the device 32-bit addresses
pb_unit_test(test_virtio_block SOURCES
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/delay.c
    ${PROJECT_SOURCE_DIR}/src/drivers/virtio/virtio_block.c
)
target_include_directories(test_virtio_block BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/mmio
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/cdefs
)
target_compile_options(test_virtio_block PRIVATE -fno-pie)
target_link_options(test_virtio_block PRIVATE -no-pie)

# pb's own snprintf(), as pb_snprintf(): the host's C library has one (F-089)
pb_unit_test(test_snprintf SOURCES
    ${PROJECT_SOURCE_DIR}/src/lib/libc/snprintf.c
)
target_compile_definitions(test_snprintf PRIVATE snprintf=pb_snprintf vsnprintf=pb_vsnprintf)

# --- Coverage of what a board builds and no host test reached (F-230) ---

# The i.MX6UL's HAB helpers with the 6UL's fuse map and the HABv4 driver. The
# ROM's vector table is the test's (fakes/imx6ul_hab): its functions are
# called through 32 bit pointers, so -no-pie
pb_unit_test(test_imx6ul_hab SOURCES
    ${PROJECT_SOURCE_DIR}/src/plat/imx6ul/hab.c
    ${PROJECT_SOURCE_DIR}/src/drivers/fuse/imx_hab_fuses.c
)
target_include_directories(test_imx6ul_hab BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/imx6ul_hab
)
target_compile_options(test_imx6ul_hab PRIVATE -fno-pie)
target_link_options(test_imx6ul_hab PRIVATE -no-pie)

# The i.MX8X's fuse, RoT and SLC helpers as the boards build them: without the
# fuse dry run, the SCFW's calls faked by the test
pb_unit_test(test_imx8x_helpers SOURCES
    ${PROJECT_SOURCE_DIR}/src/plat/imx8x/fusebox.c
    ${PROJECT_SOURCE_DIR}/src/plat/imx8x/rot_helpers.c
    ${PROJECT_SOURCE_DIR}/src/plat/imx8x/slc_helpers.c
)
target_compile_definitions(test_imx8x_helpers PRIVATE CONFIG_PLAT_IMX8X=1)
target_include_directories(test_imx8x_helpers PRIVATE ${PROJECT_SOURCE_DIR}/include/plat/imx8x)

# The CAAM with the low power MDHA before era 10: no SHA-384/512. The rings
# and the descriptors hold 32 bit addresses
pb_unit_test(test_caam_lp256
    SOURCES
        ${PB_DMA_MODEL}
        ${PROJECT_SOURCE_DIR}/src/crypto.c
        ${PROJECT_SOURCE_DIR}/src/delay.c
        ${PROJECT_SOURCE_DIR}/src/drivers/crypto/caam/imx_caam.c
    LIBS pb_caam_model
)
target_include_directories(test_caam_lp256 BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/mmio
)
target_compile_options(test_caam_lp256 PRIVATE -fno-pie)
target_link_options(test_caam_lp256 PRIVATE -no-pie)

# The host tools' pure parts, without a device (F-041): pytest on the
# punchboot package, which must be installed as for the integration tests
add_test(
    NAME unit.python
    COMMAND ${Python3_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/python -q
            -p no:cacheprovider
)
set_tests_properties(unit.python PROPERTIES LABELS "unit;python" TIMEOUT 120)
