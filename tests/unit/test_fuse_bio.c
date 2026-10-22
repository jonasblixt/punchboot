/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The block device backed test fuse driver (src/drivers/fuse/test_fuse_bio.c).
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/fuse/test_fuse_bio.h>
#include <pb/bio.h>
#include <pb/pb.h>
#include <stdlib.h>
#include <string.h>

#include "fakes/ramdisk.h"

#define UU_DISK \
    (const unsigned char *)"\x1e\xac\xed\xf3\x37\x90\x48\xc7\x8e\xd8\x91\x88\xff\x49\x67\x2b"

struct fixture {
    struct ramdisk *disk;
    bio_dev_t dev;
};

static int setup(void **state)
{
    struct fixture *f = calloc(1, sizeof(*f));
    assert_non_null(f);
    f->disk = ramdisk_new(4);
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

static void test_fuses_persist(void **state)
{
    struct fixture *f = *state;
    uint32_t *fuses = (uint32_t *)ramdisk_block(f->disk, 0);

    fuses[3] = 0x10;
    assert_int_equal(test_fuse_init(f->dev), PB_OK);
    assert_int_equal(test_fuse_read(3), 0x10);

    /* Fuses can only be set, never cleared */
    assert_int_equal(test_fuse_write(3, 0x01), PB_OK);
    assert_int_equal(test_fuse_read(3), 0x11);
    assert_int_equal(fuses[3], 0x11);
}

static void test_address_range(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(test_fuse_init(f->dev), PB_OK);
    assert_int_equal(test_fuse_write(127, 1), PB_OK);
    assert_int_equal(test_fuse_write(128, 1), -PB_ERR_PARAM);
    assert_int_equal(test_fuse_read(128), -PB_ERR_PARAM);
}

static void test_io_errors(void **state)
{
    struct fixture *f = *state;

    f->disk->fail_reads_after = 0;
    assert_int_equal(test_fuse_init(f->dev), -PB_ERR_IO);

    f->disk->fail_reads_after = -1;
    assert_int_equal(test_fuse_init(f->dev), PB_OK);
    f->disk->fail_writes_after = 0;
    assert_int_equal(test_fuse_write(0, 1), -PB_ERR_IO);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_fuses_persist, setup, teardown),
        cmocka_unit_test_setup_teardown(test_address_range, setup, teardown),
        cmocka_unit_test_setup_teardown(test_io_errors, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
