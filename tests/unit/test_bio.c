/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <pb/bio.h>
#include <pb/dma.h>
#include <pb/pb.h>
#include <stdlib.h>
#include <string.h>

#include "fakes/ramdisk.h"

#define DISK_BLOCKS 65536
#define PART_FIRST  34
#define PART_LAST   1057

#define UU_DISK \
    (const unsigned char *)"\x1e\xac\xed\xf3\x37\x90\x48\xc7\x8e\xd8\x91\x88\xff\x49\x67\x2b"
#define UU_PART \
    (const unsigned char *)"\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62\x01\x4c\xfa\x73\x5c\xe0"
#define UU_OTHER \
    (const unsigned char *)"\xc0\x46\xcc\xd8\x0f\x2e\x40\x36\x98\x4d\x76\xc1\x4d\xc7\x39\x92"

struct fixture {
    struct ramdisk *disk;
    bio_dev_t part;
    /* A buffer a device may write: it owns its cache lines (bio.md 3.1) */
    uint8_t buf[4 * RAMDISK_BLOCK_SIZE] __aligned(PB_DMA_ALIGN);
};

static int setup(void **state)
{
    /* Static: an object from calloc() starts on 16 bytes, not on a line */
    static struct fixture fixture;
    struct fixture *f = &fixture;

    memset(f, 0, sizeof(*f));
    f->disk = ramdisk_new(DISK_BLOCKS);
    assert_non_null(f->disk);

    bio_reset();
    bio_dev_t dev = ramdisk_attach(f->disk, UU_DISK);
    assert_true(dev >= 0);
    f->part = bio_allocate_parent(dev, PART_FIRST, PART_LAST, RAMDISK_BLOCK_SIZE, UU_PART, "Part");
    assert_true(f->part >= 0);

    memset(f->buf, 0xa5, sizeof(f->buf));
    *state = f;
    return 0;
}

static int teardown(void **state)
{
    struct fixture *f = *state;
    ramdisk_free(f->disk);
    return 0;
}

static void test_access_is_relative_to_partition(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(bio_write(f->part, 0, RAMDISK_BLOCK_SIZE, f->buf), PB_OK);
    assert_memory_equal(ramdisk_block(f->disk, PART_FIRST), f->buf, RAMDISK_BLOCK_SIZE);
}

static void test_last_block_ok(void **state)
{
    struct fixture *f = *state;
    lba_t last = PART_LAST - PART_FIRST;

    assert_int_equal(bio_write(f->part, last, RAMDISK_BLOCK_SIZE, f->buf), PB_OK);
    assert_int_equal(bio_read(f->part, last, RAMDISK_BLOCK_SIZE, f->buf), PB_OK);
}

static void test_past_end_rejected(void **state)
{
    struct fixture *f = *state;
    lba_t last = PART_LAST - PART_FIRST;

    assert_int_equal(bio_write(f->part, last + 1, RAMDISK_BLOCK_SIZE, f->buf), -PB_ERR_PARAM);
    assert_int_equal(bio_read(f->part, last + 1, RAMDISK_BLOCK_SIZE, f->buf), -PB_ERR_PARAM);
    /* Starts inside, ends outside */
    assert_int_equal(bio_write(f->part, last, 2 * RAMDISK_BLOCK_SIZE, f->buf), -PB_ERR_PARAM);
    /* A partial block at the end counts as a whole block */
    assert_int_equal(bio_write(f->part, last, RAMDISK_BLOCK_SIZE + 1, f->buf), -PB_ERR_PARAM);
    assert_int_equal(f->disk->writes, 0);
    assert_int_equal(f->disk->reads, 0);
}

/* F-054: first_lba + lba must not wrap around to a valid block */
static void test_lba_wrap_rejected(void **state)
{
    struct fixture *f = *state;
    lba_t wrap_to_zero = (lba_t)(0 - PART_FIRST);

    assert_int_equal(bio_write(f->part, wrap_to_zero, RAMDISK_BLOCK_SIZE, f->buf), -PB_ERR_PARAM);
    assert_int_equal(bio_read(f->part, wrap_to_zero + 1, RAMDISK_BLOCK_SIZE, f->buf),
                     -PB_ERR_PARAM);
    assert_int_equal(bio_write(f->part, UINT32_MAX, RAMDISK_BLOCK_SIZE, f->buf), -PB_ERR_PARAM);
    assert_int_equal(f->disk->writes, 0);
    assert_int_equal(f->disk->reads, 0);
}

static void test_huge_length_rejected(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(bio_read(f->part, 0, SIZE_MAX, f->buf), -PB_ERR_PARAM);
    assert_int_equal(bio_read(f->part, 1, SIZE_MAX - RAMDISK_BLOCK_SIZE + 2, f->buf),
                     -PB_ERR_PARAM);
    assert_int_equal(f->disk->reads, 0);
}

static void assert_all(const uint8_t *buf, size_t len, uint8_t value)
{
    for (size_t i = 0; i < len; i++) {
        if (buf[i] != value)
            fail_msg("Byte %zu of %zu is 0x%02x, not 0x%02x", i, len, buf[i], value);
    }
}

