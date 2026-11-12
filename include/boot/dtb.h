/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_BOOT_DTB_H
#define INCLUDE_BOOT_DTB_H

#include <bpak/bpak.h>
#include <stddef.h>
#include <stdint.h>
#include <uuid.h>

/** A board's device tree patch, e.g. board_patch_bootargs(): 'offset' is /chosen */
typedef int (*boot_patch_dtb_cb_t)(void *fdt, int offset);

/** What a boot driver puts in /chosen, besides pb's own properties */
struct boot_dtb_chosen {
    boot_patch_dtb_cb_t patch_cb; /*!< Optional, the board's */
    bpak_id_t ramdisk_id; /*!< Optional: linux,initrd-start and -end */
    uintptr_t ramdisk_addr; /*!< Its load address */
    const char *(*resolve_part_name)(uuid_t part_uu); /*!< Optional: pb,active-system */
};

/**
 * Make room in a loaded device tree for what the boot drivers add to it
 *
 * The device tree must fit its part. It is grown in place by headroom bytes
 * (fdt_open_into), into the RAM after the part: the part's size plus headroom
 * from its load address must not overlap another part of the image or pb's
 * own memory. That is checked before the device tree is read.
 *
 * @param[in] hdr The verified image header
 * @param[in] dtb_id bpak id of the device tree part
 * @param[in] headroom Bytes to grow the device tree by
 * @param[out] fdt The device tree, at its load address
 *
 * @return PB_OK on success,
 *        -PB_ERR_BAD_META if the part or its load address is missing,
 *        -PB_ERR_MEM if the grown device tree would overlap a part or pb,
 *        -PB_ERR if it isn't a device tree or is larger than its part
 */
int boot_dtb_grow(struct bpak_header *hdr, bpak_id_t dtb_id, size_t headroom, void **fdt);

/**
 * Set /chosen, added if there is none: pb,device-uuid, the board's patch,
 * pb,slc (unless the board has no SLC), pb,slc-active-key (the image's key),
 * pb,slc-available-keys, linux,initrd-start and -end, pb,active-system
 *
 * @param[in] fdt The device tree, with room for them (boot_dtb_grow())
 * @param[in] hdr The verified image header
 * @param[in] boot_part_uu The partition booted from
 * @param[in] chosen What the boot driver adds
 *
 * @return PB_OK on success, the patch's or the SLC's error,
 *        -PB_ERR_BAD_META if the ramdisk part is missing,
 *        -PB_ERR on a device tree error
 */
int boot_dtb_set_chosen(void *fdt,
                        struct bpak_header *hdr,
                        uuid_t boot_part_uu,
                        const struct boot_dtb_chosen *chosen);

#endif
