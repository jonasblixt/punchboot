/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Arm TZC-380 driver (src/drivers/tzc/tzc380.c) against a register file in RAM.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/tzc/tzc380.h>
#include <pb/pb.h>
#include <string.h>

#define REG(off)                 (regs[(off) / 4])

#define CONFIGURATION            0x000
#define SETUP_LOW(n)             (0x100 + 0x10 * (n))
#define SETUP_HIGH(n)            (0x104 + 0x10 * (n))
#define ATTR(n)                  (0x108 + 0x10 * (n))

/* 16 regions, 32 bit addresses, as on the i.MX6UL */
#define CONFIG_16_REGIONS_32_BIT 0x00001f0f

static uint32_t regs[0x200 / 4];

static void init_regs(uint32_t config)
{
    memset(regs, 0, sizeof(regs));
    REG(CONFIGURATION) = config;
}

/* Runs first: the driver keeps its state between tests */
static void test_not_initialized(void **state)
{
    (void)state;
    assert_int_equal(tzc380_configure_region(1, 0x80000000, 0x40000000, TZC380_SP_ALL),
                     -PB_ERR_STATE);
}

static void test_no_configurable_region(void **state)
{
    (void)state;
    init_regs(0x00001f00);
    assert_int_equal(tzc380_init((uintptr_t)regs), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(tzc380_configure_region(1, 0x80000000, 0x40000000, TZC380_SP_ALL),
                     -PB_ERR_STATE);
}

/* The setup jiffy uses: all of its 1 GiB of DDR open to both worlds */
static void test_configure_region(void **state)
{
    (void)state;
    init_regs(CONFIG_16_REGIONS_32_BIT);
    assert_int_equal(tzc380_init((uintptr_t)regs), PB_OK);
    assert_int_equal(tzc380_configure_region(1, 0x80000000, 0x40000000, TZC380_SP_ALL), PB_OK);

    assert_int_equal(REG(SETUP_LOW(1)), 0x80000000);
    assert_int_equal(REG(SETUP_HIGH(1)), 0);
    /* sp 0xf, size 29 (2^30 bytes), enabled */
    assert_int_equal(REG(ATTR(1)), 0xf000003b);
    /* Region 0 and the others are left alone */
    assert_int_equal(REG(ATTR(0)), 0);
    assert_int_equal(REG(ATTR(2)), 0);
}

static void test_last_region_min_size(void **state)
{
    (void)state;
    init_regs(CONFIG_16_REGIONS_32_BIT);
    assert_int_equal(tzc380_init((uintptr_t)regs), PB_OK);
    assert_int_equal(tzc380_configure_region(15, 0xbe008000, 0x8000, TZC380_SP_S_R | TZC380_SP_S_W),
                     PB_OK);

    assert_int_equal(REG(SETUP_LOW(15)), 0xbe008000);
    /* sp 0xc, size 14 (32 KiB), enabled */
    assert_int_equal(REG(ATTR(15)), 0xc000001d);
}

static void test_whole_address_space(void **state)
{
    (void)state;
    init_regs(CONFIG_16_REGIONS_32_BIT);
    assert_int_equal(tzc380_init((uintptr_t)regs), PB_OK);
    assert_int_equal(tzc380_configure_region(1, 0, UINT64_C(0x100000000), TZC380_SP_ALL), PB_OK);
    /* size 31 (4 GiB) */
    assert_int_equal(REG(ATTR(1)), 0xf000003f);
}

static void test_high_address(void **state)
{
    (void)state;
    /* 16 regions, 40 bit addresses */
    init_regs(0x0000270f);
    assert_int_equal(tzc380_init((uintptr_t)regs), PB_OK);
    assert_int_equal(
        tzc380_configure_region(1, UINT64_C(0x10000000000), UINT64_C(0x8000000000), TZC380_SP_ALL),
        -PB_ERR_PARAM);
    assert_int_equal(
        tzc380_configure_region(1, UINT64_C(0x4000000000), UINT64_C(0x4000000000), TZC380_SP_ALL),
        PB_OK);
    assert_int_equal(REG(SETUP_LOW(1)), 0);
    assert_int_equal(REG(SETUP_HIGH(1)), 0x40);
}

static void test_bad_params(void **state)
{
    (void)state;
    init_regs(CONFIG_16_REGIONS_32_BIT);
    assert_int_equal(tzc380_init((uintptr_t)regs), PB_OK);

    /* Region 0 and past the last region */
    assert_int_equal(tzc380_configure_region(0, 0x80000000, 0x40000000, TZC380_SP_ALL),
                     -PB_ERR_PARAM);
    assert_int_equal(tzc380_configure_region(16, 0x80000000, 0x40000000, TZC380_SP_ALL),
                     -PB_ERR_PARAM);
    /* Not a power of two, too small, past the address width */
    assert_int_equal(tzc380_configure_region(1, 0x80000000, 0x30000000, TZC380_SP_ALL),
                     -PB_ERR_PARAM);
    assert_int_equal(tzc380_configure_region(1, 0x80000000, 0, TZC380_SP_ALL), -PB_ERR_PARAM);
    assert_int_equal(tzc380_configure_region(1, 0x80000000, 0x4000, TZC380_SP_ALL), -PB_ERR_PARAM);
    assert_int_equal(tzc380_configure_region(1, 0, UINT64_C(0x200000000), TZC380_SP_ALL),
                     -PB_ERR_PARAM);
    /* Base not aligned to the size */
    assert_int_equal(tzc380_configure_region(1, 0x90000000, 0x40000000, TZC380_SP_ALL),
                     -PB_ERR_PARAM);
    /* Bits outside sp */
    assert_int_equal(tzc380_configure_region(1, 0x80000000, 0x40000000, TZC380_SP_ALL | BIT(0)),
                     -PB_ERR_PARAM);

    /* Nothing was written */
    assert_int_equal(REG(SETUP_LOW(1)), 0);
    assert_int_equal(REG(ATTR(1)), 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_not_initialized),     cmocka_unit_test(test_no_configurable_region),
        cmocka_unit_test(test_configure_region),    cmocka_unit_test(test_last_region_min_size),
        cmocka_unit_test(test_whole_address_space), cmocka_unit_test(test_high_address),
        cmocka_unit_test(test_bad_params),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