/* bio.md 3.2 (F-182): a length that isn't whole blocks is the core's. The
 * driver gets whole blocks, which the RAM disk insists on, and the last
 * block is the bytes given, then zeros. In the partition's blocks */
static void test_a_write_that_isnt_whole_blocks_is_padded(void **state)
{
    struct fixture *f = *state;
    const size_t len = RAMDISK_BLOCK_SIZE + 100;

    memset(ramdisk_block(f->disk, PART_FIRST - 1), 0xff, 5 * RAMDISK_BLOCK_SIZE);

    assert_int_equal(bio_write(f->part, 1, len, f->buf), PB_OK);
    /* The whole block, then the last one */
    assert_int_equal(f->disk->writes, 2);
    assert_all(ramdisk_block(f->disk, PART_FIRST - 1), 2 * RAMDISK_BLOCK_SIZE, 0xff);
    assert_memory_equal(ramdisk_block(f->disk, PART_FIRST + 1), f->buf, len);
    assert_all(ramdisk_block(f->disk, PART_FIRST + 2) + 100, RAMDISK_BLOCK_SIZE - 100, 0);
    assert_all(ramdisk_block(f->disk, PART_FIRST + 3), RAMDISK_BLOCK_SIZE, 0xff);

    /* Less than a block: one call */
    assert_int_equal(bio_write(f->part, 0, 1, f->buf), PB_OK);
    assert_int_equal(f->disk->writes, 3);
    assert_int_equal(*ramdisk_block(f->disk, PART_FIRST), 0xa5);
    assert_all(ramdisk_block(f->disk, PART_FIRST) + 1, RAMDISK_BLOCK_SIZE - 1, 0);

    /* A failure of either call is the write's */
    f->disk->fail_writes_after = 1;
    assert_int_equal(bio_write(f->part, 1, len, f->buf), -PB_ERR_IO);
    f->disk->fail_writes_after = 0;
    assert_int_equal(bio_write(f->part, 1, len, f->buf), -PB_ERR_IO);
}

/* bio.md 3.1 (F-182): the last block is read into the core's block buffer,
 * and exactly 'length' bytes are written to the caller's */
static void test_a_read_that_isnt_whole_blocks_is_trimmed(void **state)
{
    struct fixture *f = *state;
    const size_t len = RAMDISK_BLOCK_SIZE + 100;
    uint8_t *medium = ramdisk_block(f->disk, PART_FIRST + 1);

    for (size_t i = 0; i < 3 * RAMDISK_BLOCK_SIZE; i++)
        medium[i] = (uint8_t)(i * 3 + 1);

    assert_int_equal(bio_read(f->part, 1, len, f->buf), PB_OK);
    assert_int_equal(f->disk->reads, 2);
    assert_memory_equal(f->buf, medium, len);
    assert_all(f->buf + len, sizeof(f->buf) - len, 0xa5);

    memset(f->buf, 0xa5, sizeof(f->buf));
    assert_int_equal(bio_read(f->part, 2, 1, f->buf), PB_OK);
    assert_int_equal(f->disk->reads, 3);
    assert_int_equal(f->buf[0], medium[RAMDISK_BLOCK_SIZE]);
    assert_all(f->buf + 1, sizeof(f->buf) - 1, 0xa5);

    /* A failure of either call is the read's */
    f->disk->fail_reads_after = 1;
    assert_int_equal(bio_read(f->part, 1, len, f->buf), -PB_ERR_IO);
    f->disk->fail_reads_after = 0;
    assert_int_equal(bio_read(f->part, 1, len, f->buf), -PB_ERR_IO);
}

/* bio.md 3.1, 3.2: a length of 0 is PB_OK, nothing is read or written and
 * the driver isn't called */
static void test_a_length_of_0_doesnt_reach_the_driver(void **state)
{
    struct fixture *f = *state;
    uint8_t before[sizeof(f->buf)];

    memset(ramdisk_block(f->disk, PART_FIRST), 0x3c, RAMDISK_BLOCK_SIZE);
    memcpy(before, f->buf, sizeof(before));

    assert_int_equal(bio_read(f->part, 0, 0, f->buf), PB_OK);
    assert_int_equal(f->disk->reads, 0);
    assert_memory_equal(f->buf, before, sizeof(before));

    assert_int_equal(bio_write(f->part, 0, 0, f->buf), PB_OK);
    assert_int_equal(f->disk->writes, 0);
    assert_int_equal(*ramdisk_block(f->disk, PART_FIRST), 0x3c);
}

/* bio.md 3.1, drivers.md 4.3: what a device writes owns its cache lines. A
 * buffer that doesn't start on one is refused before the driver is called,
 * also for a read that isn't whole blocks; a write reads the buffer only,
 * and takes any (drivers.md 4.3, out) */
