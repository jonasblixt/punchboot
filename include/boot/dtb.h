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

#include <boot/handover.h>
#include <bpak/bpak.h>
#include <stddef.h>
#include <stdint.h>
#include <uuid.h>

/**
 * A board's device tree hook, e.g. board_patch_bootargs() (board.md 2.7).
 * It gets the whole tree, and 'offset' is /chosen. It runs before pb writes
 * its own properties: what it sets of them is overwritten.
 */
typedef int (*boot_patch_dtb_cb_t)(void *fdt, int offset);

/** What a handover puts in the device tree, besides pb's own properties */
struct boot_dtb_chosen {
    boot_patch_dtb_cb_t patch_cb; /*!< Optional, the board's hook */
    bpak_id_t ramdisk_id; /*!< Optional: linux,initrd-start and -end */
    uintptr_t ramdisk_addr; /*!< Its load address */
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
 * Open a loaded device tree into memory the handover gives, with room for
 * what the boot drivers add to it
 *
 * For a handover that doesn't leave the device tree where the image has it:
 * Firmware Handoff, whose device tree is in its transfer list (boot.md
 * 5.4). The copy is headroom bytes larger than the device tree
 * (fdt_open_into copies and grows in one step), and the room it grew by is
 * zero. The part isn't written, so it needs no room after it and may be in
 * a flash that is booted in place.
 *
 * @param[in] hdr The verified image header
 * @param[in] dtb_id bpak id of the device tree part
 * @param[in] headroom Bytes the copy is larger than the device tree
 * @param[out] dest Where the copy goes, 8-byte aligned
 * @param[in] dest_size The room at dest
 *
 * @return PB_OK on success,
 *        -PB_ERR_BAD_META if the part or its load address is missing, or
 *         the part is smaller than a device tree's header,
 *        -PB_ERR_MEM if the device tree with its headroom doesn't fit
 *         dest_size, or the part overlaps dest,
 *        -PB_ERR if it isn't a device tree or is larger than its part
 */
int boot_dtb_open_into(struct bpak_header *hdr,
                       bpak_id_t dtb_id,
                       size_t headroom,
                       void *dest,
                       size_t dest_size);

/**
 * The board's hook, then pb's properties in /chosen, which is added if
 * there is none (boot.md 5.5). pb's values win: a property the hook or the
 * image's tree has under a name pb writes is overwritten.
 *
 * | Property              | Encoding                    | Not written         |
 * | --------------------- | --------------------------- | ------------------- |
 * | pb,device-uuid        | string, a UUID              |                     |
 * | pb,slc                | u32, the wire's pb_slc      | no SLC              |
 * | pb,slc-active-key     | u32, a key id               |                     |
 * | pb,slc-available-keys | u32 list, key ids           | every key revoked   |
 * | linux,initrd-start,   | u32 each, u64 each when one | no 'ramdisk_id'     |
 * | linux,initrd-end      | is at 4 GiB or above        |                     |
 * | pb,active-system      | string: "A", "B" or "?"     | no system name      |
 * | pb,boot-mode          | string: "normal",           | no room             |
 * |                       | "fallback", "speculative"   |                     |
 * | pb,boot-partition     | string, a UUID              | no partition, no    |
 * |                       |                             | room                |
 * | pb,version            | string                      | no room             |
 *
 * They are only added to, and a property keeps its encoding (boot.md 7).
 *
 * The first seven are the ones every release wrote. One that doesn't fit
 * the tree fails, as it did. One pb doesn't write is left as the tree has
 * it, from the image or the hook (but the key list, which is removed first,
 * as it was): pb doesn't remove a released property (D-008, F-348).
 *
 * The last three are new. A tree without room for one, e.g. one built with
 * placeholders for the released properties and booted with a headroom of 0,
 * does without it: the property is left out with a warning and the boot
 * goes on. In this order, each on its own: pb,boot-mode, pb,boot-partition,
 * pb,version. Room for the mode is asked for with the longest of its
 * strings, so what is left out doesn't depend on the mode. A new property
 * pb doesn't write is not in the tree afterwards, whoever put one there.
 * Together they take 138 bytes in a tree that has none of them, with the
 * version "3.0.0": 37, 70 and 31.
 *
 * @param[in] fdt The device tree, with room for them (boot_dtb_grow())
 * @param[in] hdr The verified image header, for the ramdisk's size
 * @param[in] info pb's information, the core's
 * @param[in] chosen What the handover adds
 *
 * @return PB_OK on success, the hook's error,
 *        -PB_ERR_BAD_META if the ramdisk part is missing,
 *        -PB_ERR_PARAM for a mode that has no name,
 *        -PB_ERR on a device tree error
 */
int boot_dtb_set_chosen(void *fdt,
                        struct bpak_header *hdr,
                        const struct boot_info *info,
                        const struct boot_dtb_chosen *chosen);

#endif
