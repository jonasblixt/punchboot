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
#include <pb/transport.h>
#include <bpak/bpak.h>
#include <uuid/uuid.h>

#define PB_STATE_MAGIC 0x026d4a65

struct pb_boot_state /* 512 bytes */
{
    uint32_t magic;
    uint8_t private[468];
    uint8_t rz[36];
    uint32_t crc;
} __attribute__((packed));

struct pb_boot_driver;

typedef int (*pb_boot_dtb_call_t) (struct pb_boot_driver *boot,
                                    void *dtb, int offset);

typedef int (*pb_boot_call_t) (struct pb_boot_driver *boot);

typedef int (*pb_boot_call_uu_t) (struct pb_boot_driver *boot, uint8_t *uu);

struct pb_boot_driver
{
    uint32_t boot_image_id;
    uint32_t dtb_image_id;
    uint32_t ramdisk_image_id;
    pb_boot_dtb_call_t on_dt_patch_bootargs;
    pb_boot_dtb_call_t patch_dt;
    pb_boot_call_uu_t activate;
    pb_boot_call_t on_jump;
    pb_boot_call_t load_boot_state;
    pb_boot_call_t boot;
    void *device_tree;
    struct pb_storage *storage;
    struct bpak_keystore *keystore;
    struct pb_crypto *crypto;
    struct pb_boot_state *state;
    struct pb_boot_state *backup_state;
    const char *primary_state_uu;
    const char *backup_state_uu;
    bool update_boot_state;
    bool verbose_boot;
    struct pb_image_load_context *load_ctx;
    uintptr_t jump_addr;
    uint8_t boot_part_uu[16];
    void *private;
    size_t size;
};

struct pb_boot_context
{
    struct pb_boot_driver *driver;
};

int pb_boot_init(struct pb_boot_context *ctx,
                 struct pb_boot_driver *driver,
                 struct pb_storage *storage,
                 struct pb_crypto *crypto,
                 struct bpak_keystore *keystore);

int pb_boot_free(struct pb_boot_context *ctx);

int pb_boot_load_state(struct pb_boot_context *ctx);

int pb_boot_load_transport(struct pb_boot_context *ctx,
                           struct pb_transport *transport);

int pb_boot_load_fs(struct pb_boot_context *ctx, uint8_t *boot_part_uu);
int pb_boot(struct pb_boot_context *ctx,
            uint8_t *device_uuid,
            bool verbose);

int pb_boot_activate(struct pb_boot_context *ctx, uint8_t *uu);
#endif  // INCLUDE_PB_BOOT_H_
