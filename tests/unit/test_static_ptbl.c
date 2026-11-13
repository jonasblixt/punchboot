/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The static partition table (src/drivers/partition/static.c): the board's
 * table becomes partitions, and on a device that is memory mapped a
 * partition can be booted in place (board.md 2.4).
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/partition/static.h>
#include <pb/bio.h>
#include <pb/pb.h>
#include <stdlib.h>
#include <string.h>

#include "fakes/ramdisk.h"

#define DISK_BLOCKS 64
#define BLOCK       RAMDISK_BLOCK_SIZE

#define UU_DISK \
    (const unsigned char *)"\x1e\xac\xed\xf3\x37\x90\x48\xc7\x8e\xd8\x91\x88\xff\x49\x67\x2b"
#define UU_DISK2 \
    (const unsigned char *)"\x8b\x0e\x0c\x55\x4f\x0a\x4d\x4a\x9b\x1c\x5a\x6e\x21\x3d\x7f\x10"
#define UU_BL \
    (const unsigned char *)"\xff\x4d\xdc\x6c\xad\x7a\x47\xe8\x87\x73\x67\x29\x39\x2d\xd1\xb5"
#define UU_STATE \
    (const unsigned char *)"\xf5\xf8\xc9\xae\xef\xb5\x40\x71\x9b\xa9\xd3\x13\xb0\x82\x28\x1e"
#define UU_SYS_A \
    (const unsigned char *)"\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62\x01\x4c\xfa\x73\x5c\xe0"
#define UU_SYS_B \
    (const unsigned char *)"\xc0\x46\xcc\xd8\x0f\x2e\x40\x36\x98\x4d\x76\xc1\x4d\xc7\x39\x92"
#define UU_SYS_C \
    (const unsigned char *)"\x4b\x1d\x2f\x0a\x93\x6c\x4e\x11\xa7\x30\x5e\x8d\x02\xc4\x6b\x9f"
#define UU_OTHER \
    (const unsigned char *)"\x6d\x21\x9a\x3e\x7c\x58\x4f\x02\xb1\x44\x0d\x9e\x35\xa8\x17\xc6"

/*
 * BL at 0, the state after it, a gap, then the systems. A and C are booted
 * in place: A after the gap, C behind an entry that isn't.
 */
static const struct static_part_table table[] = {
    { .uu = UU_BL, .description = "BL", .size = 4 * BLOCK },
    { .uu = UU_STATE, .description = "State", .size = BLOCK },
    {
        .uu = UU_SYS_A,
        .description = "System A",
        .first_lba = 16,
        .size = 16 * BLOCK,
        .flags = STATIC_PART_FLAG_IN_PLACE,
    },
    { .uu = UU_SYS_B, .description = "System B", .size = 16 * BLOCK },
    {
        .uu = UU_SYS_C,
        .description = "System C",
        .size = 8 * BLOCK,
        .flags = STATIC_PART_FLAG_IN_PLACE,
    },
    { .uu = NULL },
};

/* The same layout, everything loaded */
static const struct static_part_table plain_table[] = {
    { .uu = UU_BL, .description = "BL", .size = 4 * BLOCK },
    { .uu = UU_STATE, .description = "State", .size = BLOCK },
    { .uu = UU_SYS_A, .description = "System A", .first_lba = 16, .size = 16 * BLOCK },
    { .uu = UU_SYS_B, .description = "System B", .size = 16 * BLOCK },
    { .uu = UU_SYS_C, .description = "System C", .size = 8 * BLOCK },
    { .uu = NULL },
};

/*
 * The board's objects for these tables, the way a board has them in its data
 * (board.ptbl, F-265): PTBL_STATIC() and PTBL_STATIC_MAPPED() stay constant
 * initializers. A board's flash is mapped at an address that is a constant;
 * the ramdisk's memory isn't, see test_object_mapped.
 */
#define FLASH_BASE 0x60000000

static const struct ptbl ptbl_plain = PTBL_STATIC(UU_DISK, plain_table);
static const struct ptbl ptbl_flagged = PTBL_STATIC(UU_DISK, table);
static const struct ptbl ptbl_flash = PTBL_STATIC_MAPPED(UU_DISK, FLASH_BASE, table);

struct fixture {
    struct ramdisk *disk;
    bio_dev_t dev;
};

