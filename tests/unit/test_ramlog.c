/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * RAM log (src/ramlog.c), built with the host config's log size and
 * CONFIG_RAMLOG_SYNC.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <cmocka.h>

#include <pb/pb.h>
#include <pb/ramlog.h>

#define LOG_SIZE (CONFIG_RAMLOG_SIZE_KiB * 1024)

/* Cache cleans done by the RAM log, in order */
struct clean_call {
    uintptr_t start;
    size_t len;
};

static struct clean_call clean_calls[8];
static unsigned int no_of_clean_calls;

void arch_clean_cache_range(uintptr_t start, size_t len);

void arch_clean_cache_range(uintptr_t start, size_t len)
{
    if (no_of_clean_calls < ARRAY_SIZE(clean_calls)) {
        clean_calls[no_of_clean_calls].start = start;
        clean_calls[no_of_clean_calls].len = len;
    }
    no_of_clean_calls++;
}

static void puts_ramlog(const char *s)
{
    while (*s)
        ramlog_putc(*s++);
}

static int setup(void **state)
{
    (void)state;
    ramlog_init();
    no_of_clean_calls = 0;
    return 0;
}

/* Must run first: the log is in a static buffer that nothing has initialized */
static void test_putc_before_init_is_dropped(void **state)
{
    char buf[4];
    (void)state;

    ramlog_putc('x');
    assert_int_equal(ramlog_length(), 0);
    assert_int_equal(ramlog_head(), 0);
    assert_int_equal(ramlog_read(0, buf, sizeof(buf)), 0);
    assert_int_equal(no_of_clean_calls, 0);
}

static void test_init_writes_and_syncs_header(void **state)
{
    const struct pb_ramlog_hdr *hdr;
    (void)state;

    no_of_clean_calls = 0;
    ramlog_init();

    assert_int_equal(no_of_clean_calls, 1);
    assert_int_equal(clean_calls[0].len, sizeof(struct pb_ramlog_hdr));
    hdr = (const struct pb_ramlog_hdr *)clean_calls[0].start;
    assert_int_equal(hdr->magic, PB_RAMLOG_MAGIC);
    assert_memory_equal(&hdr->magic, "PBLG", 4);
    assert_int_equal(hdr->version, PB_RAMLOG_VERSION);
    assert_int_equal(hdr->size, LOG_SIZE);
    assert_int_equal(hdr->head, 0);
    assert_int_equal(ramlog_length(), 0);
}

static void test_write_and_read(void **state)
{
    char buf[16];
    (void)state;

    puts_ramlog("hello");
    assert_int_equal(ramlog_length(), 5);
    assert_int_equal(ramlog_head(), 5);

    memset(buf, 0, sizeof(buf));
    assert_int_equal(ramlog_read(0, buf, sizeof(buf)), 5);
    assert_string_equal(buf, "hello");

    memset(buf, 0, sizeof(buf));
    assert_int_equal(ramlog_read(1, buf, 3), 3);
    assert_string_equal(buf, "ell");

    assert_int_equal(ramlog_read(5, buf, sizeof(buf)), 0);
    assert_int_equal(ramlog_read(100, buf, sizeof(buf)), 0);
}

static void test_init_resets_log(void **state)
{
    char buf[4];
    (void)state;

    puts_ramlog("old");
    ramlog_init();
    assert_int_equal(ramlog_length(), 0);
    assert_int_equal(ramlog_read(0, buf, sizeof(buf)), 0);
}

/* After writing more than the log holds, the oldest bytes are gone, and a read
 * returns the rest oldest first, in one go or in chunks across the wrap. */
static void test_wrap_keeps_newest(void **state)
{
    static char buf[LOG_SIZE];
    const size_t total = LOG_SIZE * 2 + 100;
    size_t got = 0;
    (void)state;

    for (size_t i = 0; i < total; i++)
        ramlog_putc((char)(i % 251));

    assert_int_equal(ramlog_length(), LOG_SIZE);
    assert_int_equal(ramlog_head(), total);

    assert_int_equal(ramlog_read(0, buf, sizeof(buf)), LOG_SIZE);
    for (size_t i = 0; i < LOG_SIZE; i++)
        assert_int_equal((uint8_t)buf[i], (total - LOG_SIZE + i) % 251);

    memset(buf, 0, sizeof(buf));
    while (got < LOG_SIZE) {
        size_t n = ramlog_read(got, &buf[got], 7);
        assert_true(n > 0);
        got += n;
    }
    assert_int_equal(got, LOG_SIZE);
    for (size_t i = 0; i < LOG_SIZE; i++)
        assert_int_equal((uint8_t)buf[i], (total - LOG_SIZE + i) % 251);
}

/* Synchronized output: the character is cleaned before the header, and the
 * data follows the header directly in memory (external readers rely on it). */
static void test_sync_cleans_data_then_header(void **state)
{
    const struct pb_ramlog_hdr *hdr;
    uintptr_t data;
    (void)state;

    puts_ramlog("ab");
    no_of_clean_calls = 0;
    ramlog_putc('c');

    assert_int_equal(no_of_clean_calls, 2);
    assert_int_equal(clean_calls[1].len, sizeof(struct pb_ramlog_hdr));
    hdr = (const struct pb_ramlog_hdr *)clean_calls[1].start;
    data = clean_calls[1].start + sizeof(struct pb_ramlog_hdr);

    assert_int_equal(clean_calls[0].len, 1);
    assert_int_equal(clean_calls[0].start, data + 2);
    assert_int_equal(*(const char *)clean_calls[0].start, 'c');
    assert_int_equal(hdr->head, 3);
    assert_int_equal(clean_calls[1].start % 64, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_putc_before_init_is_dropped),
        cmocka_unit_test(test_init_writes_and_syncs_header),
        cmocka_unit_test_setup(test_write_and_read, setup),
        cmocka_unit_test_setup(test_init_resets_log, setup),
        cmocka_unit_test_setup(test_wrap_keeps_newest, setup),
        cmocka_unit_test_setup(test_sync_cleans_data_then_header, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
