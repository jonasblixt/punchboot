/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <uuid.h>
#include <boot/boot.h>
#include <bpak/bpak.h>

typedef int (*boot_patch_dtb_cb_t) (void *fdt, int offset);

struct boot_driver_linux_config
{
    bpak_id_t image_bpak_id;            /*!< BPAK id of bootable part */
    bpak_id_t dtb_bpak_id;              /*!< Optional BPAK id of a device tree part */
    bpak_id_t ramdisk_bpak_id;          /*!< Optional BPAK id of a ramdisk part */
    boot_patch_dtb_cb_t dtb_patch_cb;   /*!< Optional DTB patch callback */
    const char *(*resolve_part_name)(uuid_t part_uu);
    bool set_dtb_boot_arg;              /*!< Pass dtb address as first argument */
};

int boot_driver_linux_init(const struct boot_driver_linux_config *cfg);
int boot_driver_linux_prepare(struct bpak_header *hdr, uuid_t boot_part_uu);
void boot_driver_linux_jump(void);
