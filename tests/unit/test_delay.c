/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Timeouts and delays (src/delay.c) against the fake microsecond tick.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <pb/delay.h>
#include <pb/pb.h>

#include "fakes/plat.h"

static int setup(void **state)
{
    (void)state;
    fake_us_tick = 1000;
    fake_us_tick_step = 0;
    return 0;
}

static int teardown(void **state)
{
    (void)state;
    fake_us_tick = FAKE_US_TICK_DEFAULT;
    fake_us_tick_step = 0;
    return 0;
}

static void test_invalid_timeouts(void **state)
{
    struct pb_timeout to;
    (void)state;

    assert_int_equal(pb_timeout_init_us(&to, 0), -PB_ERR);
    assert_int_equal(pb_timeout_init_us(&to, 0xffffffff), -PB_ERR);
}

static void test_expires(void **state)
{
    struct pb_timeout to;
    (void)state;

    assert_int_equal(pb_timeout_init_us(&to, 500), PB_OK);
    assert_false(to.rollover);

    fake_us_tick = 1499;
    assert_false(pb_timeout_has_expired(&to));
    fake_us_tick = 1500;
    assert_true(pb_timeout_has_expired(&to));
}

static void test_expires_across_tick_rollover(void **state)
{
    struct pb_timeout to;
    (void)state;

    fake_us_tick = 0xffffff00;
    assert_int_equal(pb_timeout_init_us(&to, 0x200), PB_OK);
    assert_true(to.rollover);

    /* Before the tick wraps */
    fake_us_tick = 0xfffffff0;
    assert_false(pb_timeout_has_expired(&to));
    /* After the wrap, but before the end */
    fake_us_tick = 0x50;
    assert_false(pb_timeout_has_expired(&to));
    fake_us_tick = to.ts_end;
    assert_true(pb_timeout_has_expired(&to));
}

static void test_delay_us_waits_for_the_tick(void **state)
{
    (void)state;

    fake_us_tick_step = 10;
    pb_delay_us(100);
    /* One read in init, then one per poll until the tick reaches 1100 */
    assert_true(fake_us_tick >= 1100);
    assert_true(fake_us_tick <= 1120);
}

static void test_delay_ms(void **state)
{
    (void)state;

    fake_us_tick_step = 100;
    pb_delay_ms(2);
    assert_true(fake_us_tick >= 3000);
    assert_true(fake_us_tick <= 3200);
}

static void test_zero_delay_returns(void **state)
{
    (void)state;

    /* An invalid timeout must not spin forever */
    pb_delay_us(0);
    assert_int_equal(fake_us_tick, 1000);
}

#define DELAY_TEST(f) cmocka_unit_test_setup_teardown(f, setup, teardown)

int main(void)
{
    const struct CMUnitTest tests[] = {
        DELAY_TEST(test_invalid_timeouts),
        DELAY_TEST(test_expires),
        DELAY_TEST(test_expires_across_tick_rollover),
        DELAY_TEST(test_delay_us_waits_for_the_tick),
        DELAY_TEST(test_delay_ms),
        DELAY_TEST(test_zero_delay_returns),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
