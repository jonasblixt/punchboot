/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The DMA and cache rule's functions (pb/dma.h, src/dma.c, drivers.md 4)
 * against the D-cache model the drivers' tests use (fakes/dcache.h), and the
 * model itself: each way a driver breaks the rule is a wrong byte or a fault
 * here. The "device" is the test, which reads and writes RAM.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <cmocka.h>

#include <pb/arch.h>
#include <pb/dma.h>
#include <pb/pb.h>

#include "fakes/dcache.h"

#define LINE PB_DMA_ALIGN

/* A caller's memory: whole lines, with room around what a test uses */
static uint8_t mem[8 * LINE] __aligned(LINE);

/* The driver's own, declared with the macros, each followed by an object the
 * linker is free to put in the room after it */
struct setup_packet {
    uint8_t request_type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
};

PB_DMA_BUFFER(struct setup_packet, setup_pkt);
static uint8_t after_setup_pkt __section(".no_init");
PB_DMA_ARRAY(uint32_t, descs, 5);
static uint8_t after_descs __section(".no_init");
PB_DMA_ARRAY(uint8_t, digest, LINE + 1);
static uint8_t after_digest __section(".no_init");
PB_DMA_ARRAY(uint8_t, two_lines, 2 * LINE);
static uint8_t after_two_lines __section(".no_init");
PB_DMA_ARRAY_ALIGNED(uint8_t, on_a_page, 100, 4096);
static uint8_t after_on_a_page __section(".no_init");

static int setup(void **state)
{
    (void)state;

    dcache_reset();
    memset(mem, 0, sizeof(mem));
    return 0;
}

static int teardown(void **state)
{
    (void)state;

    /* Every range that was begun is ended */
    assert_int_equal(dma_host_in_open(), 0);
    assert_int_equal(dcache_faults, 0);
    return 0;
}

/* drivers.md 4.1: the constant, which the model shares */
static void test_line(void **state)
{
    (void)state;

    assert_int_equal(PB_DMA_ALIGN, 64);
    assert_int_equal(DCACHE_LINE, PB_DMA_ALIGN);
    assert_int_equal(PB_DMA_LINES(0), 0);
    assert_int_equal(PB_DMA_LINES(1), 64);
    assert_int_equal(PB_DMA_LINES(64), 64);
    assert_int_equal(PB_DMA_LINES(65), 128);
}

static bool in_object(const void *p, const void *obj, size_t size)
{
    uintptr_t a = (uintptr_t)p;
    uintptr_t start = (uintptr_t)obj;

    return a >= start && a < start + PB_DMA_LINES(size);
}

/* drivers.md 4.4: an object declared with the macro has its own type and
 * size, starts on a line, and nothing else is in its lines */
static void test_declare(void **state)
{
    const void *others[] = { &after_setup_pkt, &after_descs, &after_digest, &after_two_lines,
                             &setup_pkt,       descs,        digest,        two_lines };

    (void)state;

    assert_int_equal(sizeof(setup_pkt), 8);
    assert_int_equal(sizeof(descs), 20);
    assert_int_equal(ARRAY_SIZE(descs), 5);
    assert_int_equal(sizeof(digest), LINE + 1);
    assert_int_equal(sizeof(two_lines), 2 * LINE);

    assert_true(dma_owns_lines(&setup_pkt, 0));
    assert_true(dma_owns_lines(descs, 0));
    assert_true(dma_owns_lines(digest, 0));
    assert_true(dma_owns_lines(two_lines, sizeof(two_lines)));

    /* With the device's own alignment, and still whole lines */
    assert_int_equal(sizeof(on_a_page), 100);
    assert_int_equal((uintptr_t)on_a_page % 4096, 0);
    assert_false(in_object(&after_on_a_page, on_a_page, sizeof(on_a_page)));
    after_on_a_page = 1;

    for (size_t i = 0; i < ARRAY_SIZE(others); i++) {
        if (others[i] != (const void *)&setup_pkt)
            assert_false(in_object(others[i], &setup_pkt, sizeof(setup_pkt)));
        if (others[i] != (const void *)descs)
            assert_false(in_object(others[i], descs, sizeof(descs)));
        if (others[i] != (const void *)digest)
            assert_false(in_object(others[i], digest, sizeof(digest)));
        if (others[i] != (const void *)two_lines)
            assert_false(in_object(others[i], two_lines, sizeof(two_lines)));
    }

    /* They are objects like any other */
    setup_pkt.length = 0x1234;
    descs[4] = 0xdeadbeef;
    digest[LINE] = 0x5a;
    after_setup_pkt = after_descs = after_digest = after_two_lines = 1;
    assert_int_equal(setup_pkt.length, 0x1234);
    assert_int_equal(descs[4], 0xdeadbeef);
    assert_int_equal(digest[LINE], 0x5a);
}

