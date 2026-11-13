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
    size_t block_size;
    unsigned int reads;
    unsigned int writes;
    /* How often each block was written, by its LBA: 'blocks' counters */
    unsigned int *block_writes;
    /* Fault injection: -1 never fails, n lets n more operations succeed and
     * then fails every operation with -PB_ERR_IO */
    int fail_reads_after;
    int fail_writes_after;
};

/* Allocate 'blocks' zeroed blocks of RAMDISK_BLOCK_SIZE bytes of backing memory */
struct ramdisk *ramdisk_new(size_t blocks);
/* The same with blocks of 'block_size' bytes */
struct ramdisk *ramdisk_new_block_size(size_t blocks, size_t block_size);
void ramdisk_free(struct ramdisk *disk);

/**
 * Register the ramdisk as a visible, writable block device. Can be called
 * again after bio_reset() to simulate a reboot with the same disk contents.
 *
 * Its callbacks are no more forgiving than a driver's (bio.md 8): they take
 * whole blocks and never none, and answer -PB_ERR_PARAM for anything else.
 */
bio_dev_t ramdisk_attach(struct ramdisk *disk, const uuid_t uu);

/* Pointer to the start of block 'lba', for inspecting/corrupting the disk */
uint8_t *ramdisk_block(struct ramdisk *disk, lba_t lba);

#endif // TESTS_UNIT_FAKES_RAMDISK_H