static void test_a_read_into_a_buffer_off_a_line_is_refused(void **state)
{
    struct fixture *f = *state;
    static const size_t shifts[] = { 1, 4, 8, 32, PB_DMA_ALIGN - 1 };
    static const size_t lengths[] = { 1, RAMDISK_BLOCK_SIZE, RAMDISK_BLOCK_SIZE + 100 };
    uint8_t before[sizeof(f->buf)];

    memcpy(before, f->buf, sizeof(before));

    for (size_t i = 0; i < ARRAY_SIZE(shifts); i++) {
        for (size_t j = 0; j < ARRAY_SIZE(lengths); j++) {
            assert_int_equal(bio_read(f->part, 1, lengths[j], f->buf + shifts[i]), -PB_ERR_ALIGN);
        }
    }

    assert_int_equal(f->disk->reads, 0);
    assert_memory_equal(f->buf, before, sizeof(before));

    /* The arguments first, and a length of 0 reads nothing, as before */
    assert_int_equal(bio_read(f->part, PART_LAST, RAMDISK_BLOCK_SIZE, f->buf + 1), -PB_ERR_PARAM);
    assert_int_equal(bio_read(f->part, 1, 0, f->buf + 1), PB_OK);

    /* On a line it is read: the whole block, then the tail through the block
     * buffer */
    assert_int_equal(bio_read(f->part, 1, RAMDISK_BLOCK_SIZE + 100, f->buf + PB_DMA_ALIGN), PB_OK);
    assert_int_equal(f->disk->reads, 2);

    /* A write takes a buffer anywhere */
    assert_int_equal(bio_write(f->part, 1, RAMDISK_BLOCK_SIZE, f->buf + 1), PB_OK);
    assert_memory_equal(ramdisk_block(f->disk, PART_FIRST + 1), f->buf + 1, RAMDISK_BLOCK_SIZE);
}

static void test_invalid_handles(void **state)
{
    struct fixture *f = *state;
    const bio_dev_t bad[] = { -1, CONFIG_BIO_MAX_DEVS, f->part + 1 };

    for (size_t i = 0; i < ARRAY_SIZE(bad); i++) {
        bio_dev_t dev = bad[i];

        assert_false(bio_valid(dev));
        assert_int_equal(bio_set_ios(dev, NULL, NULL), -PB_ERR_PARAM);
        assert_int_equal(bio_set_ios_erase(dev, NULL), -PB_ERR_PARAM);
        assert_int_equal(bio_set_private(dev, 1), -PB_ERR_PARAM);
        assert_int_equal(bio_get_private(dev), 0);
        assert_int_equal(bio_size(dev), -PB_ERR_PARAM);
        assert_int_equal(bio_block_size(dev), -PB_ERR_PARAM);
        assert_int_equal(bio_read(dev, 0, RAMDISK_BLOCK_SIZE, f->buf), -PB_ERR_PARAM);
        assert_int_equal(bio_write(dev, 0, RAMDISK_BLOCK_SIZE, f->buf), -PB_ERR_PARAM);
        assert_int_equal(bio_erase(dev, 0, 1), -PB_ERR_PARAM);
        assert_int_equal(bio_get_hal_flags(dev), -PB_ERR_PARAM);
        assert_int_equal(bio_set_hal_flags(dev, 1), -PB_ERR_PARAM);
        assert_int_equal(bio_get_flags(dev), -PB_ERR_PARAM);
        assert_int_equal(bio_set_flags(dev, 1), -PB_ERR_PARAM);
        assert_int_equal(bio_clear_set_flags(dev, 0, 1), -PB_ERR_PARAM);
        assert_null(bio_get_description(dev));
        assert_null(bio_get_uu(dev));
        assert_int_equal(bio_get_last_block(dev), -PB_ERR_PARAM);
        assert_int_equal(bio_get_first_block(dev), -PB_ERR_PARAM);
        assert_int_equal(bio_get_no_of_blocks(dev), -PB_ERR_PARAM);
        assert_int_equal(bio_set_install_partition_cb(dev, NULL), -PB_ERR_PARAM);
        assert_int_equal(bio_set_write_done_cb(dev, NULL), -PB_ERR_PARAM);
        assert_int_equal(bio_write_done(dev), -PB_ERR_PARAM);
        assert_int_equal(bio_allocate_parent(dev, 0, 1, RAMDISK_BLOCK_SIZE, UU_PART, "x"),
                         -PB_ERR_PARAM);
    }
}

static void test_allocate_rejects(void **state)
{
    (void)state;

    assert_int_equal(bio_allocate(0, 1, 0, UU_OTHER, "x"), -PB_ERR_PARAM);
    assert_int_equal(bio_allocate(2, 1, RAMDISK_BLOCK_SIZE, UU_OTHER, "x"), -PB_ERR_PARAM);
}

/* A UUID of its own for each device a test adds (bio.md 2.6) */
static const unsigned char *filler_uu(int i)
{
    static uuid_t uu;

    memset(uu, 0xf1, sizeof(uu));
    uu[0] = (unsigned char)i;
    uu[1] = (unsigned char)(i >> 8);
    return uu;
}

