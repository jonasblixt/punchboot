/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This is a simplistic fusebox emulator using a block device as backend,
 * it's only inteded to be used for testing.
 *
 */

#include <drivers/fuse/test_fuse_bio.h>
#include <pb/errors.h>

#define TEST_FUSE_ADDR_MAX 128

/* Use one 512 byte sector */
static uint32_t fuse_box[TEST_FUSE_ADDR_MAX];
static bio_dev_t dev;

int test_fuse_init(bio_dev_t dev_)
{
    int rc;
    dev = dev_;

    rc = bio_read(dev, 0, TEST_FUSE_ADDR_MAX * sizeof(uint32_t),
                  (uintptr_t) fuse_box);

    if (rc < 0)
        return rc;

    return PB_OK;
}

int test_fuse_write(uint16_t addr, uint32_t value)
{
    if (addr >= TEST_FUSE_ADDR_MAX)
        return -PB_ERR_PARAM;

    fuse_box[addr] |= value;

    return bio_write(dev, 0, TEST_FUSE_ADDR_MAX * sizeof(uint32_t),
                  (uintptr_t) fuse_box);
}

int test_fuse_read(uint16_t addr)
{
    if (addr >= TEST_FUSE_ADDR_MAX)
        return -PB_ERR_PARAM;

    return fuse_box[addr];
}
