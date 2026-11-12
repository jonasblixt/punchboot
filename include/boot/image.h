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

struct boot_handover_image_config {
    bpak_id_t image_bpak_id; /*!< BPAK id of bootable part */
    bpak_id_t dtb_bpak_id; /*!< Optional BPAK id of a device tree part */
    bpak_id_t ramdisk_bpak_id; /*!< Optional BPAK id of a ramdisk part */
    boot_patch_dtb_cb_t dtb_patch_cb; /*!< Optional DTB patch callback */
    const char *(*resolve_part_name)(uuid_t part_uu);
    bool set_dtb_boot_arg; /*!< Pass dtb address as first argument */
};

int boot_handover_image_init(const struct boot_handover_image_config *cfg);
int boot_handover_image_prepare(struct bpak_header *hdr, uuid_t boot_part_uu);
void boot_handover_image_jump(void);

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