/* bio.md 2.6 (F-283): a UUID names at most one device. A second device with
 * a UUID that is taken is refused, a partition's too */
static void test_a_uuid_that_is_taken_is_refused(void **state)
{
    struct fixture *f = *state;
    bio_dev_t dev;

    assert_int_equal(bio_allocate(0, 1, RAMDISK_BLOCK_SIZE, UU_DISK, "x"), -PB_ERR_PARAM);
    assert_int_equal(bio_allocate(0, 1, RAMDISK_BLOCK_SIZE, UU_PART, "x"), -PB_ERR_PARAM);
    dev = bio_allocate_parent(0, PART_LAST + 1, PART_LAST + 2, RAMDISK_BLOCK_SIZE, UU_PART, "x");
    assert_int_equal(dev, -PB_ERR_PARAM);
    dev = bio_allocate_parent(0, PART_LAST + 1, PART_LAST + 2, RAMDISK_BLOCK_SIZE, UU_DISK, "x");
    assert_int_equal(dev, -PB_ERR_PARAM);

    /* Nothing was registered, and the UUIDs name what they named */
    assert_false(bio_valid(f->part + 1));
    assert_int_equal(bio_get_part_by_uu(UU_DISK), 0);
    assert_int_equal(bio_get_part_by_uu(UU_PART), f->part);
    assert_int_equal(bio_get_first_block(f->part), PART_FIRST);

    assert_int_equal(bio_allocate(0, 1, RAMDISK_BLOCK_SIZE, UU_OTHER, "x"), f->part + 1);
    assert_int_equal(bio_allocate(0, 1, RAMDISK_BLOCK_SIZE, UU_OTHER, "x"), -PB_ERR_PARAM);
}

/* bio.md 2.3: the largest block is CONFIG_BIO_MAX_BLOCK_SIZE, and a device
 * with a larger one is refused at registration */
static void test_a_block_larger_than_the_largest_is_refused(void **state)
{
    struct fixture *f = *state;
    bio_dev_t dev;

    assert_int_equal(bio_allocate(0, 1, CONFIG_BIO_MAX_BLOCK_SIZE + 1, UU_OTHER, "x"),
                     -PB_ERR_PARAM);
    assert_int_equal(bio_allocate(0, 1, 2 * CONFIG_BIO_MAX_BLOCK_SIZE, UU_OTHER, "x"),
                     -PB_ERR_PARAM);
    assert_int_equal(bio_allocate_parent(0, 0, 1, CONFIG_BIO_MAX_BLOCK_SIZE + 1, UU_OTHER, "x"),
                     -PB_ERR_PARAM);
    /* Nothing was registered */
    assert_int_equal(bio_get_part_by_uu(UU_OTHER), -PB_ERR_NOT_FOUND);
    assert_false(bio_valid(f->part + 1));

    dev = bio_allocate(0, 1, CONFIG_BIO_MAX_BLOCK_SIZE, UU_OTHER, "x");
    assert_int_equal(dev, f->part + 1);
    assert_int_equal(bio_block_size(dev), CONFIG_BIO_MAX_BLOCK_SIZE);
}

static void test_allocate_pool_full(void **state)
{
    struct fixture *f = *state;
    bio_dev_t dev;

    /* The fixture uses two devices */
    for (int i = 2; i < CONFIG_BIO_MAX_DEVS; i++)
        assert_int_equal(bio_allocate(0, 1, RAMDISK_BLOCK_SIZE, filler_uu(i), "x"), i);

    assert_int_equal(bio_allocate(0, 1, RAMDISK_BLOCK_SIZE, UU_OTHER, "x"), -PB_ERR_MEM);
    dev =
        bio_allocate_parent(f->part, PART_FIRST, PART_FIRST + 1, RAMDISK_BLOCK_SIZE, UU_OTHER, "x");
    assert_int_equal(dev, -PB_ERR_MEM);
}

/* bio.md 2.2 (F-193): a partition lies inside its parent, and the core
 * checks it. The blocks are the parent's, as the driver counts them */
static void test_a_partition_lies_inside_its_parent(void **state)
{
    struct fixture *f = *state;

    const struct {
        bio_dev_t parent;
        lba_t first;
        lba_t last;
    } outside[] = {
        { 0, DISK_BLOCKS - 1, DISK_BLOCKS }, /* Ends after the device */
        { 0, DISK_BLOCKS, DISK_BLOCKS + 10 }, /* All of it after the device */
        { 0, 0, UINT32_MAX },
        { f->part, PART_FIRST - 1, PART_FIRST + 10 }, /* Starts before a partition */
        { f->part, PART_LAST, PART_LAST + 1 }, /* Ends after it */
        { f->part, 0, 1 }, /* On the device, not in the partition */
    };

    bio_dev_t dev;

    for (size_t i = 0; i < ARRAY_SIZE(outside); i++) {
        dev = bio_allocate_parent(outside[i].parent,
                                  outside[i].first,
                                  outside[i].last,
                                  RAMDISK_BLOCK_SIZE,
                                  UU_OTHER,
                                  "Outside");
        assert_int_equal(dev, -PB_ERR_PARAM);
    }

    /* Nothing was registered */
    assert_int_equal(bio_get_part_by_uu(UU_OTHER), -PB_ERR_NOT_FOUND);
    assert_false(bio_valid(f->part + 1));

    /* The parent's first and last block are inside */
    dev = bio_allocate_parent(f->part, PART_FIRST, PART_LAST, RAMDISK_BLOCK_SIZE, UU_OTHER, "All");
    assert_int_equal(dev, f->part + 1);
    dev = bio_allocate_parent(
        0, DISK_BLOCKS - 1, DISK_BLOCKS - 1, RAMDISK_BLOCK_SIZE, filler_uu(0), "Last");
    assert_int_equal(dev, f->part + 2);
}

