/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Timeouts and delays (src/delay.c) against the fake microsecond tick. The
 * tick wraps modulo 2^32 and a timeout uses differences only (platform.md
 * 3.2, drivers.md 3).
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <cmocka.h>

#include <pb/delay.h>
#include <pb/pb.h>

#include "fakes/plat.h"

/* The longest timeout a caller passes: CONFIG_CM_TRANSFER_TIMEOUT's
 * range ends at 65 s (src/cm/Kconfig) */
#define LONGEST_CALLER_TIMEOUT_us (65 * 1000000U)

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

/* A timeout that starts at 'start': expired at start + timeout_us, not before */
static void expect_end(uint32_t start, uint32_t timeout_us)
{
    struct pb_timeout to;

    fake_us_tick = start;
    assert_int_equal(pb_timeout_init_us(&to, timeout_us), PB_OK);

    fake_us_tick = start;
    assert_false(pb_timeout_has_expired(&to));
    fake_us_tick = start + timeout_us - 1;
    assert_false(pb_timeout_has_expired(&to));
    fake_us_tick = start + timeout_us;
    assert_true(pb_timeout_has_expired(&to));
    fake_us_tick = start + timeout_us + 1;
    assert_true(pb_timeout_has_expired(&to));
}

/* 0 and UINT32_MAX are refused, and a caller that doesn't check doesn't wait */
static void test_refused_timeouts_have_expired(void **state)
{
    struct pb_timeout to;
    (void)state;

    memset(&to, 0xa5, sizeof(to));
    assert_int_equal(pb_timeout_init_us(&to, 0), -PB_ERR);
    assert_true(pb_timeout_has_expired(&to));

    memset(&to, 0xa5, sizeof(to));
    assert_int_equal(pb_timeout_init_us(&to, 0xffffffff), -PB_ERR);
    assert_true(pb_timeout_has_expired(&to));
}

static void test_expires(void **state)
{
    (void)state;

    expect_end(1000, 500);
    expect_end(1000, 1);
}

/* F-211: the end just below the wrap, and the next poll just after it */
static void test_end_before_the_wrap_poll_after_it(void **state)
{
    struct pb_timeout to;
    (void)state;

    fake_us_tick = 0xfffff000;
    assert_int_equal(pb_timeout_init_us(&to, 0x800), PB_OK);

    fake_us_tick = 0xfffff7ff;
    assert_false(pb_timeout_has_expired(&to));
    fake_us_tick = 0x10;
    assert_true(pb_timeout_has_expired(&to));
}

static void test_expires_across_the_wrap(void **state)
{
    (void)state;

    expect_end(0xffffff00, 0x200);
    /* Ending on the last tick before the wrap, and on the first after it */
    expect_end(0xffffff00, 0xff);
    expect_end(0xffffff00, 0x100);
}

/* A poll loop, the tick advancing between polls: the last poll before the end
 * reads 0xfffffff0, the next one 0 */
static void test_polled_across_the_wrap(void **state)
{
    struct pb_timeout to;
    unsigned int polls = 0;
    (void)state;

    fake_us_tick = 0xfffff000;
    fake_us_tick_step = 16;
    assert_int_equal(pb_timeout_init_us(&to, 0xffa), PB_OK);
    while (!pb_timeout_has_expired(&to) && polls < 10000)
        polls++;

    /* The poll that sees it read the tick less than a step after the end */
    assert_true(polls < 10000);
    assert_true(fake_us_tick - 16 - 0xfffff000 >= 0xffa);
    assert_true(fake_us_tick - 16 - 0xfffff000 < 0xffa + 16);
}

static void test_longest_caller_timeout(void **state)
{
    (void)state;

    expect_end(1000, LONGEST_CALLER_TIMEOUT_us);
    /* Started just before the wrap, and ending just before it */
    expect_end(0xffffff00, LONGEST_CALLER_TIMEOUT_us);
    expect_end(0xffffffffU - LONGEST_CALLER_TIMEOUT_us, LONGEST_CALLER_TIMEOUT_us);
}

/* The longest one that is taken ends a tick before the start comes round */
static void test_longest_timeout(void **state)
{
    (void)state;

    expect_end(1000, 0xfffffffe);
    expect_end(0x80000000, 0xfffffffe);
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

static void test_delay_us_across_the_wrap(void **state)
{
    (void)state;

    fake_us_tick = 0xffffffc0;
    fake_us_tick_step = 10;
    pb_delay_us(100);
    assert_true(fake_us_tick - 0xffffffc0 >= 100);
    assert_true(fake_us_tick - 0xffffffc0 <= 120);
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

    /* A refused timeout must not spin forever */
    pb_delay_us(0);
    assert_int_equal(fake_us_tick, 1000);
}

#define DELAY_TEST(f) cmocka_unit_test_setup_teardown(f, setup, teardown)

int main(void)
{
    const struct CMUnitTest tests[] = {
        DELAY_TEST(test_refused_timeouts_have_expired),
        DELAY_TEST(test_expires),
        DELAY_TEST(test_end_before_the_wrap_poll_after_it),
        DELAY_TEST(test_expires_across_the_wrap),
        DELAY_TEST(test_polled_across_the_wrap),
        DELAY_TEST(test_longest_caller_timeout),
        DELAY_TEST(test_longest_timeout),
        DELAY_TEST(test_delay_us_waits_for_the_tick),
        DELAY_TEST(test_delay_us_across_the_wrap),
        DELAY_TEST(test_delay_ms),
        DELAY_TEST(test_zero_delay_returns),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