static int setup(void **state)
{
    struct fixture *f = calloc(1, sizeof(*f));
    assert_non_null(f);
    f->disk = ramdisk_new(DISK_BLOCKS);
    assert_non_null(f->disk);

    bio_reset();
    f->dev = ramdisk_attach(f->disk, UU_DISK);
    assert_true(f->dev >= 0);

    *state = f;
    return 0;
}

static int teardown(void **state)
{
    struct fixture *f = *state;
    ramdisk_free(f->disk);
    free(f);
    return 0;
}

static void assert_part(const unsigned char *uu, int first, int last)
{
    bio_dev_t part = bio_get_part_by_uu(uu);

    assert_true(part >= 0);
    assert_int_equal(bio_get_first_block(part), first);
    assert_int_equal(bio_get_last_block(part), last);
    assert_int_equal(bio_block_size(part), BLOCK);
}

/* Only the device itself: no entry of a table became a partition */
static void assert_nothing_allocated(struct fixture *f)
{
    assert_true(bio_get_part_by_uu(UU_BL) < 0);
    assert_true(bio_get_part_by_uu(UU_STATE) < 0);
    assert_true(bio_get_part_by_uu(UU_SYS_A) < 0);
    assert_true(bio_get_part_by_uu(UU_SYS_B) < 0);
    assert_true(bio_get_part_by_uu(UU_SYS_C) < 0);
    assert_true(bio_get_part_by_uu(UU_OTHER) < 0);
    /* The next device is the one after the fixture's */
    assert_false(bio_valid(f->dev + 1));
}

static void assert_not_in_place(bio_dev_t dev)
{
    uintptr_t base = 0;
    size_t size = 0;

    assert_int_equal(static_ptbl_in_place(dev, &base, &size), -PB_ERR_NOT_FOUND);
}

static void assert_in_place(bio_dev_t dev, uintptr_t expected_base, size_t expected_size)
{
    uintptr_t base = ~expected_base;
    size_t size = ~expected_size;

    assert_int_equal(static_ptbl_in_place(dev, &base, &size), PB_OK);
    assert_true(base == expected_base);
    assert_int_equal(size, expected_size);
}

/* Runs first: no mapped table yet, the module's state can't be reset */
static void test_nothing_mapped(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(static_ptbl_init(f->dev, plain_table), PB_OK);

    assert_not_in_place(bio_get_part_by_uu(UU_SYS_A));
    assert_not_in_place(f->dev);
    assert_not_in_place(-1);
}

static void test_init(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(static_ptbl_init(f->dev, plain_table), PB_OK);

    /* Back to back from the device's start, and from first_lba after a gap */
    assert_part(UU_BL, 0, 3);
    assert_part(UU_STATE, 4, 4);
    assert_part(UU_SYS_A, 16, 31);
    assert_part(UU_SYS_B, 32, 47);
    assert_part(UU_SYS_C, 48, 55);
    assert_int_equal(bio_size(bio_get_part_by_uu(UU_SYS_A)), 16 * BLOCK);

    /* A partition is read through its parent */
    memset(ramdisk_block(f->disk, 32), 0xa5, BLOCK);
    uint8_t buf[BLOCK];
    assert_int_equal(bio_read(bio_get_part_by_uu(UU_SYS_B), 0, sizeof(buf), buf), PB_OK);
    assert_int_equal(buf[0], 0xa5);
    assert_int_equal(buf[BLOCK - 1], 0xa5);
}