static void test_properties(void **state)
{
    struct fixture *f = *state;
    const int blocks = PART_LAST - PART_FIRST + 1;

    assert_true(bio_valid(f->part));
    assert_int_equal(bio_size(f->part), (int64_t)blocks * RAMDISK_BLOCK_SIZE);
    assert_int_equal(bio_block_size(f->part), RAMDISK_BLOCK_SIZE);
    assert_int_equal(bio_get_first_block(f->part), PART_FIRST);
    assert_int_equal(bio_get_last_block(f->part), PART_LAST);
    assert_int_equal(bio_get_no_of_blocks(f->part), blocks);
    assert_string_equal(bio_get_description(f->part), "Part");
    assert_memory_equal(bio_get_uu(f->part), UU_PART, 16);
    /* The partition inherits the parent's private data and flags */
    assert_int_equal(bio_get_private(f->part), (uintptr_t)f->disk);
    assert_int_equal(bio_get_flags(f->part), BIO_FLAG_VISIBLE | BIO_FLAG_WRITABLE);
}

/* bio.md 2.6 (F-283): LBAs are 32 bits, and a device of 2^31 blocks or more
 * is a device: the block getters answer its blocks, not an error */
static void test_block_getters_of_a_large_device(void **state)
{
    bio_dev_t half, most, all;
    (void)state;

    half = bio_allocate(0, 0x7fffffff, RAMDISK_BLOCK_SIZE, filler_uu(0), "2^31 blocks");
    most = bio_allocate(0x80000000, 0xfffffffe, RAMDISK_BLOCK_SIZE, filler_uu(1), "Upper half");
    all = bio_allocate(0, UINT32_MAX, RAMDISK_BLOCK_SIZE, filler_uu(2), "2^32 blocks");
    assert_true(half >= 0 && most >= 0 && all >= 0);

    assert_true(bio_get_first_block(half) == 0);
    assert_true(bio_get_last_block(half) == INT64_C(0x7fffffff));
    assert_true(bio_get_no_of_blocks(half) == INT64_C(0x80000000));

    assert_true(bio_get_first_block(most) == INT64_C(0x80000000));
    assert_true(bio_get_last_block(most) == INT64_C(0xfffffffe));
    assert_true(bio_get_no_of_blocks(most) == INT64_C(0x7fffffff));

    assert_true(bio_get_first_block(all) == 0);
    assert_true(bio_get_last_block(all) == INT64_C(0xffffffff));
    assert_true(bio_get_no_of_blocks(all) == INT64_C(0x100000000));
    /* 2 TiB of 512 byte blocks (bio.md 2.1) */
    assert_true(bio_size(all) == INT64_C(0x100000000) * RAMDISK_BLOCK_SIZE);
    assert_true(bio_size(half) == INT64_C(0x80000000) * RAMDISK_BLOCK_SIZE);
}

static void test_flags(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(bio_set_hal_flags(f->part, 0xa5), PB_OK);
    assert_int_equal(bio_get_hal_flags(f->part), 0xa5);
    /* HAL flags and generic flags are separate */
    assert_int_equal(bio_set_flags(f->part, BIO_FLAG_BOOTABLE), PB_OK);
    assert_int_equal(bio_get_flags(f->part), BIO_FLAG_BOOTABLE);
    assert_int_equal(bio_get_hal_flags(f->part), 0xa5);

    assert_int_equal(bio_clear_set_flags(f->part, BIO_FLAG_BOOTABLE, BIO_FLAG_READABLE), PB_OK);
    assert_int_equal(bio_get_flags(f->part), BIO_FLAG_READABLE);

    assert_int_equal(bio_set_hal_flags(f->part, 0), PB_OK);
    assert_int_equal(bio_get_hal_flags(f->part), 0);
    assert_int_equal(bio_get_flags(f->part), BIO_FLAG_READABLE);
}

