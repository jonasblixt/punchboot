/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_BOOT_HANDOVER_FH_H
#define INCLUDE_BOOT_HANDOVER_FH_H

#include <boot/dtb.h>
#include <boot/handover.h>
#include <bpak/bpak.h>
#include <pb/utils_def.h>

/**
 * The generic handover (boot.md 5.4): Arm's Firmware Handoff. For any next
 * stage: an RTOS, an application, a hypervisor, a stage that parses a
 * device tree, on any arch pb runs on. What the next stage reads is in
 * boot/pb_fh.h: the registers, the transfer list and pb's structure.
 *
 * The entry is the load address (pb-load-addr) of the part 'entry_bpak_id',
 * the address pb hashed the part at: where pb loaded it, or in place (the
 * boot partition has STATIC_PART_FLAG_IN_PLACE in the board's static
 * partition table) its address in the map. The part starts with the entry:
 * the image's vector table on ARMv7-M, its first instruction on ARMv7-A and
 * ARMv8-A.
 *
 * The transfer list is in the board's region, CONFIG_BOOT_FH_REGION_BASE
 * and CONFIG_BOOT_FH_REGION_SIZE: memory that works after plat_init(), is
 * no image data region's and none of pb's, with a base that is 8-byte
 * aligned and a size that is a multiple of 8. It is part of the board's
 * interface to image authors: the image's memory map keeps it free until
 * the next stage has read or moved the list. It holds one of two, never
 * both:
 *
 * - With 'dtb_bpak_id': the image's device tree, copied into the list with
 *   CONFIG_BOOT_DTB_HEADROOM bytes of room. The board's hook 'dtb_patch_cb'
 *   and then pb's /chosen (boot.md 5.5) work on that copy, and the device
 *   tree register points to it. The part needs no room after it. An image
 *   without the part fails to boot, and so does a device tree that doesn't
 *   fit the region with its headroom.
 * - Without it: pb's structure (struct pb_fh_handover), pb's information.
 *
 * The image starts as every next stage does (include/pb/arch.h): the MMU
 * (MPU) and the D-cache off, the I-cache invalidated, interrupts masked, at
 * the exception level pb runs at (EL3 on the i.MX8M and i.MX8X). On ARMv7-M
 * VTOR is its vector table and MSP and PSP its initial stack pointer.
 * Delivered directly, or through TF-A as BL33's entry point (boot/atf.h).
 */
struct boot_handover_fh_config {
    bpak_id_t entry_bpak_id; /*!< The part that starts with the entry */
    bpak_id_t dtb_bpak_id; /*!< Optional: the device tree's part */
    /** Optional, with a device tree part: the board's device tree hook. It
     * runs before pb writes its own properties (boot.md 5.5) */
    boot_patch_dtb_cb_t dtb_patch_cb;
};

int boot_handover_fh_init(const struct boot_handover_fh_config *cfg);
int boot_handover_fh_prepare(struct bpak_header *hdr,
                             const struct boot_info *info,
                             struct boot_handover_entry *next);

extern const struct boot_handover_ops boot_handover_fh_ops;

/**
 * A board's handover (board.boot_handover): Firmware Handoff, with
 * '_config', a const struct boot_handover_fh_config *.
 */
#define BOOT_HANDOVER_FH(_config)                                           \
    {                                                                       \
        .ops = &boot_handover_fh_ops,                                       \
        .config = PB_CONFIG(_config, const struct boot_handover_fh_config), \
    }

#endif /* INCLUDE_BOOT_HANDOVER_FH_H */