static void test_init_errors(void **state)
{
    static const struct static_part_table unaligned[] = {
        { .uu = UU_BL, .description = "BL", .size = 4 * BLOCK },
        { .uu = UU_STATE, .description = "State", .size = BLOCK + 1 },
        { .uu = NULL },
    };
    /* An empty partition, first in the table and after another */
    static const struct static_part_table empty_first[] = {
        { .uu = UU_BL, .description = "BL", .size = 0 },
        { .uu = NULL },
    };
    static const struct static_part_table empty_later[] = {
        { .uu = UU_BL, .description = "BL", .size = 4 * BLOCK },
        { .uu = UU_STATE, .description = "State", .size = 0 },
        { .uu = NULL },
    };
    struct fixture *f = *state;

    assert_int_equal(static_ptbl_init(f->dev, empty_first), -PB_ERR_PARAM);
    assert_int_equal(static_ptbl_init(f->dev, empty_later), -PB_ERR_PARAM);
    assert_int_equal(static_ptbl_init_mapped(f->dev, 0, empty_later), -PB_ERR_PARAM);
    assert_nothing_allocated(f);

    assert_int_equal(static_ptbl_init(f->dev, NULL), -PB_ERR_PARAM);
    assert_int_equal(static_ptbl_init_mapped(f->dev, 0, NULL), -PB_ERR_PARAM);

    /* A device that isn't there, also with nothing in the table */
    assert_int_equal(static_ptbl_init(-1, unaligned), -PB_ERR_PARAM);
    assert_int_equal(static_ptbl_init_mapped(-1, 0, &unaligned[2]), -PB_ERR_PARAM);

    assert_int_equal(static_ptbl_init(f->dev, unaligned), -PB_ERR_ALIGN);
    assert_nothing_allocated(f);
    assert_int_equal(static_ptbl_init_mapped(f->dev, 0, unaligned), -PB_ERR_ALIGN);
    assert_nothing_allocated(f);
}

/* In place needs the address the device is mapped at */
static void test_init_refuses_in_place(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(static_ptbl_init(f->dev, table), -PB_ERR_PARAM);

    /* The table is checked whole first: nothing of it is there, the entries
     * before the flagged one neither */
    assert_nothing_allocated(f);
}

/*
 * The table through the board's object (F-265), the way src/board.c sets it
 * up. Runs before a table is mapped: the module's state can't be reset, and
 * a partition with the UUID of a mapped table's flagged entry is in place.
 */
static void test_object_static(void **state)
{
    struct fixture *f = *state;

    assert_ptr_equal(ptbl_plain.ops, &ptbl_static_ops);
    assert_memory_equal(ptbl_plain.device, UU_DISK, 16);
    assert_ptr_equal(ptbl_plain.table, plain_table);
    assert_int_equal(ptbl_plain.length, 0);
    assert_true(ptbl_plain.base == 0);

    assert_int_equal(ptbl_plain.ops->init(&ptbl_plain, f->dev), PB_OK);

    assert_part(UU_BL, 0, 3);
    assert_part(UU_STATE, 4, 4);
    assert_part(UU_SYS_A, 16, 31);
    assert_part(UU_SYS_B, 32, 47);
    assert_part(UU_SYS_C, 48, 55);

    /* Everything is loaded */
    assert_not_in_place(bio_get_part_by_uu(UU_SYS_A));
    assert_not_in_place(bio_get_part_by_uu(UU_SYS_C));

    /* The core leaves the device visible only (src/board.c). Its partitions
     * got the device's flags when they were made and keep them: a host
     * writes them, and the A/B state is written through them */
    int device_flags = bio_get_flags(f->dev);

    assert_true(device_flags & BIO_FLAG_WRITABLE);
    assert_int_equal(bio_set_flags(f->dev, BIO_FLAG_VISIBLE), PB_OK);
    assert_int_equal(bio_get_flags(f->dev), BIO_FLAG_VISIBLE);
    assert_int_equal(bio_get_flags(bio_get_part_by_uu(UU_STATE)), device_flags);
    assert_int_equal(bio_get_flags(bio_get_part_by_uu(UU_SYS_A)), device_flags);
}

/* PTBL_STATIC() has no address: a flagged entry is refused through the
 * object as it is by static_ptbl_init() */
static void test_object_static_refuses_in_place(void **state)
{
    struct fixture *f = *state;

    assert_ptr_equal(ptbl_flagged.ops, &ptbl_static_ops);
    assert_ptr_equal(ptbl_flagged.table, table);

    assert_int_equal(ptbl_flagged.ops->init(&ptbl_flagged, f->dev), -PB_ERR_PARAM);
    assert_nothing_allocated(f);
}

