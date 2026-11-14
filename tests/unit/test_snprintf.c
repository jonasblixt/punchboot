/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * pb's reduced snprintf() (src/lib/libc/snprintf.c, from TF-A), built for the
 * host as pb_snprintf() and pb_vsnprintf() (CMakeLists.txt renames them, the
 * host has its own).
 * It writes at most n - 1 characters and the terminator, and returns the
 * number of characters the whole output has (F-089). A specifier it doesn't
 * have ends the output where it stands, and the result is at least n: the
 * text was cut (F-418).
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

/* vsnprintf() is snprintf() with the arguments in a va_list */
static int call_vsnprintf(char *s, size_t n, const char *fmt, ...)
{
    va_list args;
    int rc;

    va_start(args, fmt);
    rc = vsnprintf(s, n, fmt, args);
    va_end(args);
    return rc;
}

/* A specifier it doesn't have ends the call: the text before it, terminated.
 * It used to spin forever (F-089). The result is at least n, so that a
 * caller sees the text was cut: it used to be the length before the
 * specifier, which is below n and read as a whole text (F-418) */
static void test_unsupported_ends(void **state)
{
    struct out o;
    (void)state;

    memset(o.buf, GUARD, sizeof(o.buf));
    assert_int_equal(snprintf(o.buf, 16, "ab%cde", 'x'), 16);
    assert_string_equal(o.buf, "ab");
    check_guard(o.buf, 16);

    memset(o.buf, GUARD, sizeof(o.buf));
    assert_int_equal(snprintf(o.buf, 16, "%u %zu", 1u, (size_t)2), 16);
    assert_string_equal(o.buf, "1 ");

    /* A width without a 0 */
    memset(o.buf, GUARD, sizeof(o.buf));
    assert_int_equal(snprintf(o.buf, 16, "[%2u]", 3u), 16);
    assert_string_equal(o.buf, "[");

    /* After padding, and at the end of the format */
    memset(o.buf, GUARD, sizeof(o.buf));
    assert_int_equal(snprintf(o.buf, 16, "%04x%08", 0x1u), 16);
    assert_string_equal(o.buf, "0001");

    memset(o.buf, GUARD, sizeof(o.buf));
    assert_int_equal(snprintf(o.buf, 16, "abc%"), 16);
    assert_string_equal(o.buf, "abc");

    /* In a buffer that is already full: the length so far is above n */
    memset(o.buf, GUARD, sizeof(o.buf));
    assert_int_equal(snprintf(o.buf, 2, "abc%c", 'x'), 3);
    assert_string_equal(o.buf, "a");
    check_guard(o.buf, 2);

    /* One byte holds the terminator alone, none holds nothing */
    memset(o.buf, GUARD, sizeof(o.buf));
    assert_int_equal(snprintf(o.buf, 1, "%c", 'x'), 1);
    assert_int_equal(o.buf[0], '\0');
    check_guard(o.buf, 1);

    memset(o.buf, GUARD, sizeof(o.buf));
    assert_int_equal(snprintf(o.buf, 0, "%c", 'x'), 0);
    check_guard(o.buf, 0);

    /* The C99 check of a cut text holds whatever the size */
    for (size_t n = 0; n < 16; n++) {
        int rc;

        memset(o.buf, GUARD, sizeof(o.buf));
        rc = snprintf(o.buf, n, "ab%pcd", (void *)&rc);
        assert_true(rc >= 0 && (size_t)rc >= n);
        check_guard(o.buf, n);
    }

    /* And vsnprintf() the same, for a caller that keeps a text's length
     * (imx_hab_log.c, seco_log.c) */
    memset(o.buf, GUARD, sizeof(o.buf));
    assert_int_equal(call_vsnprintf(o.buf, 32, "ev %s %lu", "a", 1UL), 32);
    assert_string_equal(o.buf, "ev a ");
}

static void test_vsnprintf(void **state)
{
    struct out o;
    (void)state;

    memset(o.buf, GUARD, sizeof(o.buf));
    assert_int_equal(call_vsnprintf(o.buf, 32, "%s %i 0x%08x", "ev", -3, 0x87ee00u), 16);
    assert_string_equal(o.buf, "ev -3 0x0087ee00");
    check_guard(o.buf, 32);

    memset(o.buf, GUARD, sizeof(o.buf));
    assert_int_equal(call_vsnprintf(o.buf, 8, "%s %i 0x%08x", "ev", -3, 0x87ee00u), 16);
    assert_string_equal(o.buf, "ev -3 0");
    check_guard(o.buf, 8);
}

int main(void)
{
    /* A spin in the code under test fails the test in seconds */
    alarm(10);

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_x_fits),        cmocka_unit_test(test_x_bounded),
        cmocka_unit_test(test_d_u_s_bounded), cmocka_unit_test(test_unsupported_ends),
        cmocka_unit_test(test_vsnprintf),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
