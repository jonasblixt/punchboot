/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The OCOTP's locks until a power-on reset (src/drivers/fuse/imx_ocotp.c,
 * imx_ocotp_lock()) against a register file in RAM.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/fuse/imx_ocotp.h>
#include <pb/pb.h>
#include <string.h>

#define REG(off) (regs[(off) / 4])

static uint32_t regs[0x100 / 4];

#define LOCKS (OCOTP_SW_STICKY_SRK_REVOKE_LOCK | OCOTP_SW_STICKY_FIELD_RETURN_LOCK)

/* Runs first: the driver keeps its base between tests */
static void test_not_initialized(void **state)
{
    (void)state;
    assert_int_equal(imx_ocotp_lock(LOCKS, true), -PB_ERR_STATE);
}

/* The register file has no SET alias, so SCS's lock is set by hand where the
 * hardware would set it */
static void test_lock(void **state)
{
    (void)state;
    memset(regs, 0, sizeof(regs));
    assert_int_equal(imx_ocotp_init((uintptr_t)regs, 8), PB_OK);

    REG(OCOTP_SCS) = OCOTP_SCS_LOCK;
    assert_int_equal(imx_ocotp_lock(LOCKS, true), PB_OK);
    assert_int_equal(REG(OCOTP_SW_STICKY), LOCKS);
    assert_int_equal(REG(OCOTP_SCS_SET), OCOTP_SCS_LOCK);

    /* Bits already set stay set */
    REG(OCOTP_SW_STICKY) = BIT(4);
    assert_int_equal(imx_ocotp_lock(LOCKS, false), PB_OK);
    assert_int_equal(REG(OCOTP_SW_STICKY), BIT(4) | LOCKS);
}

/* SCS didn't lock: the write went to SCS_SET, SCS reads unlocked */
static void test_scs_not_locked(void **state)
{
    (void)state;
    memset(regs, 0, sizeof(regs));
    assert_int_equal(imx_ocotp_init((uintptr_t)regs, 8), PB_OK);

    assert_int_equal(imx_ocotp_lock(LOCKS, true), -PB_ERR_IO);
    assert_int_equal(imx_ocotp_lock(LOCKS, false), PB_OK);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_not_initialized),
        cmocka_unit_test(test_lock),
        cmocka_unit_test(test_scs_not_locked),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