static void test_mapped(void **state)
{
    struct fixture *f = *state;
    uintptr_t base = (uintptr_t)f->disk->data;

    assert_int_equal(static_ptbl_init_mapped(f->dev, base, table), PB_OK);

    /* The partitions are the same as without a map */
    assert_part(UU_BL, 0, 3);
    assert_part(UU_STATE, 4, 4);
    assert_part(UU_SYS_A, 16, 31);
    assert_part(UU_SYS_B, 32, 47);
    assert_part(UU_SYS_C, 48, 55);

    /* The base and the entry's first block give the address */
    assert_in_place(bio_get_part_by_uu(UU_SYS_A), base + 16 * BLOCK, 16 * BLOCK);
    assert_in_place(bio_get_part_by_uu(UU_SYS_C), base + 48 * BLOCK, 8 * BLOCK);

    /* It is the memory that bio reads */
    uintptr_t a_base;
    size_t a_size;
    uint8_t buf[BLOCK];
    assert_int_equal(static_ptbl_in_place(bio_get_part_by_uu(UU_SYS_A), &a_base, &a_size), PB_OK);
    memset(ramdisk_block(f->disk, 31), 0x3c, BLOCK);
    assert_int_equal(bio_read(bio_get_part_by_uu(UU_SYS_A), 15, sizeof(buf), buf), PB_OK);
    assert_memory_equal((const void *)(a_base + a_size - BLOCK), buf, BLOCK);

    /* Not flagged, the device itself, no device */
    assert_not_in_place(bio_get_part_by_uu(UU_BL));
    assert_not_in_place(bio_get_part_by_uu(UU_SYS_B));
    assert_not_in_place(f->dev);
    assert_not_in_place(-1);
    assert_not_in_place(CONFIG_BIO_MAX_DEVS);
}

/* No address stands for "not mapped": flash can be mapped at 0 */
static void test_mapped_at_zero(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(static_ptbl_init_mapped(f->dev, 0, table), PB_OK);
    assert_in_place(bio_get_part_by_uu(UU_SYS_A), 16 * BLOCK, 16 * BLOCK);
    assert_in_place(bio_get_part_by_uu(UU_SYS_C), 48 * BLOCK, 8 * BLOCK);
    assert_not_in_place(bio_get_part_by_uu(UU_SYS_B));
}

/* A mapped table without a flagged entry: everything is loaded */
static void test_mapped_nothing_in_place(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(static_ptbl_init_mapped(f->dev, (uintptr_t)f->disk->data, plain_table), PB_OK);
    assert_not_in_place(bio_get_part_by_uu(UU_SYS_A));
    assert_not_in_place(bio_get_part_by_uu(UU_SYS_C));
}

/* The last byte of a partition that is booted in place has an address */
static void test_mapped_address_space(void **state)
{
    static const struct static_part_table two[] = {
        {
            .uu = UU_SYS_A,
            .description = "System A",
            .size = 4 * BLOCK,
            .flags = STATIC_PART_FLAG_IN_PLACE,
        },
        {
            .uu = UU_SYS_B,
            .description = "System B",
            .size = 4 * BLOCK,
            .flags = STATIC_PART_FLAG_IN_PLACE,
        },
        { .uu = NULL },
    };
    /* B's last byte is the last address there is */
    const uintptr_t top = UINTPTR_MAX - 8 * BLOCK + 1;
    struct fixture *f = *state;

    assert_int_equal(static_ptbl_init_mapped(f->dev, top, two), PB_OK);
    assert_in_place(bio_get_part_by_uu(UU_SYS_A), top, 4 * BLOCK);
    assert_in_place(bio_get_part_by_uu(UU_SYS_B), top + 4 * BLOCK, 4 * BLOCK);

    /* One byte higher: A fits, B doesn't, and the table is refused whole */
    bio_reset();
    f->dev = ramdisk_attach(f->disk, UU_DISK);
    assert_int_equal(static_ptbl_init_mapped(f->dev, top + 1, two), -PB_ERR_PARAM);
    assert_nothing_allocated(f);

    /* A partition that isn't booted in place may end anywhere */
    assert_int_equal(static_ptbl_init_mapped(f->dev, UINTPTR_MAX, plain_table), PB_OK);
    assert_part(UU_SYS_C, 48, 55);
}

/* A table with one entry, booted in place, for a second device */
static const struct static_part_table other_table[] = {
    {
        .uu = UU_OTHER,
        .description = "Other",
        .size = 8 * BLOCK,
        .flags = STATIC_PART_FLAG_IN_PLACE,
    },
    { .uu = NULL },
};

/*
 * A partition that is booted in place stays so. With another table in its
 * table's place it would be loaded, to its address in the flash: the second
 * table is refused, whether it is a good one or not, and nothing changes.
 */
