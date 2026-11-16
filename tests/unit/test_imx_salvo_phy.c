/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8X's USB3 PHY (src/drivers/usb/imx_salvo_phy.c) on a register file
 * (fakes/regs.h).
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/usb/imx_salvo_phy.h>
#include <pb/pb.h>

#include "fakes/plat.h"
#include "fakes/regs.h"

#define BASE   0x5b160000u
#define REG(n) (BASE + (n) * 4u)

static int setup(void **state)
{
    (void)state;
    fake_regs_reset();
    /* The 1 ms the PHY settles in passes */
    fake_us_tick_step = 10;
    return 0;
}

/* F-205: 69 writes, each to a register's word, then the receiver detection's
 * and the USB 2.0 part's. F-468: the VCO calibration's iteration timer is
 * register 0x0085, and 0x0088 isn't written */
static void test_init(void **state)
{
    (void)state;

    fake_regs_set(REG(0xe0af), 0x0f);
    fake_regs_set(REG(0x4124), 0x40);
    assert_int_equal(imx_salvo_phy_init(BASE), PB_OK);

    assert_int_equal(fake_reg_log_len, 69 + 3);
    assert_int_equal(fake_reg_log[0].addr, REG(0xc800));
    assert_int_equal(fake_reg_log[0].value, 0x0830);
    assert_int_equal(fake_reg_log[68].addr, REG(0x40f2));
    assert_int_equal(fake_reg_log[68].value, 0x0090);

    for (size_t i = 0; i < fake_reg_log_len; i++) {
        assert_int_equal(fake_reg_log[i].width, 32);
        assert_int_equal(fake_reg_log[i].value >> 16, 0);
    }

    assert_int_equal(fake_regs_get(REG(0x0085)), 0x0018);
    assert_int_equal(fake_regs_writes(REG(0x0088)), 0);

    /* Read, modify, write: the bits beside them stay */
    assert_int_equal(fake_regs_get(REG(0x4124)), 0x41);
    assert_int_equal(fake_regs_get(REG(0xe0af)), 0x3f);
    assert_int_equal(fake_regs_get(REG(0xe012)), 0x05);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_init, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