static void test_missing_ops(void **state)
{
    struct fixture *f = *state;
    bio_dev_t dev = bio_allocate(0, 10, RAMDISK_BLOCK_SIZE, UU_OTHER, "No ops");

    assert_true(dev >= 0);
    assert_int_equal(bio_read(dev, 0, RAMDISK_BLOCK_SIZE, f->buf), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(bio_write(dev, 0, RAMDISK_BLOCK_SIZE, f->buf), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(bio_erase(dev, 0, 1), -PB_ERR_NOT_SUPPORTED);
    /* bio.md 3.4: a device without the callback answers PB_OK */
    assert_int_equal(bio_write_done(dev), PB_OK);
}

static int write_done_calls;

static int fake_write_done(bio_dev_t dev, int param)
{
    (void)dev;
    (void)param;
    write_done_calls++;
    return -PB_ERR_IO;
}

/* bio.md 2.2, 3.4: write_done is the device's, a partition doesn't get it,
 * and the device's callback's answer is the caller's */
static void test_write_done_isnt_inherited(void **state)
{
    struct fixture *f = *state;
    bio_dev_t disk = bio_get_part_by_uu(UU_DISK);
    bio_dev_t part;

    assert_true(disk >= 0);
    write_done_calls = 0;
    assert_int_equal(bio_set_write_done_cb(disk, fake_write_done), PB_OK);
    part = bio_allocate_parent(disk, 2, 9, RAMDISK_BLOCK_SIZE, UU_OTHER, "Other");
    assert_true(part >= 0);

    assert_int_equal(bio_write_done(part), PB_OK);
    assert_int_equal(bio_write_done(f->part), PB_OK);
    assert_int_equal(write_done_calls, 0);

    assert_int_equal(bio_write_done(disk), -PB_ERR_IO);
    assert_int_equal(write_done_calls, 1);
}

static int erase_calls;
static lba_t erase_lba;
static size_t erase_count;

static int fake_erase(bio_dev_t dev, lba_t first_lba, size_t count)
{
    (void)dev;
    erase_calls++;
    erase_lba = first_lba;
    erase_count = count;
    return PB_OK;
}

/* F-055: the LBA is relative to the partition, like read and write, and the
 * range is checked */
static void test_erase_calls_the_driver(void **state)
{
    struct fixture *f = *state;
    lba_t last = PART_LAST - PART_FIRST;

    erase_calls = 0;
    assert_int_equal(bio_set_ios_erase(f->part, fake_erase), PB_OK);
    assert_int_equal(bio_erase(f->part, 3, 7), PB_OK);
    assert_int_equal(erase_calls, 1);
    assert_int_equal(erase_lba, PART_FIRST + 3);
    assert_int_equal(erase_count, 7);

    assert_int_equal(bio_erase(f->part, last, 1), PB_OK);
    assert_int_equal(erase_lba, PART_LAST);
    assert_int_equal(bio_erase(f->part, last, 2), -PB_ERR_PARAM);
    assert_int_equal(bio_erase(f->part, last + 1, 1), -PB_ERR_PARAM);
    assert_int_equal(bio_erase(f->part, (lba_t)(0 - PART_FIRST), 1), -PB_ERR_PARAM);
    assert_int_equal(bio_erase(f->part, 0, SIZE_MAX), -PB_ERR_PARAM);
    assert_int_equal(erase_calls, 2);

    /* Nothing to erase */
    assert_int_equal(bio_erase(f->part, 0, 0), PB_OK);
    assert_int_equal(erase_calls, 2);
}

static void test_set_private(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(bio_set_private(f->part, 0x1234), PB_OK);
    assert_int_equal(bio_get_private(f->part), 0x1234);
}

static void test_get_part_by_uu(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(bio_get_part_by_uu(UU_PART), f->part);
    assert_int_equal(bio_get_part_by_uu(UU_DISK), 0);
    assert_int_equal(bio_get_part_by_uu(NULL), -PB_ERR_NOT_FOUND);
    assert_int_equal(bio_get_part_by_uu(UU_OTHER), -PB_ERR_NOT_FOUND);

    assert_int_equal(bio_get_part_by_uu_str("2af755d8-8de5-45d5-a862-014cfa735ce0"), f->part);
    assert_int_equal(bio_get_part_by_uu_str("not a uuid"), -PB_ERR_PARAM);
}

static int install_calls;
static int install_variant;

static int fake_install(bio_dev_t dev, int variant)
{
    (void)dev;
    install_calls++;
    install_variant = variant;
    return PB_OK;
}

static void test_install_partition_table(void **state)
{
    (void)state;

    install_calls = 0;
    assert_int_equal(bio_install_partition_table((unsigned char *)UU_OTHER, 0), -PB_ERR_NOT_FOUND);
    assert_int_equal(bio_install_partition_table((unsigned char *)UU_DISK, 0),
                     -PB_ERR_NOT_SUPPORTED);

    assert_int_equal(bio_set_install_partition_cb(0, fake_install), PB_OK);
    assert_int_equal(bio_install_partition_table((unsigned char *)UU_DISK, 2), PB_OK);
    assert_int_equal(install_calls, 1);
    assert_int_equal(install_variant, 2);
}

#define UU_NEW \
    (const unsigned char *)"\xf5\xf8\xc9\xae\xef\xb5\x40\x71\x9b\xa9\xd3\x13\xb0\x82\x28\x1e"
#define UU_NEW2 \
    (const unsigned char *)"\x65\x6a\xb3\xfc\x58\x56\x4a\x5e\xa2\xae\x5a\x01\x83\x13\xb3\xee"
#define DISK_FLAGS (BIO_FLAG_VISIBLE | BIO_FLAG_WRITABLE)

/*
 * A table an install changed (bio.md 4, D-093): the partitions stay until
 * the reset and refuse writes and erases, before the driver is called. The
 * device the table is on, and other devices, are written as before.
 */
static void test_a_changed_table_refuses_writes(void **state)
{
    struct fixture *f = *state;
    struct ramdisk *other = ramdisk_new(DISK_BLOCKS);
    bio_dev_t other_dev;
    bio_dev_t other_part;
    unsigned int writes;

    assert_non_null(other);
    other_dev = ramdisk_attach(other, UU_OTHER);
    assert_true(other_dev >= 0);
    other_part = bio_allocate_parent(
        other_dev, PART_FIRST, PART_LAST, RAMDISK_BLOCK_SIZE, UU_NEW, "Other part");
    assert_true(other_part >= 0);

    erase_calls = 0;
    assert_int_equal(bio_set_ios_erase(f->part, fake_erase), PB_OK);
    assert_int_equal(bio_set_table_changed(-1), -PB_ERR_PARAM);
    assert_int_equal(bio_set_table_changed(CONFIG_BIO_MAX_DEVS), -PB_ERR_PARAM);
    assert_int_equal(bio_set_table_changed(0), PB_OK);
    /* Twice is once */
    assert_int_equal(bio_set_table_changed(0), PB_OK);

    writes = f->disk->writes;
    assert_int_equal(bio_write(f->part, 0, RAMDISK_BLOCK_SIZE, f->buf), -PB_ERR_STATE);
    assert_int_equal(bio_erase(f->part, 0, 1), -PB_ERR_STATE);
    assert_int_equal(f->disk->writes, writes);
    assert_int_equal(erase_calls, 0);
    /* A range outside the partition is still that, and nothing is nothing */
    assert_int_equal(bio_erase(f->part, PART_LAST, 1), -PB_ERR_PARAM);
    assert_int_equal(bio_write(f->part, 0, 0, f->buf), PB_OK);
    assert_int_equal(bio_erase(f->part, 0, 0), PB_OK);

    /* The partition is as it was: found, read, its flags */
    assert_int_equal(bio_get_part_by_uu(UU_PART), f->part);
    assert_true(bio_valid(f->part));
    assert_int_equal(bio_read(f->part, 0, RAMDISK_BLOCK_SIZE, f->buf), PB_OK);
    assert_int_equal(bio_set_flags(f->part, BIO_FLAG_VISIBLE), PB_OK);

    /* The device, where the table is, and the other device are written */
    assert_int_equal(bio_write(0, 1, RAMDISK_BLOCK_SIZE, f->buf), PB_OK);
    assert_int_equal(f->disk->writes, writes + 1);
    assert_int_equal(bio_write(other_dev, 1, RAMDISK_BLOCK_SIZE, f->buf), PB_OK);
    assert_int_equal(bio_write(other_part, 0, RAMDISK_BLOCK_SIZE, f->buf), PB_OK);

    /* A partition made after it is the device's too */
    bio_dev_t late =
        bio_allocate_parent(0, PART_LAST + 1, PART_LAST + 10, RAMDISK_BLOCK_SIZE, UU_NEW2, "Late");
    assert_true(late >= 0);
    assert_int_equal(bio_write(late, 0, RAMDISK_BLOCK_SIZE, f->buf), -PB_ERR_STATE);

    /* The reset ends it */
    bio_reset();
    bio_dev_t dev = ramdisk_attach(f->disk, UU_DISK);
    bio_dev_t part =
        bio_allocate_parent(dev, PART_FIRST, PART_LAST, RAMDISK_BLOCK_SIZE, UU_PART, "Part");
    assert_int_equal(bio_write(part, 0, RAMDISK_BLOCK_SIZE, f->buf), PB_OK);

    ramdisk_free(other);
}

/* An install whose table changes nothing doesn't lock: the callback didn't
 * say so. One that did locks also when it failed after it said so */
static int changing_install_rc;

static int changing_install(bio_dev_t dev, int variant)
{
    (void)variant;
    assert_int_equal(bio_set_table_changed(dev), PB_OK);
    return changing_install_rc;
}

static void test_an_install_locks_only_when_its_table_changed(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(bio_set_install_partition_cb(0, fake_install), PB_OK);
    assert_int_equal(bio_install_partition_table(UU_DISK, 0), PB_OK);
    assert_int_equal(bio_write(f->part, 0, RAMDISK_BLOCK_SIZE, f->buf), PB_OK);

    changing_install_rc = -PB_ERR_IO;
    assert_int_equal(bio_set_install_partition_cb(0, changing_install), PB_OK);
    assert_int_equal(bio_install_partition_table(UU_DISK, 1), -PB_ERR_IO);
    assert_int_equal(bio_write(f->part, 0, RAMDISK_BLOCK_SIZE, f->buf), -PB_ERR_STATE);

    /* A later install that changes nothing doesn't lift it */
    assert_int_equal(bio_set_install_partition_cb(0, fake_install), PB_OK);
    assert_int_equal(bio_install_partition_table(UU_DISK, 0), PB_OK);
    assert_int_equal(bio_write(f->part, 0, RAMDISK_BLOCK_SIZE, f->buf), -PB_ERR_STATE);
}

/* bio.md 2.4, command-mode.md 5.3 (F-083): the board's readable list. A
 * device that exists gets READABLE when the list is given, a table's
 * partition as it is made. Nothing else gets it */
static void test_readable_parts(void **state)
{
    static const unsigned char *const readable[] = { UU_DISK, UU_NEW };
    static const unsigned char *const with_null[] = { UU_NEW, NULL };
    struct fixture *f = *state;
    bio_dev_t dev;

    assert_int_equal(bio_set_readable(NULL, 1), -PB_ERR_PARAM);
    assert_int_equal(bio_set_readable(with_null, ARRAY_SIZE(with_null)), -PB_ERR_PARAM);
    assert_int_equal(bio_set_readable(NULL, 0), PB_OK);
    assert_false(bio_get_flags(0) & BIO_FLAG_READABLE);

    assert_int_equal(bio_set_flags(0, DISK_FLAGS), PB_OK);
    assert_int_equal(bio_set_flags(f->part, DISK_FLAGS), PB_OK);
    assert_int_equal(bio_set_readable(readable, ARRAY_SIZE(readable)), PB_OK);
    assert_int_equal(bio_get_flags(0), DISK_FLAGS | BIO_FLAG_READABLE);
    /* Not on the list: as it was */
    assert_int_equal(bio_get_flags(f->part), DISK_FLAGS);

    /* Made after the list: by its UUID. A partition starts with its device's
     * flags, READABLE too (F-470), so the device has it no more here */
    assert_int_equal(bio_set_flags(0, DISK_FLAGS), PB_OK);
    dev = bio_allocate_parent(0, 2000, 2999, RAMDISK_BLOCK_SIZE, UU_NEW, "Readable");
    assert_true(dev >= 0);
    assert_int_equal(bio_get_flags(dev), DISK_FLAGS | BIO_FLAG_READABLE);
    dev = bio_allocate_parent(0, 3000, 3999, RAMDISK_BLOCK_SIZE, UU_NEW2, "Not readable");
    assert_true(dev >= 0);
    assert_int_equal(bio_get_flags(dev), DISK_FLAGS);

    /* A reset forgets the list */
    bio_reset();
    dev = ramdisk_attach(f->disk, UU_DISK);
    assert_int_equal(bio_get_flags(dev), DISK_FLAGS);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_access_is_relative_to_partition, setup, teardown),
        cmocka_unit_test_setup_teardown(test_last_block_ok, setup, teardown),
        cmocka_unit_test_setup_teardown(test_past_end_rejected, setup, teardown),
        cmocka_unit_test_setup_teardown(test_lba_wrap_rejected, setup, teardown),
        cmocka_unit_test_setup_teardown(test_huge_length_rejected, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_a_write_that_isnt_whole_blocks_is_padded, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_a_read_that_isnt_whole_blocks_is_trimmed, setup, teardown),
        cmocka_unit_test_setup_teardown(test_a_length_of_0_doesnt_reach_the_driver, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_a_read_into_a_buffer_off_a_line_is_refused, setup, teardown),
        cmocka_unit_test_setup_teardown(test_invalid_handles, setup, teardown),
        cmocka_unit_test_setup_teardown(test_allocate_rejects, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_a_block_larger_than_the_largest_is_refused, setup, teardown),
        cmocka_unit_test_setup_teardown(test_a_uuid_that_is_taken_is_refused, setup, teardown),
        cmocka_unit_test_setup_teardown(test_allocate_pool_full, setup, teardown),
        cmocka_unit_test_setup_teardown(test_a_partition_lies_inside_its_parent, setup, teardown),
        cmocka_unit_test_setup_teardown(test_properties, setup, teardown),
        cmocka_unit_test_setup_teardown(test_block_getters_of_a_large_device, setup, teardown),
        cmocka_unit_test_setup_teardown(test_flags, setup, teardown),
        cmocka_unit_test_setup_teardown(test_missing_ops, setup, teardown),
        cmocka_unit_test_setup_teardown(test_write_done_isnt_inherited, setup, teardown),
        cmocka_unit_test_setup_teardown(test_erase_calls_the_driver, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_private, setup, teardown),
        cmocka_unit_test_setup_teardown(test_get_part_by_uu, setup, teardown),
        cmocka_unit_test_setup_teardown(test_install_partition_table, setup, teardown),
        cmocka_unit_test_setup_teardown(test_a_changed_table_refuses_writes, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_an_install_locks_only_when_its_table_changed, setup, teardown),
        cmocka_unit_test_setup_teardown(test_readable_parts, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
