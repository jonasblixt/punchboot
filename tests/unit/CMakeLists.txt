# C unit tests (CMocka). Each test links the modules under test, the shared
# libraries and the fakes it needs.

find_package(PkgConfig REQUIRED)
pkg_check_modules(CMOCKA REQUIRED IMPORTED_TARGET cmocka)

add_library(pb_unit_fakes STATIC
    fakes/plat.c
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

pb_unit_test(test_gpt SOURCES
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/drivers/partition/gpt.c
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

pb_unit_test(test_rom_log SOURCES
    ${PROJECT_SOURCE_DIR}/src/plat/imx6ul/rom_log.c
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

pb_unit_test(test_rot
    SOURCES
        ${PROJECT_SOURCE_DIR}/src/rot.c
        ${PROJECT_SOURCE_DIR}/src/slc.c
    LIBS pb_keystore
)

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

pb_unit_test(test_boot SOURCES
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/boot/boot.c
)

pb_unit_test(test_image_helpers SOURCES
    ${PROJECT_SOURCE_DIR}/src/boot/image_helpers.c
    ${PROJECT_SOURCE_DIR}/src/crypto.c
)
# The pb memory regions that images must not overlap, same as for pb-host
target_link_options(test_image_helpers PRIVATE
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

pb_unit_test(test_linux SOURCES
    ${PROJECT_SOURCE_DIR}/src/boot/linux.c
)

pb_unit_test(test_cm SOURCES
    ${PROJECT_SOURCE_DIR}/src/cm/cm_main.c
    ${PROJECT_SOURCE_DIR}/src/wire.c
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/crypto.c
    ${PROJECT_SOURCE_DIR}/src/delay.c
)

pb_unit_test(test_mmc_core SOURCES
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/drivers/mmc/mmc_core.c
)

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
