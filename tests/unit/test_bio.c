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
    uint8_t buf[4 * RAMDISK_BLOCK_SIZE];
};

static int setup(void **state)
{
    struct fixture *f = calloc(1, sizeof(*f));
    assert_non_null(f);
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
    free(f);
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

    assert_int_equal(bio_allocate(0, 1, 0, UU_PART, "x"), -PB_ERR_PARAM);
    assert_int_equal(bio_allocate(2, 1, RAMDISK_BLOCK_SIZE, UU_PART, "x"), -PB_ERR_PARAM);
}

static void test_allocate_pool_full(void **state)
{
    struct fixture *f = *state;
    bio_dev_t dev;

    /* The fixture uses two devices */
    for (int i = 2; i < CONFIG_BIO_MAX_DEVS; i++)
        assert_int_equal(bio_allocate(0, 1, RAMDISK_BLOCK_SIZE, UU_PART, "x"), i);

    assert_int_equal(bio_allocate(0, 1, RAMDISK_BLOCK_SIZE, UU_PART, "x"), -PB_ERR_MEM);
    dev = bio_allocate_parent(f->part, 0, 1, RAMDISK_BLOCK_SIZE, UU_PART, "x");
    assert_int_equal(dev, -PB_ERR_MEM);
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
    bio_dev_t dev = bio_allocate(0, 10, RAMDISK_BLOCK_SIZE, UU_PART, "No ops");

    assert_true(dev >= 0);
    assert_int_equal(bio_read(dev, 0, RAMDISK_BLOCK_SIZE, f->buf), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(bio_write(dev, 0, RAMDISK_BLOCK_SIZE, f->buf), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(bio_erase(dev, 0, 1), -PB_ERR_NOT_SUPPORTED);
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

/* Only the dispatch is tested. The range is not checked and the LBA is not
 * made absolute (F-055). */
static void test_erase_calls_the_driver(void **state)
{
    struct fixture *f = *state;

    erase_calls = 0;
    assert_int_equal(bio_set_ios_erase(f->part, fake_erase), PB_OK);
    assert_int_equal(bio_erase(f->part, 3, 7), PB_OK);
    assert_int_equal(erase_calls, 1);
    assert_int_equal(erase_count, 7);
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

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_access_is_relative_to_partition, setup, teardown),
        cmocka_unit_test_setup_teardown(test_last_block_ok, setup, teardown),
        cmocka_unit_test_setup_teardown(test_past_end_rejected, setup, teardown),
        cmocka_unit_test_setup_teardown(test_lba_wrap_rejected, setup, teardown),
        cmocka_unit_test_setup_teardown(test_huge_length_rejected, setup, teardown),
        cmocka_unit_test_setup_teardown(test_invalid_handles, setup, teardown),
        cmocka_unit_test_setup_teardown(test_allocate_rejects, setup, teardown),
        cmocka_unit_test_setup_teardown(test_allocate_pool_full, setup, teardown),
        cmocka_unit_test_setup_teardown(test_properties, setup, teardown),
        cmocka_unit_test_setup_teardown(test_flags, setup, teardown),
        cmocka_unit_test_setup_teardown(test_missing_ops, setup, teardown),
        cmocka_unit_test_setup_teardown(test_erase_calls_the_driver, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_private, setup, teardown),
        cmocka_unit_test_setup_teardown(test_get_part_by_uu, setup, teardown),
        cmocka_unit_test_setup_teardown(test_install_partition_table, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
