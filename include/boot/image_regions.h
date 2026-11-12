/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The image data regions (board.md 2.9, boot.md 4.3): the only memory an
 * image's parts are loaded into. The board lists them, 'image_regions' in its
 * data:
 *
 *   static const struct boot_image_region image_regions[] = {
 *       BOOT_IMAGE_REGION(0x41000000, 0x7f000000), // DRAM, above pb
 *       BOOT_IMAGE_REGION_END,
 *   };
 *
 * A part lies inside one region, with the room a handover grows it by, and
 * over no other part. That no region holds anything of pb's is checked once,
 * on the list.
 */

#ifndef INCLUDE_BOOT_IMAGE_REGIONS_H
#define INCLUDE_BOOT_IMAGE_REGIONS_H

#include <stdbool.h>
#include <stdint.h>

/* In 64 bits on every arch, as a part's load address is: a region above what
 * the arch can address fails the list's check and isn't narrowed */
struct boot_image_region {
    uint64_t base;
    uint64_t size;
};

#define BOOT_IMAGE_REGION(base_, size_) { .base = (base_), .size = (size_) }
/* Ends the list. Not a region: an empty one fails the list's check */
#define BOOT_IMAGE_REGION_END           { .base = UINT64_MAX, .size = 0 }

/**
 * Check the board's list and make it placement's (board.md 3). Once, before
 * any image is read.
 *
 * No region is empty or wraps, each is inside what the arch can address, and
 * none overlaps 'pb', 'buffers', the Firmware Handoff region or bl_params'
 * region. A list that fails is no list: nothing is loaded.
 *
 * @param[in] regions The board's list, or NULL: nothing is loaded, only a
 *  partition that is booted in place boots
 *
 * @return PB_OK, or -PB_ERR_PARAM for a list that fails
 */
int boot_image_set_regions(const struct boot_image_region *regions);

/**
 * Whether 'size' bytes at 'addr' lie inside one image data region. In 64
 * bits: a range that wraps, and an address the arch can't hold, is inside
 * none.
 */
bool boot_image_in_regions(uint64_t addr, uint64_t size);

#endif