static void test_second_mapped_table_is_refused(void **state)
{
    static const struct static_part_table unaligned[] = {
        { .uu = UU_OTHER, .description = "Other", .size = BLOCK + 1 },
        { .uu = NULL },
    };
    struct fixture *f = *state;
    uintptr_t base = (uintptr_t)f->disk->data;
    struct ramdisk *disk2 = ramdisk_new(DISK_BLOCKS);
    assert_non_null(disk2);

    assert_int_equal(static_ptbl_init_mapped(f->dev, base, table), PB_OK);

    bio_dev_t dev2 = ramdisk_attach(disk2, UU_DISK2);
    assert_true(dev2 >= 0);
    bio_dev_t next = bio_get_part_by_uu(UU_SYS_C) + 2;

    assert_int_equal(static_ptbl_init_mapped(dev2, (uintptr_t)disk2->data, other_table),
                     -PB_ERR_STATE);
    assert_int_equal(static_ptbl_init_mapped(dev2, 0, plain_table), -PB_ERR_STATE);
    /* The state is looked at first: a bad table gets the same answer */
    assert_int_equal(static_ptbl_init_mapped(dev2, 0, unaligned), -PB_ERR_STATE);
    assert_int_equal(static_ptbl_init_mapped(f->dev, base, table), -PB_ERR_STATE);

    /* Nothing of the second table is there */
    assert_true(bio_get_part_by_uu(UU_OTHER) < 0);
    assert_false(bio_valid(next));

    /* The first is as it was */
    assert_in_place(bio_get_part_by_uu(UU_SYS_A), base + 16 * BLOCK, 16 * BLOCK);
    assert_in_place(bio_get_part_by_uu(UU_SYS_C), base + 48 * BLOCK, 8 * BLOCK);
    assert_not_in_place(bio_get_part_by_uu(UU_SYS_B));

    ramdisk_free(disk2);
}

/* A mapped table with nothing booted in place has nothing to keep */
static void test_mapped_table_without_in_place_is_replaced(void **state)
{
    struct fixture *f = *state;
    struct ramdisk *disk2 = ramdisk_new(DISK_BLOCKS);
    assert_non_null(disk2);
    uintptr_t base2 = (uintptr_t)disk2->data;

    assert_int_equal(static_ptbl_init_mapped(f->dev, (uintptr_t)f->disk->data, plain_table), PB_OK);

    bio_dev_t dev2 = ramdisk_attach(disk2, UU_DISK2);
    assert_true(dev2 >= 0);
    assert_int_equal(static_ptbl_init_mapped(dev2, base2, other_table), PB_OK);

    assert_part(UU_OTHER, 0, 7);
    assert_in_place(bio_get_part_by_uu(UU_OTHER), base2, 8 * BLOCK);
    /* The first table's partitions are there, loaded as before */
    assert_part(UU_SYS_A, 16, 31);
    assert_not_in_place(bio_get_part_by_uu(UU_SYS_A));

    ramdisk_free(disk2);
}

/*
 * A table that fails its check changes nothing, the map neither: the table
 * that was mapped before it still is. Here its partitions are gone (a reboot
 * of bio only, which pb doesn't do), so the bad table gets as far as its
 * check.
 */
