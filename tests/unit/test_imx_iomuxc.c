/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The IOMUXC pin tables (src/drivers/pinctrl/imx_iomuxc.c), with a fake
 * pb/mmio.h that records the writes, and the platforms' pin macros.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/pinctrl/imx_iomuxc.h>
#include <pb/pb.h>
#include <plat/imx6ul/pins.h>
#include <plat/imxrt/pins.h>
#include <string.h>

static struct {
    uintptr_t addr;
    uint32_t value;
} writes[16];

static size_t n_writes;

uint64_t fake_mmio_read(uintptr_t addr, unsigned int width)
{
    (void)addr;
    (void)width;
    fail();
    return 0;
}

void fake_mmio_write(uintptr_t addr, uint64_t value, unsigned int width)
{
    assert_int_equal(width, 32);
    assert_true(n_writes < ARRAY_SIZE(writes));
    writes[n_writes].addr = addr;
    writes[n_writes].value = (uint32_t)value;
    n_writes++;
}

static void assert_write(size_t n, uintptr_t addr, uint32_t value)
{
    assert_true(n < n_writes);
    assert_int_equal(writes[n].addr, addr);
    assert_int_equal(writes[n].value, value);
}

static int setup(void **state)
{
    (void)state;
    memset(writes, 0, sizeof(writes));
    n_writes = 0;
    return 0;
}

static void test_setup(void **state)
{
    static const struct imx_pin pins[] = {
        { 0x1000, 0x2000, 2, 0x10b0 },
        { 0x1004, 0x2004, 1, IMX_PAD_KEEP },
        { 0x1008, 0x2008, 0x12, 0x0, 0x3000, 1 },
    };
    (void)state;

    imx_iomuxc_setup(pins, ARRAY_SIZE(pins));
    assert_int_equal(n_writes, 6);
    assert_write(0, 0x1000, 2);
    assert_write(1, 0x2000, 0x10b0);
    /* The pad is kept */
    assert_write(2, 0x1004, 1);
    /* A pad of 0 is written, then the select input */
    assert_write(3, 0x1008, 0x12);
    assert_write(4, 0x2008, 0x0);
    assert_write(5, 0x3000, 1);
}

static void test_empty(void **state)
{
    (void)state;

    imx_iomuxc_setup(NULL, 0);
    assert_int_equal(n_writes, 0);
}

/* The pin macros make an entry by the pin's name */
static void test_pin_macros(void **state)
{
    static const struct imx_pin pins[] = {
        IMX6UL_PIN(SD1_CLK, MUX_ALT(0), 0x10),
        IMXRT_PIN_IN(GPIO_AD_B0_12, IMXRT_IOMUX_ALT(2), IMX_PAD_KEEP, 0x401f8600, 1),
    };
    (void)state;

    assert_int_equal(pins[0].mux_reg, SW_MUX_CTL_PAD_SD1_CLK);
    assert_int_equal(pins[0].pad_reg, SW_PAD_CTL_PAD_SD1_CLK);
    assert_int_equal(pins[0].mux, 0);
    assert_int_equal(pins[0].pad, 0x10);
    assert_int_equal(pins[0].input_reg, 0);
    assert_int_equal(pins[1].mux_reg, SW_MUX_CTL_PAD_GPIO_AD_B0_12);
    assert_int_equal(pins[1].pad_reg, SW_PAD_CTL_PAD_GPIO_AD_B0_12);
    assert_int_equal(pins[1].mux, 2);
    assert_int_equal(pins[1].pad, IMX_PAD_KEEP);
    assert_int_equal(pins[1].input_reg, 0x401f8600);
    assert_int_equal(pins[1].input, 1);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_setup, setup),
        cmocka_unit_test_setup(test_empty, setup),
        cmocka_unit_test_setup(test_pin_macros, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
