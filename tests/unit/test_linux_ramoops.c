/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Linux's ramoops console as the log "linux" (src/boot/linux_ramoops.c),
 * with regions laid out like Linux 6.18's fs/pstore/ram.c does.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <boot/linux_ramoops.h>
#include <pb/crc.h>
#include <pb/log_source.h>
#include <pb/pb.h>
#include <string.h>

#define HDR_SIZE 12 /* struct persistent_ram_buffer: sig, start, size */
#define SIG      0x43474244

static uint8_t region[64 * 1024] __attribute__((aligned(8)));
static char out[16 * 1024];

static const struct log_source *linux_log(void)
{
    return log_source_get(crc32(0, (const uint8_t *)"linux", 5));
}

/* A console zone at 'offset' in the region */
static void put_console(size_t offset, uint32_t sig, uint32_t start, uint32_t size)
{
    uint32_t hdr[3] = { sig, start, size };

    memcpy(&region[offset], hdr, sizeof(hdr));
}

static size_t read_all(void)
{
    const struct log_source *src = linux_log();

    assert_non_null(src);
    assert_int_equal(src->head(), src->length());
    return src->read(0, out, sizeof(out));
}

static int init(size_t record, size_t console, size_t ftrace, size_t pmsg)
{
    static struct linux_ramoops_config cfg;

    cfg = (struct linux_ramoops_config){
        .addr = (uintptr_t)region,
        .size = sizeof(region),
        .record_size = record,
        .console_size = console,
        .ftrace_size = ftrace,
        .pmsg_size = pmsg,
    };
    memset(region, 0, sizeof(region));
    return linux_ramoops_init(&cfg);
}

/* 64 KiB, 4 KiB records, 8 KiB console, 4 KiB ftrace and pmsg: twelve
 * records take 48 KiB, the console follows */
static void test_console(void **state)
{
    (void)state;

    assert_int_equal(init(4096, 8192, 4096, 4096), PB_OK);
    put_console(48 * 1024, SIG, 6, 6);
    memcpy(&region[48 * 1024 + HDR_SIZE], "hello\n", 6);

    assert_int_equal(read_all(), 6);
    assert_memory_equal(out, "hello\n", 6);
    /* Reads from an offset, and nothing past the end */
    assert_int_equal(linux_log()->read(2, out, 2), 2);
    assert_memory_equal(out, "ll", 2);
    assert_int_equal(linux_log()->read(6, out, 10), 0);
}

/* A full ring: the oldest byte is the next one Linux writes */
static void test_wrapped(void **state)
{
    const size_t capacity = 8192 - HDR_SIZE;
    uint8_t *data = &region[48 * 1024 + HDR_SIZE];
    (void)state;

    assert_int_equal(init(4096, 8192, 4096, 4096), PB_OK);
    for (size_t i = 0; i < capacity; i++)
        data[i] = (uint8_t)('a' + (i % 26));
    put_console(48 * 1024, SIG, 100, capacity);

    assert_int_equal(read_all(), capacity);
    assert_memory_equal(out, &data[100], capacity - 100);
    assert_memory_equal(&out[capacity - 100], data, 100);
    /* A read across the wrap */
    assert_int_equal(linux_log()->read(capacity - 102, out, 4), 4);
    assert_memory_equal(out, &data[capacity - 2], 2);
    assert_memory_equal(&out[2], data, 2);
}

/* Records that don't divide the space: 10000 bytes of 3000 byte records
 * are three zones of 3332 bytes (an even share, rounded down to even) */
static void test_uneven_records(void **state)
{
    (void)state;

    assert_int_equal(init(3000, 65536 - 10000, 0, 0), PB_OK);
    put_console(9996, SIG, 3, 3);
    memcpy(&region[9996 + HDR_SIZE], "abc", 3);

    assert_int_equal(read_all(), 3);
    assert_memory_equal(out, "abc", 3);
}

/* Without records the console starts at the region */
static void test_no_records(void **state)
{
    (void)state;

    assert_int_equal(init(0, 8192, 0, 0), PB_OK);
    put_console(0, SIG, 2, 2);
    memcpy(&region[HDR_SIZE], "ok", 2);

    assert_int_equal(read_all(), 2);
}

/* Without a valid header there is nothing to read */
static void test_no_console(void **state)
{
    (void)state;

    assert_int_equal(init(0, 8192, 0, 0), PB_OK);
    assert_int_equal(read_all(), 0);

    put_console(0, SIG ^ 1, 0, 2);
    assert_int_equal(read_all(), 0);
    put_console(0, SIG, 0, 8192);
    assert_int_equal(read_all(), 0);
    put_console(0, SIG, 8192 - HDR_SIZE, 2);
    assert_int_equal(read_all(), 0);
}

static void test_bad_config(void **state)
{
    (void)state;

    assert_int_equal(init(0, 65536, 1, 0), -PB_ERR_PARAM);
    assert_int_equal(init(0, HDR_SIZE, 0, 0), -PB_ERR_PARAM);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_console),        cmocka_unit_test(test_wrapped),
        cmocka_unit_test(test_uneven_records), cmocka_unit_test(test_no_records),
        cmocka_unit_test(test_no_console),     cmocka_unit_test(test_bad_config),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
