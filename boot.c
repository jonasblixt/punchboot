
/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <pb/pb.h>
#include <pb/io.h>
#include <pb/boot.h>
#include <board/config.h>
#include <pb/crypto.h>
#include <pb/board.h>
#include <pb/plat.h>
#include <pb/atf.h>
#include <pb/crc.h>
#include <pb/transport.h>
#include <pb/timing_report.h>
#include <libfdt.h>
#include <uuid/uuid.h>
#include <bpak/bpak.h>

static int ram_load_read(struct pb_image_load_context *ctx,
                            void *buf, size_t size)
{
    struct pb_transport *transport = (struct pb_transport *) ctx->private;
    struct pb_transport_driver *drv = transport->driver;

    LOG_DBG("Read %lu bytes", size);
    return drv->read(drv, buf, size);
}

static int ram_load_result(struct pb_image_load_context *ctx, int rc)
{
    struct pb_transport *transport = (struct pb_transport *) ctx->private;
    struct pb_transport_driver *drv = transport->driver;

    pb_wire_init_result(&ctx->result_data, rc);
    return drv->write(drv, &ctx->result_data, sizeof(ctx->result_data));
}

struct fs_load_private
{
    struct pb_storage_driver *sdrv;
    struct pb_storage_map *map;
    size_t block_offset;
};

static int fs_load_read(struct pb_image_load_context *ctx,
                            void *buf, size_t size)
{
    struct fs_load_private *priv = (struct fs_load_private *) ctx->private;
    int rc;
    size_t blocks = size / priv->sdrv->block_size;

    LOG_DBG("Read %lu blocks", blocks);

    rc = pb_storage_read(priv->sdrv, priv->map, buf,
                            blocks, priv->block_offset);

    priv->block_offset += blocks;
    return rc;
}

static int pb_boot_state_validate(struct pb_boot_state *state)
{
    uint32_t crc = state->crc;
    int err = PB_OK;

    state->crc = 0;

    if (state->magic != PB_STATE_MAGIC)
    {
        LOG_ERR("Incorrect magic");
        err = -PB_ERR;
        goto config_err_out;
    }

    if (crc != crc32(0, (uint8_t *) state, sizeof(struct pb_boot_state)))
    {
        LOG_ERR("CRC failed");
        err =- PB_ERR;
        goto config_err_out;
    }

config_err_out:
    return err;
}

static int pb_boot_state_defaults(struct pb_boot_state *state)
{
    memset(state, 0, sizeof(struct pb_boot_state));
    state->magic = PB_STATE_MAGIC;
    return PB_OK;
}

static int pb_boot_state_commit(struct pb_boot_driver *drv,
                         struct pb_storage_driver *sdrv,
                         struct pb_storage_map *primary_map,
                         struct pb_storage_map *backup_map)
{
    int rc;

    drv->state->crc = 0;
    uint32_t crc = crc32(0, (const uint8_t *) drv->state,
                                sizeof(struct pb_boot_state));
    drv->state->crc = crc;

    memcpy(drv->backup_state, drv->state, sizeof(struct pb_boot_state));

    rc = pb_storage_write(sdrv, primary_map, drv->state,
                       (sizeof(struct pb_boot_state) / sdrv->block_size), 0);

    if (rc != PB_OK)
        goto config_commit_err;

    rc = pb_storage_write(sdrv, backup_map, drv->backup_state,
                       (sizeof(struct pb_boot_state) / sdrv->block_size), 0);

config_commit_err:

    if (rc != PB_OK)
    {
        LOG_ERR("Could not write boot state");
    }
    else
    {
        LOG_INFO("Boot state written");
    }

    return rc;
}

static int pb_boot_state_init(struct pb_boot_driver *drv,
                              struct pb_storage_driver *sdrv,
                              struct pb_storage_map *primary_map,
                              struct pb_storage_map *backup_map)
{
    int rc;
    bool primary_state_ok = false;
    bool backup_state_ok = false;


    rc = pb_storage_read(sdrv, primary_map, drv->state,
                       (sizeof(struct pb_boot_state) / sdrv->block_size), 0);


    rc = pb_boot_state_validate(drv->state);

    if (rc == PB_OK)
    {
        primary_state_ok = true;
    }
    else
    {
        LOG_ERR("Primary boot state data corrupt");
        primary_state_ok = false;
    }

    rc = pb_storage_read(sdrv, backup_map, drv->backup_state,
                       (sizeof(struct pb_boot_state) / sdrv->block_size), 0);


    rc = pb_boot_state_validate(drv->backup_state);

    if (rc == PB_OK)
    {
        backup_state_ok = true;
    }
    else
    {
        LOG_ERR("Backup boot state data corrupt");
        backup_state_ok = false;
    }

