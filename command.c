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
#include <plat/defs.h>
#include <bpak/bpak.h>
#include <bpak/keystore.h>
#include <pb-tools/wire.h>
#include <uuid/uuid.h>

#ifdef CONFIG_AUTH_TOKEN
static int auth_token(struct pb_crypto *crypto,
                      struct bpak_keystore *keystore,
                      uint8_t *device_uu,
                      uint32_t key_id, uint8_t *sig, size_t size)
{
    int rc = -PB_ERR;
    char device_uu_str[37];
    struct bpak_key *k = NULL;
    struct pb_hash_context hash;

    uuid_unparse(device_uu, device_uu_str);

    for (int i = 0; i < keystore->no_of_keys; i++)
    {
        if (keystore->keys[i]->id == key_id)
        {
            k = keystore->keys[i];
            break;
        }
    }

    if (!k)
    {
        LOG_ERR("Key not found");
        return -PB_ERR;
    }

    LOG_DBG("Found key %x", k->id);

    int hash_kind = PB_HASH_INVALID;

    switch (k->kind)
    {
        case BPAK_KEY_PUB_PRIME256v1:
            hash_kind = PB_HASH_SHA256;
        break;
        case BPAK_KEY_PUB_SECP384r1:
            hash_kind = PB_HASH_SHA384;
        break;
        case BPAK_KEY_PUB_SECP521r1:
            hash_kind = PB_HASH_SHA512;
        break;
        case BPAK_KEY_PUB_RSA4096:
            hash_kind = PB_HASH_SHA256;
        break;
        default:
            LOG_ERR("Unkown key kind");
            return -PB_ERR;
    }

    LOG_DBG("Hash init");
    rc = pb_hash_init(crypto, &hash, hash_kind);

    if (rc != PB_OK)
        return rc;

    rc = pb_hash_update(&hash, NULL, 0);

    if (rc != PB_OK)
        return rc;

    LOG_DBG("Hash final");
    rc = pb_hash_finalize(&hash, device_uu_str, 36);

    if (rc != PB_OK)
        return rc;

    rc = pb_pk_verify(crypto, sig, size, &hash ,k);

    if (rc != PB_OK)
    {
        LOG_ERR("Authentication failed");
        return rc;
    }

    LOG_INFO("Authentication successful");

    return PB_OK;
}
#endif

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

    if (pb_wire_requires_auth(cmd) && (!ctx->authenticated))
    {
        LOG_ERR("Not authenticaated");
        pb_wire_init_result(&ctx->result, -PB_RESULT_NOT_AUTHENTICATED);
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
                drv->write(drv, result_tbl, bytes);
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

            if (ram_boot_cmd->verbose)
            {
                LOG_INFO("Verbose boot enabled");
            }

            pb_wire_init_result(&ctx->result, PB_RESULT_OK);
            rc = drv->write(drv, &ctx->result, sizeof(ctx->result));

            if (rc != PB_OK)
                break;

            rc = drv->read(drv, &ctx->boot->driver->load_ctx->header,
                            sizeof(struct bpak_header));

            if (rc != PB_OK)
                break;

            pb_wire_init_result(&ctx->result, rc);
            rc = drv->write(drv, &ctx->result, sizeof(ctx->result));

            if (rc != PB_OK)
                break;

            rc = pb_boot_load_transport(ctx->boot, ctx->transport);

            pb_wire_init_result(&ctx->result, rc);

            if (rc != PB_OK)
            {
                break;
            }

            drv->write(drv, &ctx->result, sizeof(ctx->result));
            pb_boot(ctx->boot, ctx->device_uuid, ram_boot_cmd->verbose);
            return -PB_ERR;

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
                sdrv->map_release(sdrv, ctx->stream_map);
                break;
            }

            rc = sdrv->map_release(sdrv, ctx->stream_map);

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

            rc = pb_boot_load_fs(ctx->boot, boot_cmd->uuid);
            pb_wire_init_result(&ctx->result, rc);

            drv->write(drv, &ctx->result, sizeof(ctx->result));

            if (rc != PB_OK)
                break;

            pb_boot(ctx->boot, ctx->device_uuid, boot_cmd->verbose);

            /* Should not return */
            return -PB_ERR;
        }
        break;
        case PB_CMD_DEVICE_IDENTIFIER_READ:
        {
            struct pb_result_device_identifier *ident = \
                (struct pb_result_device_identifier *) ctx->buffer;

            const char *board_id = board_get_id();
            memset(ident->board_id, 0, sizeof(ident->board_id));
            memcpy(ident->board_id, board_id, strlen(board_id));
            memcpy(ident->device_uuid, ctx->device_uuid, 16);

            pb_wire_init_result2(&ctx->result, rc,
                                    ident, sizeof(*ident));
        }
        break;
        case PB_CMD_PART_ACTIVATE:
        {
            struct pb_command_activate_part *activate_cmd = \
                (struct pb_command_activate_part *) cmd->request;

            rc = pb_boot_activate(ctx->boot, activate_cmd->uuid);

            pb_wire_init_result(&ctx->result, rc);
        }
        break;
        case PB_CMD_AUTHENTICATE:
        {
            struct pb_command_authenticate *auth_cmd = \
                (struct pb_command_authenticate *) cmd->request;

            LOG_DBG("Auth, method:%u sz:%i", auth_cmd->method, auth_cmd->size);
            pb_wire_init_result(&ctx->result, -PB_RESULT_NOT_SUPPORTED);

#ifdef CONFIG_AUTH_TOKEN
            if (auth_cmd->method == PB_AUTH_ASYM_TOKEN)
            {
                pb_wire_init_result(&ctx->result, PB_RESULT_OK);
                drv->write(drv, &ctx->result, sizeof(ctx->result));

                drv->read(drv, ctx->buffer, auth_cmd->size);

                rc = auth_token(ctx->crypto, ctx->keystore, ctx->device_uuid,
                        auth_cmd->key_id, ctx->buffer, auth_cmd->size);

                if (rc == PB_OK)
                    ctx->authenticated = true;
                else
                    ctx->authenticated = false;

                pb_wire_init_result(&ctx->result, rc);
            }
#endif


        }
        break;
        case PB_CMD_AUTH_SET_OTP_PASSWORD:
        {
#ifndef CONFIG_AUTH_PASSWORD
            pb_wire_init_result(&ctx->result, -PB_RESULT_NOT_SUPPORTED);
#endif
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
                  struct bpak_keystore *keystore,
                  struct pb_boot_context *boot,
                  uint8_t *device_uuid)
{
    memset(ctx, 0, sizeof(ctx));

    ctx->authenticated = false;
    ctx->transport = transport;
    ctx->storage = storage;
    ctx->crypto = crypto;
    ctx->keystore = keystore;
    ctx->boot = boot;
    ctx->device_uuid = device_uuid;

    return PB_OK;
}