static void test_owns_lines(void **state)
{
    (void)state;

    assert_true(dma_owns_lines(mem, 0));
    assert_true(dma_owns_lines(mem, LINE));
    assert_true(dma_owns_lines(mem + LINE, 4 * LINE));
    assert_true(dma_owns_lines(mem, 512));
    assert_false(dma_owns_lines(mem + 1, 0));
    assert_false(dma_owns_lines(mem + 32, LINE));
    assert_false(dma_owns_lines(mem, LINE - 1));
    assert_false(dma_owns_lines(mem, 1));
}

/* 4.2, 1: after dma_out() the device reads what the CPU wrote, at any
 * alignment; what the CPU writes after it, the device doesn't see */
static void test_out(void **state)
{
    uint8_t dev[100];

    (void)state;

    memset(mem + 30, 0xa5, 100);
    dma_out(mem + 30, 100);
    dcache_dev_read((uintptr_t)mem + 30, dev, sizeof(dev));

    for (size_t i = 0; i < sizeof(dev); i++)
        assert_int_equal(dev[i], 0xa5);

    mem[30] = 0x11;
    assert_int_equal(dcache_dev_read_32((uintptr_t)mem + 30) & 0xff, 0xa5);
    dma_out(mem + 30, 1);
    assert_int_equal(dcache_dev_read_32((uintptr_t)mem + 30) & 0xff, 0x11);
}

/* Without the clean the device reads memory nobody handed to it. A length
 * of 0 is no clean */
static void test_out_missing(void **state)
{
    uint8_t dev[4];

    (void)state;

    memset(mem, 0xa5, LINE);
    dma_out(mem, 0);

    dcache_faults_expected = true;
    dcache_dev_read((uintptr_t)mem, dev, sizeof(dev));
    assert_int_equal(dcache_faults, 1);
    assert_string_equal(dcache_last_fault, "the device reads a line that was never cleaned");
    dcache_faults = 0;
}

/* 4.2, 2: begin, the device writes, end. Until the end the CPU has its old
 * bytes; after it the device's. The end is rounded up to a line */
static void test_in(void **state)
{
    uint8_t from_dev[100];

    (void)state;

    memset(from_dev, 0xd0, sizeof(from_dev));
    memset(mem, 0xee, 2 * LINE);

    dma_in_begin(mem, 100);
    assert_int_equal(dma_host_in_open(), 1);
    assert_false(dcache_is_dirty(mem, 2 * LINE));
    dcache_dev_write((uintptr_t)mem, from_dev, sizeof(from_dev));
    assert_int_equal(mem[0], 0xee);

    dma_in_end(mem, 100);
    assert_int_equal(dma_host_in_open(), 0);

    for (size_t i = 0; i < 100; i++)
        assert_int_equal(mem[i], 0xd0);

    /* What the device didn't write is what the CPU had */
    for (size_t i = 100; i < 2 * LINE; i++)
        assert_int_equal(mem[i], 0xee);
}

/* What the device both reads and writes is an In: it reads what the CPU
 * wrote before the begin */
static void test_in_device_reads_first(void **state)
{
    (void)state;

    descs[0] = 0x11223344;
    dma_in_begin(descs, sizeof(descs));
    assert_int_equal(dcache_dev_read_32((uintptr_t)descs), 0x11223344);
    dcache_dev_write_32((uintptr_t)descs, 0x55667788);
    dma_in_end(descs, sizeof(descs));
    assert_int_equal(descs[0], 0x55667788);
}

/* A driver that polls: end, read, begin again while the device isn't done */
static void test_in_polled(void **state)
{
    (void)state;

    descs[1] = 1;
    dma_in_begin(descs, sizeof(descs));

    for (int i = 0; i < 3; i++) {
        dma_in_end(descs, sizeof(descs));
        assert_int_equal(descs[1], 1);
        dma_in_begin(descs, sizeof(descs));
    }

    dcache_dev_write_32((uintptr_t)&descs[1], 0);
    dma_in_end(descs, sizeof(descs));
    assert_int_equal(descs[1], 0);
}

