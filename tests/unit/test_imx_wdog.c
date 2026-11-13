/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX WDOG's driver (src/drivers/wdog/imx_wdog.c), with the WDOG's
 * registers in a register file (fakes/regs.h). The values are the ones the
 * i.MX6UL, i.MX8M and i.MX RT boards run.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/wdog/imx_wdog.h>
#include <pb/pb.h>

#include "fakes/regs.h"

#define WDOG_BASE 0x020bc000

/* A write that resets the SoC leaves the code under test through here */
static jmp_buf soc_reset;

static uint64_t soc_on_write(uintptr_t addr, uint64_t value)
{
    /* The software reset: WCR.SRS written 0 */
    if (addr == WDOG_BASE + WDOG_WCR && (value & WDOG_WCR_SRS) == 0)
        longjmp(soc_reset, 1);

    return value;
}

static int setup(void **state)
{
    (void)state;
    fake_regs_reset();
    fake_regs_on_write = soc_on_write;
    return 0;
}

/* The time-out field of the WCR that imx_wdog_init() wrote for a time-out */
static unsigned int wt_of(unsigned int delay_s)
{
    fake_regs_reset();
    assert_int_equal(imx_wdog_init(WDOG_BASE, delay_s), PB_OK);
    assert_int_equal(fake_reg_log[0].addr, WDOG_BASE + WDOG_WCR);
    assert_int_equal(fake_reg_log[0].width, 16);
    return (unsigned int)(fake_reg_log[0].value >> 8);
}

/* imx_wdog_init() arms the watchdog: enabled, a time-out resets the SoC and
 * asserts WDOG_B, neither of the two software resets is asked for. The
 * power-down counter is stopped and the counter restarted */
static void test_init(void **state)
{
    (void)state;

    assert_int_equal(imx_wdog_init(WDOG_BASE, 5), PB_OK);

    assert_int_equal(fake_reg_log_len, 4);
    assert_int_equal(fake_reg_log[0].addr, WDOG_BASE + WDOG_WCR);
    assert_int_equal(fake_reg_log[0].value & 0xff,
                     WDOG_WCR_WDE | WDOG_WCR_WDT | WDOG_WCR_SRS | WDOG_WCR_WDA);
    assert_int_equal(fake_reg_log[1].addr, WDOG_BASE + WDOG_WMCR);
    assert_int_equal(fake_reg_log[1].value, 0);
    assert_int_equal(fake_reg_log[2].addr, WDOG_BASE + WDOG_WSR);
    assert_int_equal(fake_reg_log[2].value, WDOG_WSR_SEQ1);
    assert_int_equal(fake_reg_log[3].addr, WDOG_BASE + WDOG_WSR);
    assert_int_equal(fake_reg_log[3].value, WDOG_WSR_SEQ2);
}

/* WT is eight bits and counts half seconds, 0xff is 128 s. A time-out that
 * doesn't fit must not wrap into a short one: 128 s as 256 half seconds was
 * a WT of 0, which is half a second, and a reset loop (F-206) */
static void test_init_timeout_too_long(void **state)
{
    (void)state;

    assert_int_equal(wt_of(128), 0xff);
    assert_int_equal(wt_of(129), 0xff);
    assert_int_equal(wt_of(300), 0xff);
    assert_int_equal(wt_of(UINT32_MAX), 0xff);
    /* And the bits below WT are the same as for a time-out that fits */
    assert_int_equal(fake_reg_log[0].value & 0xff,
                     WDOG_WCR_WDE | WDOG_WCR_WDT | WDOG_WCR_SRS | WDOG_WCR_WDA);
}

/* The time-outs the boards in the tree run (CONFIG_WATCHDOG_TIMEOUT 5, 10
 * and 30) and the ends of the range */
static void test_init_timeout(void **state)
{
    (void)state;

    assert_int_equal(wt_of(1), 2);
    assert_int_equal(wt_of(5), 10);
    assert_int_equal(wt_of(10), 20);
    assert_int_equal(wt_of(30), 60);
    assert_int_equal(wt_of(127), 254);
    assert_int_equal(wt_of(0), 0);
}

/* The service sequence, and nothing else */
static void test_kick(void **state)
{
    (void)state;

    assert_int_equal(imx_wdog_kick(WDOG_BASE), PB_OK);

    assert_int_equal(fake_reg_log_len, 2);
    assert_int_equal(fake_reg_log[0].addr, WDOG_BASE + WDOG_WSR);
    assert_int_equal(fake_reg_log[0].value, WDOG_WSR_SEQ1);
    assert_int_equal(fake_reg_log[1].addr, WDOG_BASE + WDOG_WSR);
    assert_int_equal(fake_reg_log[1].value, WDOG_WSR_SEQ2);
}

/* The reset is one write: the software reset (SRS 0, with SRE), and WDA 0
 * with it, which asserts WDOG_B as a time-out does (imx_wdog.h) */
static void test_reset_now(void **state)
{
    (void)state;

    if (setjmp(soc_reset) == 0) {
        imx_wdog_reset_now(WDOG_BASE);
        fail_msg("imx_wdog_reset_now() returned");
    }

    assert_int_equal(fake_reg_log_len, 1);
    assert_int_equal(fake_reg_log[0].addr, WDOG_BASE + WDOG_WCR);
    assert_int_equal(fake_reg_log[0].value, WDOG_WCR_SRE | WDOG_WCR_WDE);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_init, setup),
        cmocka_unit_test_setup(test_init_timeout, setup),
        cmocka_unit_test_setup(test_init_timeout_too_long, setup),
        cmocka_unit_test_setup(test_kick, setup),
        cmocka_unit_test_setup(test_reset_now, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