    if (!primary_state_ok && !backup_state_ok)
    {
        LOG_ERR("No valid state found, installing default");
        pb_boot_state_defaults(drv->state);
        pb_boot_state_defaults(drv->backup_state);
        rc = pb_boot_state_commit(drv, sdrv, primary_map, backup_map);
    }
    else if (!backup_state_ok && primary_state_ok)
    {
        LOG_ERR("Backup state corrupt, repairing");
        pb_boot_state_defaults(drv->backup_state);
        rc = pb_boot_state_commit(drv, sdrv, primary_map, backup_map);
    }
    else if (backup_state_ok && !primary_state_ok)
    {
        LOG_ERR("Primary state corrupt, reparing");
        memcpy(drv->state, drv->backup_state, sizeof(struct pb_boot_state));
        rc = pb_boot_state_commit(drv, sdrv, primary_map, backup_map);
    }
    else
    {
        LOG_INFO("Boot state loaded");
        rc = PB_OK;
    }

    return rc;
}

int pb_boot_init(struct pb_boot_context *ctx,
                 struct pb_boot_driver *driver,
                 struct pb_storage *storage,
                 struct pb_crypto *crypto,
                 struct bpak_keystore *keystore)
{
    ctx->driver = driver;
    ctx->driver->storage = storage;
    ctx->driver->crypto = crypto;
    ctx->driver->keystore = keystore;
    driver->update_boot_state = false;
    return PB_OK;
}

int pb_boot_free(struct pb_boot_context *ctx)
{
    return PB_OK;
}

int pb_boot_load_state(struct pb_boot_context *ctx)
{
    int rc;

    uuid_t primary_uu;
    uuid_t backup_uu;
    struct pb_storage_map *primary_map, *backup_map;
    struct pb_storage_driver *sdrv;
    struct pb_boot_driver *drv = ctx->driver;

    uuid_parse(drv->primary_state_uu, primary_uu);
    uuid_parse(drv->backup_state_uu, backup_uu);

    rc = pb_storage_get_part(drv->storage, primary_uu, &primary_map, &sdrv);

    if (rc != PB_OK)
    {
        LOG_ERR("Could not find primary state partition");
        return rc;
    }

    rc = pb_storage_get_part(drv->storage, backup_uu, &backup_map, &sdrv);

    if (rc != PB_OK)
    {
        LOG_ERR("Could not find backup state partition");
        return rc;
    }

    rc = pb_boot_state_init(drv, sdrv, primary_map, backup_map);

    if (rc != PB_OK)
        return rc;

    if (drv->load_boot_state)
    {
        rc = drv->load_boot_state(drv);

        if (drv->update_boot_state)
        {
            pb_boot_state_commit(drv, sdrv, primary_map, backup_map);
            drv->update_boot_state = false;
        }

        if (rc != PB_OK)
            return rc;
    }

    return rc;
}

int pb_boot_load_fs(struct pb_boot_context *ctx, uint8_t *boot_part_uu)
{
    int rc;
    struct pb_storage_driver *stream_drv;
    struct pb_storage_map *stream_map;
    uint8_t *part_uu = NULL;

    if (!boot_part_uu)
        part_uu = ctx->driver->boot_part_uu;
    else
        part_uu = boot_part_uu;

    rc = pb_storage_get_part(ctx->driver->storage, part_uu,
                             &stream_map,
                             &stream_drv);

    if (rc != PB_OK)
    {
        LOG_ERR("Could not find partition");
        return rc;
    }

    if (!(stream_map->flags & PB_STORAGE_MAP_FLAG_BOOTABLE))
    {
        LOG_ERR("Partition not bootable");
        return -PB_RESULT_PART_NOT_BOOTABLE;
    }

    struct pb_storage_driver *sdrv = stream_drv;
    struct pb_storage_map *map = stream_map;

    size_t blocks = sizeof(struct bpak_header) / sdrv->block_size;
    size_t block_offset = map->no_of_blocks - blocks;

    rc = pb_storage_read(sdrv, map, &ctx->driver->load_ctx->header,
                            blocks, block_offset);

    if (rc != PB_OK)
        return rc;

    struct fs_load_private fs_priv;

    fs_priv.sdrv = sdrv;
    fs_priv.map = map;
    fs_priv.block_offset = 0;

    ctx->driver->load_ctx->read = fs_load_read;
    ctx->driver->load_ctx->result = NULL;
    ctx->driver->load_ctx->private = &fs_priv;
    ctx->driver->load_ctx->chunk_size = 1024*1024*4;

    return pb_image_load(ctx->driver->load_ctx,
                         ctx->driver->crypto, ctx->driver->keystore);
}

int pb_boot_load_transport(struct pb_boot_context *ctx,
                           struct pb_transport *transport)
{
    struct pb_boot_driver *bdrv = ctx->driver;

    bdrv->load_ctx->read = ram_load_read;
    bdrv->load_ctx->result = ram_load_result;
    bdrv->load_ctx->private = transport;
    bdrv->load_ctx->chunk_size = 1024*1024*4;

    return pb_image_load(bdrv->load_ctx, bdrv->crypto, bdrv->keystore);
}