static void test_failed_check_keeps_the_map(void **state)
{
    static const struct static_part_table unaligned[] = {
        { .uu = UU_OTHER, .description = "Other", .size = BLOCK + 1 },
        { .uu = NULL },
    };
    struct fixture *f = *state;
    uintptr_t base = (uintptr_t)f->disk->data;

    assert_int_equal(static_ptbl_init_mapped(f->dev, base, table), PB_OK);

    bio_reset();
    f->dev = ramdisk_attach(f->disk, UU_DISK);
    assert_int_equal(static_ptbl_init_mapped(f->dev, 0, unaligned), -PB_ERR_ALIGN);
    assert_int_equal(static_ptbl_init_mapped(f->dev, 0, NULL), -PB_ERR_PARAM);
    assert_int_equal(static_ptbl_init_mapped(-1, 0, plain_table), -PB_ERR_PARAM);
    assert_nothing_allocated(f);

    /* The first table and its base: a partition of it is in place again */
    bio_dev_t sys_a = bio_allocate_parent(f->dev, 16, 31, BLOCK, UU_SYS_A, "System A");
    assert_true(sys_a >= 0);
    assert_in_place(sys_a, base + 16 * BLOCK, 16 * BLOCK);
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

/*
 * The table is mapped before its partitions exist, so that one that is
 * there is in place also when a later one can't be allocated. Without the
 * map it would be loaded, to its address in the flash.
 */
static void test_allocation_fails_midway(void **state)
{
    struct fixture *f = *state;
    uintptr_t base = (uintptr_t)f->disk->data;
    int free_devs = CONFIG_BIO_MAX_DEVS - (f->dev + 1);

    /* Room for BL, the state and System A */
    for (int i = 0; i < free_devs - 3; i++)
        assert_true(bio_allocate(60, 61, BLOCK, filler_uu(i), "Filler") >= 0);

    assert_int_equal(static_ptbl_init_mapped(f->dev, base, table), -PB_ERR_MEM);

    assert_part(UU_BL, 0, 3);
    assert_part(UU_STATE, 4, 4);
    assert_part(UU_SYS_A, 16, 31);
    assert_true(bio_get_part_by_uu(UU_SYS_B) < 0);
    assert_true(bio_get_part_by_uu(UU_SYS_C) < 0);

    assert_in_place(bio_get_part_by_uu(UU_SYS_A), base + 16 * BLOCK, 16 * BLOCK);
    assert_not_in_place(bio_get_part_by_uu(UU_BL));

    /* And it keeps its table: another one is refused */
    assert_int_equal(static_ptbl_init_mapped(f->dev, base, plain_table), -PB_ERR_STATE);
    assert_in_place(bio_get_part_by_uu(UU_SYS_A), base + 16 * BLOCK, 16 * BLOCK);
}

/* Without a map a failed allocation leaves the partitions before it, loaded */
static void test_allocation_fails_midway_unmapped(void **state)
{
    struct fixture *f = *state;
    int free_devs = CONFIG_BIO_MAX_DEVS - (f->dev + 1);

    for (int i = 0; i < free_devs - 3; i++)
        assert_true(bio_allocate(60, 61, BLOCK, filler_uu(i), "Filler") >= 0);

    assert_int_equal(static_ptbl_init(f->dev, plain_table), -PB_ERR_MEM);
    assert_part(UU_SYS_A, 16, 31);
    assert_true(bio_get_part_by_uu(UU_SYS_B) < 0);
}

/*
 * The range is the table's, never what the device says of itself: a
 * partition of another table (a GPT's comes from the storage) with the
 * UUID of an entry gets the entry's range, not one made of its own blocks.
 */
static void test_range_is_the_tables(void **state)
{
    struct fixture *f = *state;
    uintptr_t base = (uintptr_t)f->disk->data;

    assert_int_equal(static_ptbl_init_mapped(f->dev, base, table), PB_OK);

    /* A reboot of bio only: the same UUID on other blocks, of another size */
    bio_reset();
    f->dev = ramdisk_attach(f->disk, UU_DISK);
    bio_dev_t imposter = bio_allocate_parent(f->dev, 2, 60, BLOCK, UU_SYS_A, "Imposter");
    assert_true(imposter >= 0);
    assert_int_equal(bio_get_first_block(imposter), 2);

    assert_in_place(imposter, base + 16 * BLOCK, 16 * BLOCK);

    /* And with the UUID of an entry that isn't flagged: not in place */
    bio_dev_t other = bio_allocate_parent(f->dev, 61, 62, BLOCK, UU_SYS_B, "Imposter B");
    assert_true(other >= 0);
    assert_not_in_place(other);
}

/* A table without a map, for another device, leaves the mapped one alone */
static void test_unmapped_table_keeps_the_map(void **state)
{
    static const struct static_part_table loaded_table[] = {
        { .uu = UU_OTHER, .description = "Other", .size = 8 * BLOCK },
        { .uu = NULL },
    };
    struct fixture *f = *state;
    uintptr_t base = (uintptr_t)f->disk->data;
    struct ramdisk *disk2 = ramdisk_new(DISK_BLOCKS);
    assert_non_null(disk2);

    assert_int_equal(static_ptbl_init_mapped(f->dev, base, table), PB_OK);

    bio_dev_t dev2 = ramdisk_attach(disk2, UU_DISK2);
    assert_true(dev2 >= 0);
    assert_int_equal(static_ptbl_init(dev2, loaded_table), PB_OK);
    assert_part(UU_OTHER, 0, 7);

    assert_in_place(bio_get_part_by_uu(UU_SYS_A), base + 16 * BLOCK, 16 * BLOCK);
    assert_in_place(bio_get_part_by_uu(UU_SYS_C), base + 48 * BLOCK, 8 * BLOCK);
    assert_not_in_place(bio_get_part_by_uu(UU_OTHER));

    ramdisk_free(disk2);
}

/* PTBL_STATIC_MAPPED() as a board has it: the base is a constant, and it
 * reaches the driver (F-265) */
static void test_object_mapped_constant_base(void **state)
{
    struct fixture *f = *state;

    assert_ptr_equal(ptbl_flash.ops, &ptbl_static_mapped_ops);
    assert_memory_equal(ptbl_flash.device, UU_DISK, 16);
    assert_ptr_equal(ptbl_flash.table, table);
    assert_true(ptbl_flash.base == FLASH_BASE);

    assert_int_equal(ptbl_flash.ops->init(&ptbl_flash, f->dev), PB_OK);

    assert_part(UU_SYS_A, 16, 31);
    assert_part(UU_SYS_C, 48, 55);
    assert_in_place(bio_get_part_by_uu(UU_SYS_A), FLASH_BASE + 16 * BLOCK, 16 * BLOCK);
    assert_in_place(bio_get_part_by_uu(UU_SYS_C), FLASH_BASE + 48 * BLOCK, 8 * BLOCK);
    assert_not_in_place(bio_get_part_by_uu(UU_SYS_B));
}

/*
 * The same with the ramdisk's memory for a base. It is known when the test
 * runs, which a static initializer can't take: the object is a local one,
 * made by the same macro.
 */
static void test_object_mapped(void **state)
{
    struct fixture *f = *state;
    uintptr_t base = (uintptr_t)f->disk->data;
    const struct ptbl mapped = PTBL_STATIC_MAPPED(UU_DISK, base, table);

    assert_ptr_equal(mapped.ops, &ptbl_static_mapped_ops);
    assert_true(mapped.base == base);

    assert_int_equal(mapped.ops->init(&mapped, f->dev), PB_OK);

    assert_part(UU_BL, 0, 3);
    assert_part(UU_STATE, 4, 4);
    assert_part(UU_SYS_A, 16, 31);
    assert_part(UU_SYS_B, 32, 47);
    assert_part(UU_SYS_C, 48, 55);

    /* The object's base and the entry's first block give the address */
    assert_in_place(bio_get_part_by_uu(UU_SYS_A), base + 16 * BLOCK, 16 * BLOCK);
    assert_in_place(bio_get_part_by_uu(UU_SYS_C), base + 48 * BLOCK, 8 * BLOCK);
    assert_not_in_place(bio_get_part_by_uu(UU_SYS_B));

    /* The driver's rule holds through the object: the table isn't replaced
     * while a partition of it that is booted in place exists */
    assert_int_equal(ptbl_flash.ops->init(&ptbl_flash, f->dev), -PB_ERR_STATE);
    assert_in_place(bio_get_part_by_uu(UU_SYS_A), base + 16 * BLOCK, 16 * BLOCK);
}

#define PTBL_TEST(f) cmocka_unit_test_setup_teardown(f, setup, teardown)

int main(void)
{
    const struct CMUnitTest tests[] = {
        PTBL_TEST(test_nothing_mapped),
        PTBL_TEST(test_init),
        PTBL_TEST(test_init_errors),
        PTBL_TEST(test_init_refuses_in_place),
        PTBL_TEST(test_object_static),
        PTBL_TEST(test_object_static_refuses_in_place),
        PTBL_TEST(test_mapped),
        PTBL_TEST(test_mapped_at_zero),
        PTBL_TEST(test_mapped_nothing_in_place),
        PTBL_TEST(test_mapped_address_space),
        PTBL_TEST(test_second_mapped_table_is_refused),
        PTBL_TEST(test_mapped_table_without_in_place_is_replaced),
        PTBL_TEST(test_failed_check_keeps_the_map),
        PTBL_TEST(test_allocation_fails_midway),
        PTBL_TEST(test_allocation_fails_midway_unmapped),
        PTBL_TEST(test_range_is_the_tables),
        PTBL_TEST(test_unmapped_table_keeps_the_map),
        PTBL_TEST(test_object_mapped_constant_base),
        PTBL_TEST(test_object_mapped),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
