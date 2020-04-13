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
#include <pb/image.h>
#include <pb/plat.h>
#include <pb/usb.h>
#include <pb/io.h>
#include <pb/board.h>
#include <pb/gpt.h>
#include <pb/boot.h>
#include <pb/keystore.h>
#include <pb/crypto.h>
#include <pb/command.h>
#include <plat/defs.h>
#include <bpak/bpak.h>
#include <bpak/keystore.h>
#include <pb-tools/wire.h>
#include <uuid/uuid.h>

static struct pb_command cmd __a4k __no_bss;
static struct pb_result result __a4k __no_bss;
static bool authenticated = false;
static uint8_t buffer[2][CONFIG_CMD_BUF_SIZE_KB*1024] __no_bss __a4k;
static enum pb_slc slc;
static struct pb_storage_map *stream_map;
static struct pb_storage_driver *stream_drv;
static struct pb_hash_context hash_ctx __no_bss __a4k;
static uint8_t device_uuid[16];

#ifdef CONFIG_AUTH_TOKEN
static int auth_token(uint8_t *device_uu,
                      uint32_t key_id, uint8_t *sig, size_t size)
{
    int rc = -PB_ERR;
    char device_uu_str[37];
    struct bpak_keystore *keystore = pb_keystore();
    struct bpak_key *k = NULL;

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

    LOG_DBG("Hash init (%s)", device_uu_str);
    rc = plat_hash_init(&hash_ctx, hash_kind);

    if (rc != PB_OK)
        return rc;

    rc = plat_hash_update(&hash_ctx, NULL, 0);

    if (rc != PB_OK)
        return rc;

    LOG_DBG("Hash final");
    rc = plat_hash_finalize(&hash_ctx, device_uu_str, 36);

    if (rc != PB_OK)
        return rc;

    rc = plat_pk_verify(sig, size, &hash_ctx ,k);

    if (rc != PB_OK)
    {
        LOG_ERR("Authentication failed");
        return rc;
    }

    LOG_INFO("Authentication successful");

    return PB_OK;
}
#endif

