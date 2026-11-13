# C unit tests (CMocka). Each test links the modules under test, the shared
# libraries and the fakes it needs.

find_package(PkgConfig REQUIRED)
pkg_check_modules(CMOCKA REQUIRED IMPORTED_TARGET cmocka)

add_library(pb_unit_fakes STATIC
    fakes/plat.c
    ${PROJECT_SOURCE_DIR}/src/log.c
    fakes/ramdisk.c
)
target_link_libraries(pb_unit_fakes PUBLIC pb_lib)

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

pb_unit_test(test_der)

pb_unit_test(test_tzc380 SOURCES
    ${PROJECT_SOURCE_DIR}/src/drivers/tzc/tzc380.c
)

pb_unit_test(test_ramlog SOURCES
    ${PROJECT_SOURCE_DIR}/src/ramlog.c
)
# CONFIG_RAMLOG and the log size come from configs/host_defconfig
target_compile_definitions(test_ramlog PRIVATE CONFIG_RAMLOG_SYNC=1)

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

pb_unit_test(test_mbedtls
    SOURCES
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

pb_unit_test(test_imx_iomuxc SOURCES
    ${PROJECT_SOURCE_DIR}/src/drivers/pinctrl/imx_iomuxc.c
)
target_include_directories(test_imx_iomuxc BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes/mmio
)

# Includes src/rot.c, to set the slot list per test
pb_unit_test(test_rot_slots LIBS pb_keystore)

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

# A/B's speculative mode (boot.md 3.6) isn't in pb-host: the host has no
# watchdog, which CONFIG_BOOT_AB_SPECULATIVE depends on. Its test is the same
# file and module built with the symbol, which changes nothing else in the
# build; what A/B does with a viable candidate runs in both
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
    ${PROJECT_SOURCE_DIR}/src/boot/policy.c
)

# The same with the password method, which pb-host is built without: the
# board's password_auth is then mandatory (board.md 3)
add_executable(test_board_password test_board.c
    ${PROJECT_SOURCE_DIR}/src/board.c
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
# none of: the Firmware Handoff region (F-259) and a platform's region for
# bl_params (platform.md 5.3), both between the suite's two image data regions
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

pb_unit_test(test_simple SOURCES
    ${PROJECT_SOURCE_DIR}/src/boot/simple.c
)

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
    ${PROJECT_SOURCE_DIR}/src/log_source.c
    ${PROJECT_SOURCE_DIR}/src/ramlog.c
    ${PROJECT_SOURCE_DIR}/src/wire.c
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/crypto.c
    ${PROJECT_SOURCE_DIR}/src/delay.c
)
# Both methods, whatever pb-host is built with: its board has no password
target_compile_definitions(test_cm PRIVATE CONFIG_CM_AUTH_PASSWORD=1)

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

# main() and its loop over the tasks (board.md 4.6). main.c's entry point is
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

# The transport that a build for a SoC links (usb.md 2): pb's USB class and
# the stack, over a fake controller. pb-host links the socket instead
pb_unit_test(test_usb_transport SOURCES
    ${PROJECT_SOURCE_DIR}/src/drivers/usb/pb_dev_cls.c
    ${PROJECT_SOURCE_DIR}/src/drivers/usb/usbd.c
)

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
# crypto.md and the model. The rings and the descriptors hold 32 bit addresses
pb_unit_test(test_caam_crypto
    SOURCES
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

pb_unit_test(test_imx_hab_fuses SOURCES
    ${PROJECT_SOURCE_DIR}/src/drivers/fuse/imx_hab_fuses.c
)

pb_unit_test(test_imx_ocotp SOURCES
    ${PROJECT_SOURCE_DIR}/src/drivers/fuse/imx_ocotp.c
)
