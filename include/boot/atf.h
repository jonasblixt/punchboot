/**
 * Punch BOOT
 *
 * Copyright (C) 2026 ACTIA Nordic AB
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_BOOT_ATF_H
#define INCLUDE_BOOT_ATF_H

#include <boot/boot.h>
#include <bpak/bpak.h>
#include <uuid.h>

typedef int (*boot_patch_dtb_cb_t)(void *fdt, int offset);

struct boot_driver_atf_config {
    bpak_id_t atf_bpak_id; /*!< BPAK id of ATF part */
    bpak_id_t bl32_bpak_id; /*!< Optional BPAK id of the BL32 image */
    bpak_id_t bl33_bpak_id; /*!< BPAK id of the BL33 image */
    bpak_id_t dtb_bpak_id; /*!< Optional BPAK id of a device tree part */
    bpak_id_t ramdisk_bpak_id; /*!< Optional BPAK id of a ramdisk part */
    boot_patch_dtb_cb_t dtb_patch_cb; /*!< Optional DTB patch callback */
    const char *(*resolve_part_name)(uuid_t part_uu);
};

int boot_driver_atf_init(const struct boot_driver_atf_config *cfg);
int boot_driver_atf_prepare(struct bpak_header *hdr, uuid_t boot_part_uu);
void boot_driver_atf_jump(void);

#endif /* INCLUDE_BOOT_ATF_H */
