/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <pb/pb.h>
#include <pb/command.h>
#include <pb/image.h>
#include <pb/plat.h>
#include <pb/usb.h>
#include <pb/crypto.h>
#include <pb/io.h>
#include <pb/board.h>
#include <pb/gpt.h>
#include <pb/transport.h>
#include <pb/boot.h>
#include <pb/crypto.h>
#include <board/config.h>
#include <pb/config.h>
#include <plat/defs.h>
#include <bpak/bpak.h>
#include <bpak/keystore.h>
#include <pb-tools/wire.h>
#include <uuid/uuid.h>

#define RECOVERY_CMD_BUFFER_SZ  1024*64
#define RECOVERY_BULK_BUFFER_BLOCKS 16384
#define RECOVERY_BULK_BUFFER_SZ (RECOVERY_BULK_BUFFER_BLOCKS*512)
#define RECOVERY_MAX_PARAMS 128

static uint8_t __a4k __no_bss recovery_cmd_buffer[RECOVERY_CMD_BUFFER_SZ];
extern char _code_start, _code_end, _data_region_start, _data_region_end,
            _zero_region_start, _zero_region_end, _stack_start, _stack_end;

static struct pb_image_load_context load_ctx __no_bss __a4k;

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

int pb_command_parse(struct pb_command_context *ctx, struct pb_command *cmd)
{
    struct pb_transport_driver *drv = ctx->transport->driver;
    int rc = PB_OK;

    if (!pb_wire_valid_command(cmd))
    {
        LOG_ERR("Invalid command: %i\n", cmd->command);
        pb_wire_init_result(&ctx->result, -PB_RESULT_INVALID_COMMAND);
        goto err_out;
    }

    LOG_DBG("Parse command: %i", cmd->command);

    pb_wire_init_result(&ctx->result, -PB_RESULT_NOT_SUPPORTED);

    switch (cmd->command)
    {
        case PB_CMD_BOOTLOADER_VERSION_READ:
        {
            char version_string[30];

            LOG_INFO("Get version");
            snprintf(version_string, sizeof(version_string), "%s", PB_VERSION);

            pb_wire_init_result2(&ctx->result, PB_RESULT_OK, version_string,
                                                        strlen(version_string));
        }
        break;
        case PB_CMD_DEVICE_RESET:
        {
            LOG_INFO("Board reset");
            pb_wire_init_result(&ctx->result, PB_RESULT_OK);
            drv->write(drv, &ctx->result, sizeof(ctx->result));
            plat_reset();

            while (1)
            {
                __asm__ volatile("wfi");
            }
        }
        break;
        case PB_CMD_PART_TBL_READ:
        {
            LOG_DBG("TBL read %i", ctx->storage->no_of_drivers);
            struct pb_result_part_table_read tbl_read_result;

            struct pb_result_part_table_entry *result_tbl = \
                (struct pb_result_part_table_entry *) ctx->buffer;

            int entries = 0;

            for (int i = 0; i < ctx->storage->no_of_drivers; i++)
            {
                struct pb_storage_driver *sdrv = ctx->storage->drivers[i];

                pb_storage_foreach_part(sdrv->map->map_data, part)
                {
                    if (!part->valid_entry)
                        break;

                    if (!(part->flags & PB_STORAGE_MAP_FLAG_VISIBLE))
                        continue;

                    memcpy(result_tbl[entries].uuid, part->uuid, 16);
                    memcpy(result_tbl[entries].description, part->description,
                                    sizeof(part->description));
                    result_tbl[entries].first_block = part->first_block;
                    result_tbl[entries].last_block = part->last_block;
                    result_tbl[entries].block_size = sdrv->block_size;
                    result_tbl[entries].flags = (part->flags & 0xFF);

                    entries++;
                }
            }

            tbl_read_result.no_of_entries = entries;
            LOG_DBG("%i entries", entries);
            pb_wire_init_result2(&ctx->result, PB_RESULT_OK, &tbl_read_result,
                                                     sizeof(tbl_read_result));

            drv->write(drv, &ctx->result, sizeof(ctx->result));

            if (entries)
            {
                size_t bytes = sizeof(struct pb_result_part_table_entry)*(entries);
                LOG_DBG("Bytes %li", bytes);
                memcpy(recovery_cmd_buffer, result_tbl, bytes);
                drv->write(drv, recovery_cmd_buffer, bytes);
            }

            pb_wire_init_result(&ctx->result, PB_RESULT_OK);
        }
        break;
        case PB_CMD_DEVICE_READ_CAPS:
        {
            struct pb_result_device_caps caps;
            caps.stream_no_of_buffers = 2;
            caps.stream_buffer_size = 1024;
            caps.chunk_transfer_max_bytes = 1024*1024*4;

            pb_wire_init_result2(&ctx->result, PB_RESULT_OK, &caps, sizeof(caps));
        }
        break;
        case PB_CMD_BOOT_RAM:
        {
            LOG_DBG("RAM boot");
            struct pb_command_ram_boot *ram_boot_cmd = \
                               (struct pb_command_ram_boot *) cmd->request;

            load_ctx.read = ram_load_read;
            load_ctx.result = ram_load_result;
            load_ctx.private = ctx->transport;
            load_ctx.chunk_size = 1024*1024*4;

            drv->read(drv, &load_ctx.header, sizeof(load_ctx.header));
            pb_wire_init_result(&ctx->result, PB_RESULT_OK);
            drv->write(drv, &ctx->result, sizeof(ctx->result));

            rc = pb_image_load(&load_ctx, ctx->crypto, ctx->keystore);
            pb_wire_init_result(&ctx->result, rc);
        }
        break;
        case PB_CMD_PART_TBL_INSTALL:
        {
            rc = pb_storage_install_default(ctx->storage);
        }
        break;
        case PB_CMD_STREAM_INITIALIZE:
        {
            struct pb_command_stream_initialize *stream_init = \
                    (struct pb_command_stream_initialize *) cmd->request;
            char uuid[37];
            uuid_unparse(stream_init->part_uuid, uuid);
            LOG_DBG("Stream init %s", uuid);

            rc = pb_storage_get_part(ctx->storage, stream_init->part_uuid,
                                     &ctx->stream_map,
                                     &ctx->stream_drv);

            struct pb_storage_driver *sdrv = ctx->stream_drv;

            if (rc == PB_OK)
                rc = sdrv->map_request(sdrv, ctx->stream_map);

            pb_wire_init_result(&ctx->result, rc);
        }
        break;
        case PB_CMD_STREAM_PREPARE_BUFFER:
        {
            struct pb_command_stream_prepare_buffer *stream_prep = \
                (struct pb_command_stream_prepare_buffer *) cmd->request;

            LOG_DBG("Stream prep %u, %i", stream_prep->size, stream_prep->id);

            if (stream_prep->size > ctx->buffer_size)
            {
                pb_wire_init_result(&ctx->result, -PB_RESULT_NO_MEMORY);
                break;
            }

            if (stream_prep->id > (ctx->no_of_buffers-1))
            {
                pb_wire_init_result(&ctx->result, -PB_RESULT_NO_MEMORY);
                break;
            }

            pb_wire_init_result(&ctx->result, PB_RESULT_OK);
            drv->write(drv, &ctx->result, sizeof(ctx->result));

            uint8_t *bfr = ((uint8_t *) ctx->buffer) +
                            (ctx->buffer_size*stream_prep->id);

            rc = drv->read(drv, bfr, stream_prep->size);
        }
        break;
        case PB_CMD_STREAM_WRITE_BUFFER:
        {
            struct pb_command_stream_write_buffer *stream_write = \
                (struct pb_command_stream_write_buffer *) cmd->request;

            struct pb_storage_driver *sdrv = ctx->stream_drv;

            LOG_DBG("Stream write %u, %llu, %i", stream_write->buffer_id,
                                                stream_write->offset,
                                                stream_write->size);

            size_t part_size = ctx->stream_map->no_of_blocks *
                                 ctx->stream_drv->block_size;

            if ((stream_write->offset + stream_write->size) > part_size)
            {
                LOG_ERR("Trying to write outside of partition");
                LOG_ERR("%llu > %lu", (stream_write->offset + \
                                stream_write->size), part_size);
                rc = -PB_RESULT_NO_MEMORY;
                pb_wire_init_result(&ctx->result, rc);
                break;
            }

            size_t blocks = (stream_write->size / sdrv->block_size);
            size_t block_offset = (stream_write->offset / sdrv->block_size);

            LOG_DBG("Writing %lu blocks to offset %lu", blocks, block_offset);

            uint8_t *bfr = ((uint8_t *) ctx->buffer) +
                            (ctx->buffer_size*stream_write->buffer_id);

            LOG_DBG("Buffer: %p", bfr);
            rc = pb_storage_write(sdrv, ctx->stream_map, bfr, blocks,
                                    block_offset);

            pb_wire_init_result(&ctx->result, rc);
        }
        break;
        case PB_CMD_STREAM_FINALIZE:
        {
            LOG_DBG("Stream final");
            struct pb_storage_driver *sdrv = ctx->stream_drv;

            rc = sdrv->map_release(sdrv, ctx->stream_map);
            pb_wire_init_result(&ctx->result, rc);
        }
        break;
        case PB_CMD_PART_BPAK_READ:
        {
            struct pb_command_read_bpak *read_cmd = \
                (struct pb_command_read_bpak *) cmd->request;

            LOG_DBG("Read bpak");

            rc = pb_storage_get_part(ctx->storage, read_cmd->uuid,
                                     &ctx->stream_map,
                                     &ctx->stream_drv);

            if (rc != PB_OK)
            {
                LOG_ERR("Could not find partition");
                pb_wire_init_result(&ctx->result, rc);
                break;
            }

            struct pb_storage_driver *sdrv = ctx->stream_drv;
            struct pb_storage_map *map = ctx->stream_map;


            size_t blocks = sizeof(struct bpak_header) / sdrv->block_size;
            size_t block_offset = map->no_of_blocks - blocks - 1;

            rc = sdrv->map_request(sdrv, ctx->stream_map);

            if (rc != PB_OK)
            {
                pb_wire_init_result(&ctx->result, rc);
                break;
            }

            LOG_DBG("Reading");

            rc = pb_storage_read(sdrv, map, ctx->buffer,
                                    blocks, block_offset);

            if (rc != PB_OK)
            {
                pb_wire_init_result(&ctx->result, rc);
                sdrv->map_request(sdrv, ctx->stream_map);
                break;
            }

            rc = sdrv->map_request(sdrv, ctx->stream_map);

            if (rc != PB_OK)
            {
                pb_wire_init_result(&ctx->result, rc);
                break;
            }

            rc = bpak_valid_header((struct bpak_header *) ctx->buffer);

            if (rc != BPAK_OK)
            {
                LOG_ERR("Invalid bpak header");
                pb_wire_init_result(&ctx->result, -PB_RESULT_NOT_FOUND);
                break;
            }

            pb_wire_init_result(&ctx->result, rc);
            drv->write(drv, &ctx->result, sizeof(ctx->result));

            rc = drv->write(drv, ctx->buffer, sizeof(struct bpak_header));

            pb_wire_init_result(&ctx->result, rc);
        }
        break;
        case PB_CMD_PART_VERIFY:
        {
            struct pb_command_verify_part *verify_cmd = \
                (struct pb_command_verify_part *) cmd->request;

            LOG_DBG("Verify part");

            rc = pb_storage_get_part(ctx->storage, verify_cmd->uuid,
                                     &ctx->stream_map,
                                     &ctx->stream_drv);

            if (rc != PB_OK)
            {
                LOG_ERR("Could not find partition");
                pb_wire_init_result(&ctx->result, rc);
                break;
            }

            struct pb_storage_driver *sdrv = ctx->stream_drv;
            struct pb_storage_map *map = ctx->stream_map;

            size_t blocks_to_check = verify_cmd->size / sdrv->block_size;

            size_t blocks = sizeof(struct bpak_header) / sdrv->block_size;
            size_t block_offset = map->no_of_blocks - blocks - 1;

            rc = sdrv->map_request(sdrv, ctx->stream_map);

            if (rc != PB_OK)
            {
                pb_wire_init_result(&ctx->result, rc);
                break;
            }

            rc = pb_hash_init(ctx->crypto, &ctx->hash_ctx, PB_HASH_SHA256);

            if (rc != PB_OK)
            {
                pb_wire_init_result(&ctx->result, rc);
                break;
            }

            if (verify_cmd->bpak)
            {
                LOG_DBG("Bpak header");
                rc = pb_storage_read(sdrv, map, ctx->buffer,
                                        blocks, block_offset);

                if (rc != PB_OK)
                {
                    pb_wire_init_result(&ctx->result, rc);
                    sdrv->map_request(sdrv, ctx->stream_map);
                    break;
                }

                rc = pb_hash_update(&ctx->hash_ctx, ctx->buffer,
                                        sizeof(struct bpak_header));

                if (rc != PB_OK)
                {
                    pb_wire_init_result(&ctx->result, rc);
                    break;
                }

                blocks_to_check -= blocks;
            }

            LOG_DBG("Reading %lu blocks", blocks_to_check);
            block_offset = 0;

            while (blocks_to_check)
            {
                blocks = blocks_to_check>(ctx->buffer_size/sdrv->block_size)? \
                           (ctx->buffer_size/sdrv->block_size):blocks_to_check;

                rc = pb_storage_read(sdrv, map, ctx->buffer,
                                        blocks, block_offset);

                if (rc != PB_OK)
                {
                    pb_wire_init_result(&ctx->result, rc);
                    sdrv->map_request(sdrv, ctx->stream_map);
                    break;
                }

                rc = pb_hash_update(&ctx->hash_ctx, ctx->buffer,
                                            (blocks*sdrv->block_size));

                if (rc != PB_OK)
                {
                    pb_wire_init_result(&ctx->result, rc);
                    break;
                }

                blocks_to_check -= blocks;
                block_offset += blocks;
            }

            if (rc != PB_OK)
                break;

            rc = pb_hash_finalize(&ctx->hash_ctx, NULL, 0);

            if (rc != PB_OK)
            {
                pb_wire_init_result(&ctx->result, rc);
                break;
            }

            rc = sdrv->map_request(sdrv, ctx->stream_map);

            if (rc != PB_OK)
            {
                pb_wire_init_result(&ctx->result, rc);
                break;
            }

            if (memcmp(ctx->hash_ctx.buf, verify_cmd->sha256, 32) == 0)
                rc = PB_RESULT_OK;
            else
                rc = -PB_RESULT_PART_VERIFY_FAILED;

            pb_wire_init_result(&ctx->result, rc);
        }
        break;
        case PB_CMD_BOOT_PART:
        {
            struct pb_command_boot_part *boot_cmd = \
                (struct pb_command_boot_part *) cmd->request;

            rc = pb_storage_get_part(ctx->storage, boot_cmd->uuid,
                                     &ctx->stream_map,
                                     &ctx->stream_drv);

            if (rc != PB_OK)
            {
                LOG_ERR("Could not find partition");
                pb_wire_init_result(&ctx->result, rc);
                break;
            }

            if (!(ctx->stream_map->flags & PB_STORAGE_MAP_FLAG_BOOTABLE))
            {
                LOG_ERR("Partition not bootable");
                pb_wire_init_result(&ctx->result, -PB_RESULT_PART_NOT_BOOTABLE);
                break;
            }

            struct pb_storage_driver *sdrv = ctx->stream_drv;
            struct pb_storage_map *map = ctx->stream_map;

            size_t blocks = sizeof(struct bpak_header) / sdrv->block_size;
            size_t block_offset = map->no_of_blocks - blocks - 1;

            rc = pb_storage_read(sdrv, map, &load_ctx.header,
                                    blocks, block_offset);

            if (rc != PB_OK)
            {
                pb_wire_init_result(&ctx->result, rc);
                sdrv->map_request(sdrv, ctx->stream_map);
                break;
            }

            struct fs_load_private fs_priv;

            fs_priv.sdrv = sdrv;
            fs_priv.map = map;
            fs_priv.block_offset = 0;

            load_ctx.read = fs_load_read;
            load_ctx.result = NULL;
            load_ctx.private = &fs_priv;
            load_ctx.chunk_size = 1024*1024*4;

            rc = pb_image_load(&load_ctx, ctx->crypto, ctx->keystore);

            pb_wire_init_result(&ctx->result, rc);
        }
        break;
        case PB_CMD_DEVICE_IDENTIFIER_READ:
        {
            struct pb_result_device_identifier *ident = \
                (struct pb_result_device_identifier *) ctx->buffer;

            const char *board_id = board_get_id();
            memset(ident->board_id, 0, sizeof(ident->board_id));
            memcpy(ident->board_id, board_id, strlen(board_id));

            rc = plat_get_uuid(ctx->crypto, ident->device_uuid);

            pb_wire_init_result2(&ctx->result, rc,
                                    ident, sizeof(*ident));
        }
        break;
        default:
        {
            LOG_ERR("Got unknown command: %u", cmd->command);
            pb_wire_init_result(&ctx->result, -PB_RESULT_INVALID_COMMAND);
        }
    }

err_out:
    drv->write(drv, &ctx->result, sizeof(ctx->result));
    return rc;
}

int pb_command_init(struct pb_command_context *ctx,
                  struct pb_transport *transport,
                  struct pb_storage *storage,
                  struct pb_crypto *crypto,
                  struct bpak_keystore *keystore)
{
    memset(ctx, 0, sizeof(ctx));

    ctx->transport = transport;
    ctx->storage = storage;
    ctx->crypto = crypto;
    ctx->keystore = keystore;

    return PB_OK;
}