int pb_boot(struct pb_boot_context *ctx, uint8_t *device_uuid, bool verbose)
{
    int rc;
    struct pb_boot_driver *drv = ctx->driver;
    uintptr_t *entry = 0;
    uintptr_t *dtb = 0;
    uintptr_t *ramdisk = 0;

    drv->verbose_boot = verbose;

    if (!drv->boot_image_id)
        return -PB_ERR;

                                                   /* pb-load-addr*/
    rc = bpak_get_meta_with_ref(&drv->load_ctx->header, 0xd1e64a4b,
                                drv->boot_image_id, (void **) &entry);

    if (rc != BPAK_OK)
        return -PB_ERR;

    LOG_INFO("Boot entry: 0x%lx", *entry);

    drv->jump_addr = *entry;

    if (drv->ramdisk_image_id)
    {
        rc = bpak_get_meta_with_ref(&drv->load_ctx->header, 0xd1e64a4b,
                                drv->ramdisk_image_id, (void **) &ramdisk);

        if (rc == PB_OK)
        {
            LOG_INFO("Ramdisk: 0x%lx", *ramdisk);
        }
    }

    rc = bpak_get_meta_with_ref(&drv->load_ctx->header, 0xd1e64a4b,
                                drv->dtb_image_id, (void **) &dtb);

    /* Prepare device tree */
    if (rc == BPAK_OK)
    {
        LOG_INFO("DTB: 0x%lx", *dtb);

        /* Locate the chosen node */
        void *fdt = (void *)(uintptr_t) *dtb;
        rc = fdt_check_header(fdt);

        if (rc < 0)
        {
            LOG_ERR("Invalid device tree");
            return -PB_ERR;
        }

        int depth = 0;
        int offset = 0;
        bool found_chosen_node = false;

        for (;;)
        {
            offset = fdt_next_node(fdt, offset, &depth);

            if (offset < 0)
                break;

            const char *name = fdt_get_name(fdt, offset, NULL);

            if (!name)
                continue;

            if (strcmp(name, "chosen") == 0)
            {
                found_chosen_node = true;
                break;
            }

        }

        if (!found_chosen_node)
        {
            LOG_ERR("Could not locate chosen node");
            return -PB_ERR;
        }

        char device_uuid_str[37];
        uuid_unparse(device_uuid, device_uuid_str);

        LOG_DBG("Device UUID: %s", device_uuid_str);
        fdt_setprop_string((void *) fdt, offset, "device-uuid",
                    (const char *) device_uuid_str);

        if (drv->on_dt_patch_bootargs)
        {
            LOG_DBG("Updating bootargs");
            rc = drv->on_dt_patch_bootargs(drv, fdt, offset);

            if (rc != 0)
                return -PB_ERR;
        }

        if (ramdisk)
        {
            struct bpak_part_header *pramdisk = NULL;

            rc = bpak_get_part(&drv->load_ctx->header,
                               drv->ramdisk_image_id,
                               &pramdisk);

            if (rc != BPAK_OK)
            {
                LOG_ERR("Could not read ramdisk metadata");
                return -PB_ERR;
            }

            size_t ramdisk_bytes = bpak_part_size(pramdisk);

            rc = fdt_setprop_u32((void *) fdt, offset, "linux,initrd-start",
                                *ramdisk);

            if (rc)
            {
                LOG_ERR("Could not patch initrd");
                return -PB_ERR;
            }

            rc = fdt_setprop_u32((void *) fdt, offset, "linux,initrd-end",
                                                     *ramdisk + ramdisk_bytes);

            if (rc)
            {
                LOG_ERR("Could not patch initrd");
                return -PB_ERR;
            }

            LOG_DBG("Ramdisk %lx -> %lx", *ramdisk, *ramdisk + ramdisk_bytes);
        }

        rc = drv->patch_dt(drv, fdt, offset);
    }

    return drv->boot(drv);
}

int pb_boot_activate(struct pb_boot_context *ctx, uint8_t *uu)
{
    int rc;

    uuid_t primary_uu;
    uuid_t backup_uu;
    struct pb_storage_map *primary_map, *backup_map;
    struct pb_storage_driver *sdrv;
    struct pb_boot_driver *drv = ctx->driver;

    uuid_parse(drv->primary_state_uu, primary_uu);
    uuid_parse(drv->backup_state_uu, backup_uu);

    rc = pb_storage_get_part(drv->storage, primary_uu, &primary_map, &sdrv);

    if (rc != PB_OK)
    {
        LOG_ERR("Could not find primary state partition");
        return rc;
    }

    rc = pb_storage_get_part(drv->storage, backup_uu, &backup_map, &sdrv);

    if (rc != PB_OK)
    {
        LOG_ERR("Could not find backup state partition");
        return rc;
    }

    if (ctx->driver->activate)
    {
        rc = ctx->driver->activate(ctx->driver, uu);

        if (rc != PB_OK)
            return rc;

        rc = pb_boot_state_commit(ctx->driver, sdrv, primary_map, backup_map);
    }

    return rc;
}