/* Without the end the driver reads its old bytes */
static void test_in_end_missing(void **state)
{
    (void)state;

    memset(mem, 0xee, LINE);
    dma_in_begin(mem, LINE);
    dcache_dev_write_32((uintptr_t)mem, 0x01020304);
    assert_int_equal(mem[0], 0xee);

    /* And the range is still begun: the suite's teardown would fail on it */
    assert_int_equal(dma_host_in_open(), 1);
    dma_in_end(mem, LINE);
}

/* 4.2, 3 and 4.4: a range a device writes starts on a line */
static void test_in_not_on_a_line(void **state)
{
    (void)state;

    expect_assert_failure(dma_in_begin(mem + 4, LINE));
    expect_assert_failure(dma_in_end(mem + 4, LINE));
    assert_int_equal(dma_host_in_open(), 0);
}

/* 4.4: every begin has its end, and only what was begun is ended */
static void test_in_pairs(void **state)
{
    (void)state;

    /* An end with no begin */
    expect_assert_failure(dma_in_end(mem, LINE));

    /* Another range than the one that was begun */
    dma_in_begin(mem, 2 * LINE);
    expect_assert_failure(dma_in_end(mem, LINE));
    expect_assert_failure(dma_in_end(mem + LINE, LINE));

    /* A begin in a range that is begun, and one that reaches into it */
    expect_assert_failure(dma_in_begin(mem, 2 * LINE));
    expect_assert_failure(dma_in_begin(mem + LINE, LINE));

    /* Beside it is another buffer */
    dma_in_begin(mem + 2 * LINE, 10);
    assert_int_equal(dma_host_in_open(), 2);
    dma_in_end(mem + 2 * LINE, 10);
    dma_in_end(mem, 2 * LINE);

    /* A length of 0 begins and ends nothing */
    dma_in_begin(mem, 0);
    assert_int_equal(dma_host_in_open(), 0);
    dma_in_end(mem, 0);
}

/* Nothing is cleaned in a range a device may be writing, nor in its last
 * line */
static void test_out_in_a_begun_range(void **state)
{
    (void)state;

    dma_in_begin(mem + LINE, 10);
    expect_assert_failure(dma_out(mem + LINE, 4));
    expect_assert_failure(dma_out(mem + LINE + 32, 4));
    expect_assert_failure(dma_out(mem + 60, 8));
    dma_out(mem, LINE);
    dma_out(mem + 2 * LINE, 1);
    dma_in_end(mem + LINE, 10);
}

/*
 * The model: what is silent on a board is a fault
 */

/* F-424: the device's entry is on a line with the driver's state. The line
 * is cleaned, the driver writes its state, and the device writes the entry
 * into a dirty line: whatever writes the line back puts the stale entry over
 * the device's */
static void test_model_device_writes_a_dirty_line(void **state)
{
    uint32_t *entry = (uint32_t *)mem;
    uint8_t *job_pending = mem + 8;

    (void)state;

    *entry = 0;
    dma_in_begin(entry, sizeof(*entry));
    *job_pending = 1;

    dcache_faults_expected = true;
    dcache_dev_write_32((uintptr_t)entry, 0x12345678);
    assert_int_equal(dcache_faults, 1);
    assert_string_equal(dcache_last_fault, "the device writes a line the CPU has dirty");

    /* The same when the line's write back comes after the device's write:
     * here the clean of a neighbour that the device reads */
    *job_pending = 0;
    dcache_dev_write_32((uintptr_t)entry, 0x12345678);
    assert_int_equal(dcache_faults, 1);
    *job_pending = 1;
    arch_clean_cache_range((uintptr_t)job_pending, 1);
    assert_int_equal(dcache_faults, 2);
    assert_string_equal(dcache_last_fault,
                        "a clean writes a dirty line back over what the device wrote in it");
    assert_int_equal(dcache_dev_read_32((uintptr_t)entry), 0);

    dma_in_end(entry, sizeof(*entry));
    dcache_faults = 0;
}

/* The entry on a line of its own: the same sequence is no fault */
static void test_model_device_writes_its_own_line(void **state)
{
    uint32_t *entry = (uint32_t *)mem;
    uint8_t *job_pending = mem + LINE;

    (void)state;

    *entry = 0;
    dma_in_begin(entry, sizeof(*entry));
    *job_pending = 1;
    dcache_dev_write_32((uintptr_t)entry, 0x12345678);
    dma_in_end(entry, sizeof(*entry));
    assert_int_equal(*entry, 0x12345678);
    assert_int_equal(*job_pending, 1);
}

/* F-081: a neighbour in the buffer's last line, written while the device has
 * the buffer, is lost with the invalidate */
