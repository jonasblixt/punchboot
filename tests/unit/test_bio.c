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

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_access_is_relative_to_partition, setup, teardown),
        cmocka_unit_test_setup_teardown(test_last_block_ok, setup, teardown),
        cmocka_unit_test_setup_teardown(test_past_end_rejected, setup, teardown),
        cmocka_unit_test_setup_teardown(test_lba_wrap_rejected, setup, teardown),
        cmocka_unit_test_setup_teardown(test_huge_length_rejected, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
