/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_BOOT_HANDOVER_IMAGE_H
#define INCLUDE_BOOT_HANDOVER_IMAGE_H

#include <boot/boot.h>
#include <boot/dtb.h>
#include <boot/handover.h>
#include <bpak/bpak.h>
#include <pb/utils_def.h>
#include <uuid.h>

/**
 * The image handover (boot.md 5.2): the Linux boot protocol, which Xen
 * follows too. The entry is the load address of the part 'image_bpak_id'.
 * The device tree, when the board names its part, gets the kernel command
 * line, pb's /chosen and, with a ramdisk, linux,initrd-start and -end.
 *
 * The registers, with 'set_dtb_boot_arg' (without it all four are 0):
 *
 *  ARMv7-A  r0 = 0, r1 = 0xffffffff (no machine type), r2 = the device tree
 *  ARMv8-A  x0 = the device tree, x1 to x3 = 0
 *
 * Delivered directly, or through TF-A as BL33's entry point (boot/atf.h).
 */
struct boot_handover_image_config {
    bpak_id_t image_bpak_id; /*!< BPAK id of bootable part */
    bpak_id_t dtb_bpak_id; /*!< Optional BPAK id of a device tree part */
    bpak_id_t ramdisk_bpak_id; /*!< Optional BPAK id of a ramdisk part */
    /** Optional, with a device tree part: the kernel command line,
     * /chosen/bootargs. NULL: the image's own stays */
    const char *bootargs;
    /** Optional: the one with BOOT_FLAG_VERBOSE. NULL: 'bootargs' */
    const char *bootargs_verbose;
    /** Optional: the board's device tree hook. It runs after the command
     * line is written and before pb writes its own properties (boot.md 5.5) */
    boot_patch_dtb_cb_t dtb_patch_cb;
    bool set_dtb_boot_arg; /*!< Pass dtb address as first argument */
};

int boot_handover_image_init(const struct boot_handover_image_config *cfg);
int boot_handover_image_prepare(struct bpak_header *hdr,
                                const struct boot_info *info,
                                struct boot_handover_entry *next);

extern const struct boot_handover_ops boot_handover_image_ops;

/**
 * A board's handover (board.boot_handover): the image handover (Linux, Xen), with
 * '_config', a const struct boot_handover_image_config *.
 */
#define BOOT_HANDOVER_IMAGE(_config)                                           \
    {                                                                          \
        .ops = &boot_handover_image_ops,                                       \
        .config = PB_CONFIG(_config, const struct boot_handover_image_config), \
    }

#endif /* INCLUDE_BOOT_HANDOVER_IMAGE_H */
