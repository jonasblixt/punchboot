/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_BOOT_H_
#define INCLUDE_PB_BOOT_H_

#include <stdint.h>
#include <pb/image.h>
#include <pb/storage.h>
#include <bpak/bpak.h>
#include <uuid/uuid.h>

struct pb_boot_driver;

typedef int (*pb_boot_dtb_call_t) (struct pb_boot_driver *boot,
                                    void *dtb, int offset);

typedef int (*pb_boot_call_t) (struct pb_boot_driver *boot);


struct pb_boot_driver
{
    struct bpak_header header;
    uint32_t boot_image_id;
    uint32_t dtb_image_id;
    uint32_t ramdisk_image_id;
    pb_boot_dtb_call_t on_dt_patch_bootargs;
    pb_boot_dtb_call_t on_dt_patch_ramdisk;
    pb_boot_call_t on_jump;
    pb_boot_call_t boot;
    void *device_tree;
    void *private;
    size_t size;
};

struct pb_boot_context
{
    struct pb_boot_driver *driver;
};

int pb_boot_init(struct pb_boot_context *ctx,
                 struct pb_boot_driver *driver,
                 struct pb_storage *storage);

int pb_boot_free(struct pb_boot_context *ctx);
int pb_boot_set_active(struct pb_boot_context *ctx, uint8_t *uu);
int pb_boot_is_active(struct pb_boot_context *ctx, uint8_t *uu, bool *active);
int pb_boot(struct pb_boot_context *ctx, uint8_t *uu, bool verbose, bool force);


#endif  // INCLUDE_PB_BOOT_H_
