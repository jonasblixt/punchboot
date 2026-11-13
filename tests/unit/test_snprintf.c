/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * pb's reduced snprintf() (src/lib/libc/snprintf.c, from TF-A), built for the
 * host as pb_snprintf() (CMakeLists.txt renames it, the host has its own).
 * It writes at most n - 1 characters and the terminator, and returns the
 * number of characters the whole output has (F-089).
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* What follows the buffer: nothing may be written there */
#define GUARD      0xa5
#define GUARD_SIZE 32

struct out {
    char buf[64];
};

/* buf[n..] holds the guard; check that it is untouched */
static void check_guard(const char *buf, size_t n)
{
    for (size_t i = n; i < n + GUARD_SIZE; i++)
        assert_int_equal((unsigned char)buf[i], GUARD);
}

static void test_x_fits(void **state)
{
    struct out o;
    (void)state;

    memset(o.buf, GUARD, sizeof(o.buf));
    assert_int_equal(snprintf(o.buf, 16, "a%xb", 0x12345678u), 10);
    assert_string_equal(o.buf, "a12345678b");

    assert_int_equal(snprintf(o.buf, 16, "%08x", 0xabcu), 8);
    assert_string_equal(o.buf, "00000abc");

    assert_int_equal(snprintf(o.buf, 16, "%x", 0u), 1);
    assert_string_equal(o.buf, "0");
}

/* %x is bounded by n, as %d, %u and %s are */
static void test_x_bounded(void **state)
{
    struct out o;
    (void)state;

    memset(o.buf, GUARD, sizeof(o.buf));
    assert_int_equal(snprintf(o.buf, 4, "%x", 0x12345678u), 8);
    assert_string_equal(o.buf, "123");
    check_guard(o.buf, 4);

    /* The padding too */
    memset(o.buf, GUARD, sizeof(o.buf));
    assert_int_equal(snprintf(o.buf, 4, "%016x", 0x1u), 16);
    assert_string_equal(o.buf, "000");
    check_guard(o.buf, 4);

    /* After other text, ending inside the digits */
    memset(o.buf, GUARD, sizeof(o.buf));
    assert_int_equal(snprintf(o.buf, 8, "addr 0x%08x\n", 0xdeadbeefu), 16);
    assert_string_equal(o.buf, "addr 0x");
    check_guard(o.buf, 8);

    /* One byte: only the terminator */
    memset(o.buf, GUARD, sizeof(o.buf));
    assert_int_equal(snprintf(o.buf, 1, "%08x", 0xdeadbeefu), 8);
    assert_int_equal(o.buf[0], '\0');
    check_guard(o.buf, 1);

    /* Nothing at all */
    memset(o.buf, GUARD, sizeof(o.buf));
    assert_int_equal(snprintf(o.buf, 0, "%08x", 0xdeadbeefu), 8);
    check_guard(o.buf, 0);
}

static void test_d_u_s_bounded(void **state)
{
    struct out o;
    (void)state;

    memset(o.buf, GUARD, sizeof(o.buf));
    assert_int_equal(snprintf(o.buf, 4, "%d %u %s", -12345, 678u, "abc"), 14);
    assert_string_equal(o.buf, "-12");
    check_guard(o.buf, 4);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_x_fits),
        cmocka_unit_test(test_x_bounded),
        cmocka_unit_test(test_d_u_s_bounded),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