static void test_model_invalidate_discards_a_neighbour(void **state)
{
    uint8_t *neighbour = mem + 16;

    (void)state;

    dma_in_begin(mem, 8);
    dcache_dev_write_32((uintptr_t)mem, 0x01020304);
    *neighbour = 0x77;

    dcache_faults_expected = true;
    dma_in_end(mem, 8);
    assert_int_equal(dcache_faults, 1);
    assert_string_equal(dcache_last_fault,
                        "an invalidate discards what the CPU wrote in the line and didn't clean");
    assert_int_equal(*neighbour, 0);
    dcache_faults = 0;
}

/* F-134, drivers.md 4.2, 4: a copy into the buffer before its invalidate is
 * discarded by it */
static void test_model_copy_before_invalidate(void **state)
{
    static const uint8_t bounce[4] = { 1, 2, 3, 4 };

    (void)state;

    dma_in_begin(mem, LINE);
    memcpy(mem, bounce, sizeof(bounce));

    dcache_faults_expected = true;
    dma_in_end(mem, LINE);
    assert_int_equal(dcache_faults, 1);
    assert_int_equal(mem[0], 0);
    dcache_faults = 0;
    dcache_faults_expected = false;

    /* The other order is the rule's */
    dma_in_begin(mem, LINE);
    dma_in_end(mem, LINE);
    memcpy(mem, bounce, sizeof(bounce));
    assert_int_equal(mem[0], 1);
}

/* A device that writes memory the driver didn't begin, cleaned or not, or
 * has ended */
static void test_model_device_writes_what_is_not_begun(void **state)
{
    (void)state;

    dcache_faults_expected = true;
    dcache_dev_write_32((uintptr_t)mem + 4 * LINE, 1);
    assert_int_equal(dcache_faults, 1);
    assert_string_equal(dcache_last_fault,
                        "the device writes a line that no dma_in_begin() has begun");

    dma_out(mem, LINE);
    dcache_dev_write_32((uintptr_t)mem, 1);
    assert_int_equal(dcache_faults, 2);

    dma_in_begin(mem + LINE, LINE);
    dma_in_end(mem + LINE, LINE);
    dcache_dev_write_32((uintptr_t)mem + LINE, 1);
    assert_int_equal(dcache_faults, 3);
    dcache_faults = 0;
}

/* Clean and invalidate: the CPU's bytes reach RAM, and the next read is
 * RAM's */
static void test_model_clean_invalidate(void **state)
{
    (void)state;

    mem[0] = 0x42;
    arch_clean_invalidate_cache_range((uintptr_t)mem, 1);
    assert_int_equal(dcache_dev_read_32((uintptr_t)mem) & 0xff, 0x42);
    dma_in_begin(mem, 1);
    dcache_dev_write_32((uintptr_t)mem, 0x43);
    assert_int_equal(mem[0], 0x42);
    assert_int_equal(dma_host_in_open(), 1);
    dma_host_reset();
    arch_clean_invalidate_cache_range((uintptr_t)mem, 1);
    assert_int_equal(mem[0], 0x43);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_line, setup, teardown),
        cmocka_unit_test_setup_teardown(test_declare, setup, teardown),
        cmocka_unit_test_setup_teardown(test_owns_lines, setup, teardown),
        cmocka_unit_test_setup_teardown(test_out, setup, teardown),
        cmocka_unit_test_setup_teardown(test_out_missing, setup, teardown),
        cmocka_unit_test_setup_teardown(test_in, setup, teardown),
        cmocka_unit_test_setup_teardown(test_in_device_reads_first, setup, teardown),
        cmocka_unit_test_setup_teardown(test_in_polled, setup, teardown),
        cmocka_unit_test_setup_teardown(test_in_end_missing, setup, teardown),
        cmocka_unit_test_setup_teardown(test_in_not_on_a_line, setup, teardown),
        cmocka_unit_test_setup_teardown(test_in_pairs, setup, teardown),
        cmocka_unit_test_setup_teardown(test_out_in_a_begun_range, setup, teardown),
        cmocka_unit_test_setup_teardown(test_model_device_writes_a_dirty_line, setup, teardown),
        cmocka_unit_test_setup_teardown(test_model_device_writes_its_own_line, setup, teardown),
        cmocka_unit_test_setup_teardown(test_model_invalidate_discards_a_neighbour, setup, teardown),
        cmocka_unit_test_setup_teardown(test_model_copy_before_invalidate, setup, teardown),
        cmocka_unit_test_setup_teardown(test_model_device_writes_what_is_not_begun, setup, teardown),
        cmocka_unit_test_setup_teardown(test_model_clean_invalidate, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
