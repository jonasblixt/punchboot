/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Linux's newest ramoops dmesg record as the log "linux"
 * (src/boot/linux_ramoops.c), with regions laid out like Linux 6.18's
 * fs/pstore/ram.c does.
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

/* A dmesg record at 'offset', as ramoops_pstore_write() leaves it: the
 * zone zapped, then the header and the text from its start */
static void put_record_at(size_t offset, uint32_t sig, const char *text)
{
    uint32_t size = (uint32_t)strlen(text);
    uint32_t hdr[3] = { sig, size, size };

    memcpy(&region[offset], hdr, sizeof(hdr));
    memcpy(&region[offset + HDR_SIZE], text, size);
}

/* In zone 'index' of 4 KiB zones */
static void put_record(size_t index, uint32_t sig, const char *text)
{
    put_record_at(index * 4096, sig, text);
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

/* The newest record wins, whatever its zone, with its header */
static void test_newest(void **state)
{
    (void)state;

    assert_int_equal(init(4096, 0, 0, 0), PB_OK);
    put_record(0, SIG, "====1700000000.000005-D\nPanic#1 Part1\nold\n");
    put_record(3, SIG, "====1700000000.000010-D\nPanic#1 Part1\nnew\n");
    put_record(15, SIG, "====1699999999.999999-D\nOops#1 Part1\nolder\n");

    assert_int_equal(read_all(), 42);
    assert_memory_equal(out, "====1700000000.000010-D\nPanic#1 Part1\nnew\n", 42);
    /* Reads from an offset, and nothing past the end */
    assert_int_equal(linux_log()->read(38, out, 10), 4);
    assert_memory_equal(out, "new\n", 4);
    assert_int_equal(linux_log()->read(42, out, 10), 0);
}

/* A compressed record can't be read: the newest readable one is used */
static void test_compressed(void **state)
{
    (void)state;

    assert_int_equal(init(4096, 0, 0, 0), PB_OK);
    put_record(0, SIG, "====20.000000-C\nxx");
    put_record(1, SIG, "====10.000000-D\ntext");

    assert_int_equal(read_all(), 20);
    assert_memory_equal(out, "====10.000000-D\ntext", 20);
}

/* Linux before 4.x wrote the header without "-D" */
static void test_old_header(void **state)
{
    (void)state;

    assert_int_equal(init(4096, 0, 0, 0), PB_OK);
    put_record(2, SIG, "====5.1\nold");

    assert_int_equal(read_all(), 11);
}

static void test_invalid(void **state)
{
    static const char *const bad[] = {
        "",          "===",        "====",         "====.1-D\nx", "====1.-D\nx", "====1-D\nx",
        "====1.1-D", "====1.1-Dx", "====1.1-X\nx", "====1.1x\nx", "===1.1-D\nx", "====1.1-",
    };
    (void)state;

    assert_int_equal(init(4096, 0, 0, 0), PB_OK);
    assert_int_equal(read_all(), 0);

    for (size_t i = 0; i < ARRAY_SIZE(bad); i++) {
        memset(region, 0, sizeof(region));
        put_record(0, SIG, bad[i]);
        assert_int_equal(read_all(), 0);
    }

    /* A bad signature, a size past the zone */
    put_record(0, SIG ^ 1, "====1.1-D\nx");
    assert_int_equal(read_all(), 0);
    put_record(0, SIG, "====1.1-D\nx");
    region[8] = 0xff;
    region[9] = 0x0f;
    assert_int_equal(read_all(), 0);
}

/* Records that don't divide the space: 10000 bytes of 3000 byte records
 * are three zones of 3332 bytes (an even share, rounded down to even), the
 * console, ftrace and pmsg zones follow */
static void test_uneven_records(void **state)
{
    (void)state;

    assert_int_equal(init(3000, 65536 - 10000 - 2048, 1024, 1024), PB_OK);
    put_record_at(2 * 3332, SIG, "====1.1-D\nabc");

    assert_int_equal(read_all(), 13);
    assert_memory_equal(&out[10], "abc", 3);
}

static void test_bad_config(void **state)
{
    (void)state;

    /* The zones don't fit, no records, no room for a record */
    assert_int_equal(init(4096, 65536, 1, 0), -PB_ERR_PARAM);
    assert_int_equal(init(0, 0, 0, 0), -PB_ERR_PARAM);
    assert_int_equal(init(65537, 0, 0, 0), -PB_ERR_PARAM);
    assert_int_equal(init(HDR_SIZE, 65536 - HDR_SIZE, 0, 0), -PB_ERR_PARAM);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_newest),         cmocka_unit_test(test_compressed),
        cmocka_unit_test(test_old_header),     cmocka_unit_test(test_invalid),
        cmocka_unit_test(test_uneven_records), cmocka_unit_test(test_bad_config),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
