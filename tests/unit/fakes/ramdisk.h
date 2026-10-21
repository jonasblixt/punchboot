/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * RAM backed block device for unit tests.
 */

#ifndef TESTS_UNIT_FAKES_RAMDISK_H
#define TESTS_UNIT_FAKES_RAMDISK_H

#include <pb/bio.h>
#include <stddef.h>
#include <stdint.h>
#include <uuid.h>

#define RAMDISK_BLOCK_SIZE 512

struct ramdisk {
    uint8_t *data;
    size_t blocks;
    unsigned int reads;
    unsigned int writes;
};

/* Allocate 'blocks' zeroed blocks of backing memory */
struct ramdisk *ramdisk_new(size_t blocks);
void ramdisk_free(struct ramdisk *disk);

/**
 * Register the ramdisk as a visible, writable block device. Can be called
 * again after bio_reset() to simulate a reboot with the same disk contents.
 */
bio_dev_t ramdisk_attach(struct ramdisk *disk, const uuid_t uu);

/* Pointer to the start of block 'lba', for inspecting/corrupting the disk */
uint8_t *ramdisk_block(struct ramdisk *disk, lba_t lba);

#endif // TESTS_UNIT_FAKES_RAMDISK_H