static int pb_command_parse(void)
{
    int rc = PB_OK;

    if (!pb_wire_valid_command(&cmd))
    {
        LOG_ERR("Invalid command: %i\n", cmd.command);
        pb_wire_init_result(&result, -PB_RESULT_INVALID_COMMAND);
        goto err_out;
    }

    if (pb_wire_requires_auth(&cmd) &&
             (!authenticated) &&
        (slc == PB_SLC_CONFIGURATION_LOCKED))
    {
        LOG_ERR("Not authenticaated");
        pb_wire_init_result(&result, -PB_RESULT_NOT_AUTHENTICATED);
        goto err_out;
    }

    LOG_DBG("Parse command: %i", cmd.command);

    pb_wire_init_result(&result, -PB_RESULT_NOT_SUPPORTED);

    switch (cmd.command)
    {
        case PB_CMD_BOOTLOADER_VERSION_READ:
        {
            char version_string[30];

            LOG_INFO("Get version");
            snprintf(version_string, sizeof(version_string), "%s", PB_VERSION);

            pb_wire_init_result2(&result, PB_RESULT_OK, version_string,
                                                        strlen(version_string));
        }
        break;
        case PB_CMD_DEVICE_RESET:
        {
            LOG_INFO("Board reset");
            pb_wire_init_result(&result, PB_RESULT_OK);
            plat_transport_write(&result, sizeof(result));
            plat_reset();

            while (1)
            {
                __asm__ volatile("wfi");
            }
        }
        break;
        case PB_CMD_PART_TBL_READ:
        {
            LOG_DBG("TBL read");
            struct pb_result_part_table_read tbl_read_result;

            struct pb_result_part_table_entry *result_tbl = \
                (struct pb_result_part_table_entry *) buffer;

            int entries = 0;

            for (struct pb_storage_driver *sdrv = pb_storage_get_drivers();
                    sdrv; sdrv = sdrv->next)
            {
                pb_storage_foreach_part(sdrv->map_data, part)
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
            pb_wire_init_result2(&result, PB_RESULT_OK, &tbl_read_result,
                                                     sizeof(tbl_read_result));

            plat_transport_write(&result, sizeof(result));

            if (entries)
            {
                size_t bytes = sizeof(struct pb_result_part_table_entry)*(entries);
                LOG_DBG("Bytes %zu", bytes);
                plat_transport_write(result_tbl, bytes);
            }

            pb_wire_init_result(&result, PB_RESULT_OK);
        }
        break;
        case PB_CMD_DEVICE_READ_CAPS:
        {
            struct pb_result_device_caps caps;
            caps.stream_no_of_buffers = 2;
            caps.stream_buffer_size = CONFIG_CMD_BUF_SIZE_KB*1024;
            caps.chunk_transfer_max_bytes = CONFIG_TRANSPORT_MAX_CHUNK_BYTES;

            pb_wire_init_result2(&result, PB_RESULT_OK, &caps, sizeof(caps));
        }
        break;
        case PB_CMD_BOOT_RAM:
        {
            LOG_DBG("RAM boot");
            struct pb_command_ram_boot *ram_boot_cmd = \
                               (struct pb_command_ram_boot *) &cmd.request;

            struct bpak_header *h = pb_image_header();
            if (ram_boot_cmd->verbose)
            {
                LOG_INFO("Verbose boot enabled");
            }

            pb_wire_init_result(&result, PB_RESULT_OK);
            rc = plat_transport_write(&result, sizeof(result));

            if (rc != PB_OK)
                break;

            rc = plat_transport_read(h,
                            sizeof(struct bpak_header));

            if (rc != PB_OK)
                break;

            rc = bpak_valid_header(h);

            if (rc != BPAK_OK)
            {
                LOG_ERR("Invalid BPAK header");
                rc = -PB_RESULT_ERROR;
                pb_wire_init_result(&result, rc);
                break;
            }

            rc = pb_image_check_header();

            if (rc != PB_OK)
            {
                LOG_ERR("Bad header");
                rc = -PB_RESULT_ERROR;
                pb_wire_init_result(&result, rc);
                break;
            }
/*
            pb_wire_init_result(&result, rc);
            rc = plat_transport_write(&result, sizeof(result));
*/
            if (rc != PB_OK)
                break;

            rc = pb_boot_load_transport();

            pb_wire_init_result(&result, rc);

            if (rc != PB_OK)
            {
                break;
            }

            plat_transport_write(&result, sizeof(result));
            pb_boot(ram_boot_cmd->verbose);
            return -PB_ERR;
        }
        break;
        case PB_CMD_PART_TBL_INSTALL:
        {
            rc = pb_storage_install_default();
            pb_wire_init_result(&result, rc);
        }
        break;
        case PB_CMD_STREAM_INITIALIZE:
        {
            struct pb_command_stream_initialize *stream_init = \
                    (struct pb_command_stream_initialize *) cmd.request;
            char uuid[37];
            uuid_unparse(stream_init->part_uuid, uuid);
            LOG_DBG("Stream init %s", uuid);

            rc = pb_storage_get_part(stream_init->part_uuid,
                                     &stream_map,
                                     &stream_drv);

            if (rc == PB_OK && stream_drv->map_request)
                rc = stream_drv->map_request(stream_drv, stream_map);

            pb_wire_init_result(&result, rc);
        }
        break;
        case PB_CMD_STREAM_PREPARE_BUFFER:
        {
            struct pb_command_stream_prepare_buffer *stream_prep = \
                (struct pb_command_stream_prepare_buffer *) cmd.request;

            LOG_DBG("Stream prep %u, %i", stream_prep->size, stream_prep->id);

            if (stream_prep->size > (CONFIG_CMD_BUF_SIZE_KB*1024))
            {
                pb_wire_init_result(&result, -PB_RESULT_NO_MEMORY);
                break;
            }

            if (stream_prep->id > 1)
            {
                pb_wire_init_result(&result, -PB_RESULT_NO_MEMORY);
                break;
            }

            pb_wire_init_result(&result, PB_RESULT_OK);
            plat_transport_write(&result, sizeof(result));

            uint8_t *bfr = ((uint8_t *) buffer) +
                            ((CONFIG_CMD_BUF_SIZE_KB*1024)*stream_prep->id);

            rc = plat_transport_read(bfr, stream_prep->size);
        }
        break;
        case PB_CMD_STREAM_WRITE_BUFFER:
        {
            struct pb_command_stream_write_buffer *stream_write = \
                (struct pb_command_stream_write_buffer *) cmd.request;

            struct pb_storage_driver *sdrv = stream_drv;

            LOG_DBG("Stream write %u, %llu, %i", stream_write->buffer_id,
                                                stream_write->offset,
                                                stream_write->size);

            size_t part_size = stream_map->no_of_blocks *
                                 stream_drv->block_size;

            if ((stream_write->offset + stream_write->size) > part_size)
            {
                LOG_ERR("Trying to write outside of partition");
                LOG_ERR("%llu > %zu", (stream_write->offset + \
                                stream_write->size), part_size);
                rc = -PB_RESULT_NO_MEMORY;
                pb_wire_init_result(&result, rc);
                break;
            }

            size_t blocks = (stream_write->size / sdrv->block_size);
            size_t block_offset = (stream_write->offset / sdrv->block_size);

            LOG_DBG("Writing %zu blocks to offset %zu", blocks, block_offset);

            uint8_t *bfr = ((uint8_t *) buffer) +
                      ((CONFIG_CMD_BUF_SIZE_KB*1024)*stream_write->buffer_id);

            LOG_DBG("Buffer: %p", bfr);
            rc = pb_storage_write(sdrv, stream_map, bfr, blocks,
                                    block_offset);

            pb_wire_init_result(&result, rc);
        }
        break;
        case PB_CMD_STREAM_FINALIZE:
        {
            LOG_DBG("Stream final");
            struct pb_storage_driver *sdrv = stream_drv;

            if (sdrv->map_release)
                rc = sdrv->map_release(sdrv, stream_map);
            else
                rc = PB_RESULT_OK;

            pb_wire_init_result(&result, rc);
        }
        break;
        case PB_CMD_PART_BPAK_READ:
        {
            struct pb_command_read_bpak *read_cmd = \
                (struct pb_command_read_bpak *) cmd.request;

            LOG_DBG("Read bpak");

            rc = pb_storage_get_part(read_cmd->uuid,
                                     &stream_map,
                                     &stream_drv);

            if (rc != PB_OK)
            {
                LOG_ERR("Could not find partition");
                pb_wire_init_result(&result, rc);
                break;
            }

            struct pb_storage_driver *sdrv = stream_drv;
            struct pb_storage_map *map = stream_map;


            size_t blocks = sizeof(struct bpak_header) / sdrv->block_size;

            if (blocks > map->no_of_blocks)
            {
                pb_wire_init_result(&result, -PB_RESULT_ERROR);
                break;
            }

            size_t block_offset = map->no_of_blocks - blocks;

            if (sdrv->map_request)
            {
                rc = sdrv->map_request(sdrv, stream_map);

                if (rc != PB_OK)
                {
                    pb_wire_init_result(&result, rc);
                    break;
                }
            }

            LOG_DBG("Reading, block offset: %zu", block_offset);

            rc = pb_storage_read(sdrv, map, buffer,
                                    blocks, block_offset);

            if (rc != PB_OK)
            {
                pb_wire_init_result(&result, rc);
                sdrv->map_release(sdrv, stream_map);
                break;
            }

            if (sdrv->map_release)
            {
                rc = sdrv->map_release(sdrv, stream_map);

                if (rc != PB_OK)
                {
                    pb_wire_init_result(&result, rc);
                    break;
                }
            }

            rc = bpak_valid_header((struct bpak_header *) buffer);

            if (rc != BPAK_OK)
            {
                LOG_ERR("Invalid bpak header");
                pb_wire_init_result(&result, -PB_RESULT_NOT_FOUND);
                break;
            }

            pb_wire_init_result(&result, rc);
            plat_transport_write(&result, sizeof(result));

            rc = plat_transport_write(buffer, sizeof(struct bpak_header));

            pb_wire_init_result(&result, rc);
        }
        break;
        case PB_CMD_PART_VERIFY:
        {
            struct pb_command_verify_part *verify_cmd = \
                (struct pb_command_verify_part *) cmd.request;

            LOG_DBG("Verify part");

            rc = pb_storage_get_part(verify_cmd->uuid,
                                     &stream_map,
                                     &stream_drv);

            if (rc != PB_OK)
            {
                LOG_ERR("Could not find partition");
                pb_wire_init_result(&result, rc);
                break;
            }

            struct pb_storage_driver *sdrv = stream_drv;
            struct pb_storage_map *map = stream_map;

            size_t blocks_to_check = verify_cmd->size / sdrv->block_size;

            size_t blocks = sizeof(struct bpak_header) / sdrv->block_size;
            size_t block_offset = map->no_of_blocks - blocks;

            if (sdrv->map_request)
            {
                rc = sdrv->map_request(sdrv, stream_map);

                if (rc != PB_OK)
                {
                    pb_wire_init_result(&result, rc);
                    break;
                }
            }

            rc = plat_hash_init(&hash_ctx, PB_HASH_SHA256);

            if (rc != PB_OK)
            {
                pb_wire_init_result(&result, rc);
                break;
            }

            if (verify_cmd->bpak)
            {
                LOG_DBG("Bpak header");
                rc = pb_storage_read(sdrv, map, buffer,
                                        blocks, block_offset);

                if (rc != PB_OK)
                {
                    pb_wire_init_result(&result, rc);
                    sdrv->map_request(sdrv, stream_map);
                    break;
                }

                rc = plat_hash_update(&hash_ctx, buffer,
                                        sizeof(struct bpak_header));

                if (rc != PB_OK)
                {
                    pb_wire_init_result(&result, rc);
                    break;
                }

                blocks_to_check -= blocks;
            }

            LOG_DBG("Reading %zu blocks", blocks_to_check);
            block_offset = 0;

            while (blocks_to_check)
            {
                blocks = blocks_to_check>(CONFIG_CMD_BUF_SIZE_KB/2)? \
                           (CONFIG_CMD_BUF_SIZE_KB/2):blocks_to_check;

                rc = pb_storage_read(sdrv, map, buffer,
                                        blocks, block_offset);

                if (rc != PB_OK)
                {
                    pb_wire_init_result(&result, rc);

                    if (sdrv->map_release)
                        sdrv->map_release(sdrv, stream_map);
                    break;
                }

                rc = plat_hash_update(&hash_ctx, buffer,
                                            (blocks*sdrv->block_size));

                if (rc != PB_OK)
                {
                    pb_wire_init_result(&result, rc);
                    break;
                }

                blocks_to_check -= blocks;
                block_offset += blocks;
            }

            if (rc != PB_OK)
                break;

            rc = plat_hash_finalize(&hash_ctx, NULL, 0);

            if (rc != PB_OK)
            {
                pb_wire_init_result(&result, rc);
                break;
            }


            if (memcmp(hash_ctx.buf, verify_cmd->sha256, 32) == 0)
                rc = PB_RESULT_OK;
            else
                rc = -PB_RESULT_PART_VERIFY_FAILED;

            pb_wire_init_result(&result, rc);
        }
        break;
        case PB_CMD_BOOT_PART:
        {
            struct pb_command_boot_part *boot_cmd = \
                (struct pb_command_boot_part *) cmd.request;
            rc = pb_boot_load_fs(boot_cmd->uuid);
            pb_wire_init_result(&result, rc);

            plat_transport_write(&result, sizeof(result));

            if (rc != PB_OK)
                break;

            pb_boot(boot_cmd->verbose);
            /* Should not return */
            return -PB_ERR;
        }
        break;
        case PB_CMD_DEVICE_IDENTIFIER_READ:
        {
            struct pb_result_device_identifier *ident = \
                (struct pb_result_device_identifier *) buffer;
            memset(ident->board_id, 0, sizeof(ident->board_id));
            memcpy(ident->board_id, board_name(), strlen(board_name()));
            memcpy(ident->device_uuid, device_uuid, 16);
            pb_wire_init_result2(&result, rc,
                                    ident, sizeof(*ident));
        }
        break;
        case PB_CMD_PART_ACTIVATE:
        {
            struct pb_command_activate_part *activate_cmd = \
                (struct pb_command_activate_part *) cmd.request;
            rc = pb_boot_activate(activate_cmd->uuid);
            pb_wire_init_result(&result, rc);
        }
        break;
        case PB_CMD_AUTHENTICATE:
        {
            struct pb_command_authenticate *auth_cmd = \
                (struct pb_command_authenticate *) cmd.request;

            LOG_DBG("Auth, method:%u sz:%i", auth_cmd->method, auth_cmd->size);
            pb_wire_init_result(&result, -PB_RESULT_NOT_SUPPORTED);

#ifdef CONFIG_AUTH_TOKEN
            if (auth_cmd->method == PB_AUTH_ASYM_TOKEN)
            {
                pb_wire_init_result(&result, PB_RESULT_OK);
                plat_transport_write(&result, sizeof(result));

                plat_transport_read(buffer[0], auth_cmd->size);

                rc = auth_token(device_uuid,
                        auth_cmd->key_id, buffer[0], auth_cmd->size);

                if (rc == PB_OK)
                    authenticated = true;
                else
                    authenticated = false;

                pb_wire_init_result(&result, rc);
            }
#endif


        }
        break;
        case PB_CMD_AUTH_SET_OTP_PASSWORD:
        {
#ifndef CONFIG_AUTH_PASSWORD
            pb_wire_init_result(&result, -PB_RESULT_NOT_SUPPORTED);
#endif
        }
        break;
        case PB_CMD_BOARD_COMMAND:
        {
            struct pb_command_board *board_cmd = \
                (struct pb_command_board *) cmd.request;

            struct pb_result_board board_result;

            LOG_DBG("Board command %x", board_cmd->command);
            memset(&board_result, 0, sizeof(board_result));

            uint8_t *bfr = buffer[0];

            pb_wire_init_result(&result, PB_RESULT_OK);
            plat_transport_write(&result, sizeof(result));

            if (board_cmd->request_size)
            {
                rc = plat_transport_read(bfr, board_cmd->request_size);

                if (rc != PB_OK)
                {
                    pb_wire_init_result(&result, rc);
                    break;
                }
            }

            uint8_t *bfr_response = buffer[1];

            size_t response_size = CONFIG_CMD_BUF_SIZE_KB*1024;
            void *plat_private = plat_get_private();

            rc = board_command(plat_private, board_cmd->command,
                                bfr, board_cmd->request_size,
                                bfr_response, &response_size);

            board_result.size = response_size;
            pb_wire_init_result2(&result, rc,
                            &board_result, sizeof(board_result));

            plat_transport_write(&result, sizeof(result));
            plat_transport_write(bfr_response, response_size);

        }
        break;
        case PB_CMD_BOARD_STATUS_READ:
        {
            struct pb_result_board_status status_result;

            uint8_t *bfr_response = buffer[0];

            size_t response_size = CONFIG_CMD_BUF_SIZE_KB*1024;
            void *plat_private = plat_get_private();
            rc = board_status(plat_private, bfr_response, &response_size);

            status_result.size = response_size;
            pb_wire_init_result2(&result, rc,
                            &status_result, sizeof(status_result));

            plat_transport_write(&result, sizeof(result));
            plat_transport_write(bfr_response, response_size);
        }
        break;
        case PB_CMD_SLC_SET_CONFIGURATION:
        {
            LOG_DBG("Set configuration");
            rc = plat_slc_set_configuration();
            pb_wire_init_result(&result, rc);
            plat_slc_read(&slc);
        }
        break;
        case PB_CMD_SLC_SET_CONFIGURATION_LOCK:
        {
            LOG_DBG("Set configuration lock");
            rc = plat_slc_set_configuration_lock();
            pb_wire_init_result(&result, rc);
            plat_slc_read(&slc);
        }
        break;
        case PB_CMD_SLC_SET_EOL:
        {
            LOG_DBG("Set EOL");
            rc = plat_slc_set_end_of_life();
            pb_wire_init_result(&result, rc);
            plat_slc_read(&slc);
        }
        break;
        case PB_CMD_SLC_REVOKE_KEY:
        {
            struct pb_command_revoke_key *revoke_cmd = \
                (struct pb_command_revoke_key *) cmd.request;

            LOG_DBG("Revoke key %x", revoke_cmd->key_id);

            rc = plat_slc_revoke_key(revoke_cmd->key_id);
            pb_wire_init_result(&result, rc);
        }
        break;
        case PB_CMD_SLC_READ:
        {
            LOG_DBG("SLC Read");
            struct pb_result_slc slc_status;
            rc = plat_slc_read((enum pb_slc *) &slc_status.slc);
            pb_wire_init_result2(&result, rc, &slc_status,
                                sizeof(slc_status));
        }
        break;
        default:
        {
            LOG_ERR("Got unknown command: %u", cmd.command);
            pb_wire_init_result(&result, -PB_RESULT_INVALID_COMMAND);
        }
    }

err_out:
    plat_transport_write(&result, sizeof(result));
    return rc;
}

void pb_command_run(void)
{
    int rc;

    LOG_DBG("Initializing command mode");

    plat_get_uuid((char *) device_uuid);

restart_command_mode:
    rc = plat_transport_init();

    if (rc != PB_OK)
    {
        LOG_ERR("Transport init err");
        plat_reset();
    }

    unsigned int ready_timeout = plat_get_us_tick();

    LOG_INFO("Waiting for transport to become ready...");

    while (!plat_transport_ready())
    {
        plat_transport_process();
        plat_wdog_kick();


         if ((plat_get_us_tick() - ready_timeout) > 
             (CONFIG_TRANSPORT_READY_TIMEOUT * 1000000L))
        {
            LOG_ERR("Timeout, rebooting");
            plat_reset();
        }
    }

    while (true)
    {
        rc = plat_transport_read(&cmd, sizeof(cmd));

        if (rc != PB_OK)
        {
            LOG_ERR("Read error %i", rc);
            goto restart_command_mode;
        }

        rc = pb_command_parse();

        if (rc != PB_OK)
        {
            LOG_ERR("Command error %i", rc);
        }
    }
}
