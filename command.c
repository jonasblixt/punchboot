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
#include <pb/uuid.h>
#include <pb/gpt.h>
#include <pb/boot.h>
#include <board/config.h>
#include <pb/config.h>
#include <plat/defs.h>
#include <bpak/bpak.h>
#include <bpak/keystore.h>
#include <pb-tools/wire.h>

#define RECOVERY_CMD_BUFFER_SZ  1024*64
#define RECOVERY_BULK_BUFFER_BLOCKS 16384
#define RECOVERY_BULK_BUFFER_SZ (RECOVERY_BULK_BUFFER_BLOCKS*512)
#define RECOVERY_MAX_PARAMS 128

static uint8_t __a4k __no_bss recovery_cmd_buffer[RECOVERY_CMD_BUFFER_SZ];
static uint8_t __a4k __no_bss recovery_bulk_buffer[2][RECOVERY_BULK_BUFFER_SZ];
static __no_bss __a4k uint8_t signature_data[1024];
static __no_bss __a4k struct bpak_header bp_header;
static __no_bss __a4k struct param params[RECOVERY_MAX_PARAMS];
static __no_bss __a4k uint8_t authentication_cookie[PB_RECOVERY_AUTH_COOKIE_SZ];
extern const struct partition_table pb_partition_table[];
static unsigned char hash_buffer[PB_HASH_BUF_SZ];
static bool recovery_authenticated = false;
extern char _code_start, _code_end, _data_region_start, _data_region_end,
            _zero_region_start, _zero_region_end, _stack_start, _stack_end;

extern const struct bpak_keystore keystore_pb;

static struct pb_storage *storage;

#ifdef __NOPE

static uint32_t recovery_authenticate(uint32_t key_id,
                                      uint8_t *sig, size_t size)
{
    uint32_t err = PB_ERR;
    char device_uuid[128];
    char device_uuid_raw[16];
    char auth_hash[32];
    struct bpak_key *k = NULL;

    plat_get_uuid(device_uuid_raw);

    memset(device_uuid, 0, sizeof(device_uuid));
    memset(auth_hash, 0, sizeof(auth_hash));

    uuid_to_string((uint8_t *) device_uuid_raw, device_uuid);

    for (uint32_t i = 0; i < keystore_pb.no_of_keys; i++)
    {
        if (keystore_pb.keys[i]->id == key_id)
        {
            k = keystore_pb.keys[i];
            break;
        }
    }

    if (!k)
    {
        LOG_ERR("Key not found");
        return PB_ERR;
    }

    LOG_DBG("Found key %x", k->id);

    uint32_t sign_kind = PB_SIGN_INVALID;
    uint32_t hash_kind = PB_HASH_INVALID;

    switch (k->kind)
    {
        case BPAK_KEY_PUB_PRIME256v1:
            sign_kind = PB_SIGN_PRIME256v1;
            hash_kind = PB_HASH_SHA256;
        break;
        case BPAK_KEY_PUB_SECP384r1:
            sign_kind = PB_SIGN_SECP384r1;
            hash_kind = PB_HASH_SHA384;
        break;
        case BPAK_KEY_PUB_SECP521r1:
            sign_kind = PB_SIGN_SECP521r1;
            hash_kind = PB_HASH_SHA512;
        break;
        case BPAK_KEY_PUB_RSA4096:
            sign_kind = PB_SIGN_RSA4096;
            hash_kind = PB_HASH_SHA256;
        break;
        default:
            LOG_ERR("Unkown key kind");
            return PB_ERR;
    }

    err = plat_hash_init(hash_kind);

    if (err != PB_OK)
        return PB_ERR;

    plat_hash_update(0, 0);
    err = plat_hash_finalize((uintptr_t) device_uuid, 36,
                             (uintptr_t) auth_hash, sizeof(auth_hash));

    if (err != PB_OK)
        return PB_ERR;

    err = plat_verify_signature(sig, sign_kind,
                                (uint8_t *) auth_hash, hash_kind,
                                k);

    if (err != PB_OK)
    {
        LOG_ERR("Authentication failed");
        return err;
    }

    LOG_INFO("Authentication successful");

    return PB_OK;
}

static uint32_t recovery_flash_bootloader(uint8_t *bfr,
                                          uint32_t blocks_to_write,
                                          uint32_t file_size,
                                          char *hash_data)
{
    char hash[32];
    uint32_t rc = PB_OK;

    if (plat_switch_part(PLAT_EMMC_PART_BOOT0) != PB_OK)
    {
        LOG_ERR("Could not switch partition");
        return PB_ERR;
    }

    plat_switch_part(PLAT_EMMC_PART_BOOT0);
    plat_write_block(PB_BOOTPART_OFFSET, (uintptr_t) bfr, blocks_to_write);

    LOG_INFO("Bootloader programmed, verifying...");
    /* Read back and check hash */
    plat_read_block(PB_BOOTPART_OFFSET, (uintptr_t) recovery_bulk_buffer[1],
                     blocks_to_write);

    plat_hash_init(PB_HASH_SHA256);
    plat_hash_update((uintptr_t) recovery_bulk_buffer[1],
                        file_size);
    plat_hash_finalize((uintptr_t) NULL, 0, (uintptr_t) hash, sizeof(hash));

    for (uint32_t i = 0; i < 32; i++)
    {
        if (hash[i] != hash_data[i])
        {
            rc = PB_ERR;
            goto err_out;
        }
    }

    plat_switch_part(PLAT_EMMC_PART_BOOT1);
    plat_write_block(PB_BOOTPART_OFFSET, (uintptr_t) bfr, blocks_to_write);

    /* Read back and check hash */
    plat_read_block(PB_BOOTPART_OFFSET, (uintptr_t) recovery_bulk_buffer[1]
                    , blocks_to_write);

    plat_hash_init(PB_HASH_SHA256);
    plat_hash_update((uintptr_t) recovery_bulk_buffer[1],
                        file_size);
    plat_hash_finalize((uintptr_t) NULL, 0, (uintptr_t) hash, sizeof(hash));

    for (uint32_t i = 0; i < 32; i++)
    {
        if (hash[i] != hash_data[i])
        {
            rc = PB_ERR;
            goto err_out;
        }
    }

err_out:
    if (rc != PB_OK)
    {
        LOG_ERR("Programming bootloader failed");
    }

    plat_switch_part(PLAT_EMMC_PART_USER);

    return rc;
}

static uint32_t recovery_flash_part(uint8_t part_no,
                                    uint32_t lba_offset,
                                    uint32_t no_of_blocks,
                                    uint8_t *bfr)
{
    uint32_t part_lba_offset = 0;

    part_lba_offset = gpt_get_part_first_lba(part_no);

    if (!part_lba_offset)
    {
        LOG_ERR("Unknown partition");
        return PB_ERR;
    }

    if ( (lba_offset + no_of_blocks) > gpt_get_part_last_lba(part_no))
    {
        LOG_ERR("Trying to write outside of partition");
        return PB_ERR;
    }

    plat_flush_block();

    return plat_write_block_async(part_lba_offset + lba_offset,
                                (uintptr_t) bfr, no_of_blocks);
}

static uint32_t recovery_send_response(struct usb_device *dev,
                                       uint8_t *bfr, uint32_t sz)
{
    uint32_t err = PB_OK;

    memcpy(recovery_cmd_buffer, (uint8_t *) &sz, 4);

    if (sz >= RECOVERY_CMD_BUFFER_SZ)
        return PB_ERR;

    err = plat_usb_transfer(dev, USB_EP3_IN, recovery_cmd_buffer, 4);

    if (err != PB_OK)
        return err;

    plat_usb_wait_for_ep_completion(dev, USB_EP3_IN);

    memcpy(recovery_cmd_buffer, bfr, sz);

    err = plat_usb_transfer(dev, USB_EP3_IN, recovery_cmd_buffer, sz);

    if (err != PB_OK)
        return err;

    plat_usb_wait_for_ep_completion(dev, USB_EP3_IN);

    return PB_OK;
}

static uint32_t recovery_read_data(struct usb_device *dev,
                                   uint8_t *bfr, uint32_t sz)
{
    uint32_t err = PB_OK;

    err = plat_usb_transfer(dev, USB_EP2_OUT, recovery_cmd_buffer, sz);

    if (err != PB_OK)
        return err;

    plat_usb_wait_for_ep_completion(dev, USB_EP2_OUT);

    memcpy(bfr, recovery_cmd_buffer, sz);

    return err;
}


static int recovery_verify_part(struct usb_device *dev, struct pb_cmd_header *cmd)
{
    int err = PB_OK;
    char hash_data_in[32];
    char hash_data_gen[32];

    uint32_t part_lba_offset = 0;
    uint32_t part_lba_end = 0;

    part_lba_offset = gpt_get_part_first_lba(cmd->arg0) +
                        cmd->arg1;

    part_lba_end = gpt_get_part_last_lba(cmd->arg0);

    recovery_read_data(dev,(uint8_t *) hash_data_in, 32);

    LOG_INFO("Verifying part %u with offset %x, size %i bytes",
                cmd->arg0, cmd->arg1, cmd->arg2);

    plat_hash_init(PB_HASH_SHA256);
    uint32_t no_of_blocks = cmd->arg2 / 512;
    uint32_t chunk = 0;

    memset(&bp_header, 0, sizeof(bp_header));
    plat_read_block(part_lba_end - (sizeof(bp_header) / 512) + 1,
                    (uintptr_t) &bp_header, sizeof(bp_header)/512);

    err = bpak_valid_header(&bp_header);

    if (err == BPAK_OK)
    {
        LOG_INFO("Part contains BPAK archive");
        plat_hash_update((uintptr_t) &bp_header, sizeof(bp_header));
        no_of_blocks -= sizeof(bp_header)/512;
    }

    while(no_of_blocks)
    {
        chunk = (no_of_blocks > RECOVERY_BULK_BUFFER_BLOCKS)?
                        RECOVERY_BULK_BUFFER_BLOCKS:no_of_blocks;

        plat_read_block(part_lba_offset,
                        (uintptr_t) recovery_bulk_buffer[0], chunk);

        plat_hash_update((uintptr_t) recovery_bulk_buffer[0],
                            chunk*512);
        part_lba_offset += chunk;
        no_of_blocks -= chunk;
    };

    plat_hash_finalize((uintptr_t) NULL, 0, (uintptr_t) hash_data_gen,
                                        sizeof(hash_data_gen));

    err = PB_OK;
    for (uint32_t i = 0; i < 32; i++)
    {
        if (hash_data_in[i] != hash_data_gen[i])
        {
            err = PB_ERR;
            break;
        }
    }

    return err;
}

static int recovery_ram_boot(struct usb_device *dev, struct pb_cmd_header *cmd)
{
    int err;
    uint32_t hash_kind = 0;
    uint32_t sign_kind = 0;

    recovery_read_data(dev, (uint8_t *) &bp_header,
                        sizeof(struct bpak_header));

    LOG_DBG("OK");

    err = bpak_valid_header(&bp_header);

    if (err != BPAK_OK)
    {
        LOG_ERR("Invalid BPAK header");
        return err;
    }

    err = pb_image_check_header(&bp_header);

    if (err != PB_OK)
        return err;

    LOG_DBG("Load key info");

    uint32_t *key_id = NULL;
    uint32_t *keystore_id = NULL;

                          /* bpak-key-id */
    err = bpak_get_meta(&bp_header, 0x7da19399, (void **) &key_id);

    if (!key_id || (err != BPAK_OK))
    {
        LOG_ERR("Missing bpak-key-id meta\n");
        return PB_ERR;
    }

                        /* bpak-key-store */
    err = bpak_get_meta(&bp_header, 0x106c13a7, (void **) &keystore_id);

    if (!keystore_id || (err != BPAK_OK))
    {
        LOG_ERR("Missing bpak-key-store meta\n");
        return PB_ERR;
    }

    LOG_DBG("Key-store: %x", *keystore_id);
    LOG_DBG("Key-ID: %x", *key_id);

    if (*keystore_id != keystore_pb.id)
    {
        LOG_ERR("Invalid key-store");
        return PB_ERR;
    }

    struct bpak_key *k = NULL;

    for (uint32_t i = 0; i < keystore_pb.no_of_keys; i++)
    {
        if (keystore_pb.keys[i]->id == *key_id)
        {
            k = keystore_pb.keys[i];
            break;
        }
    }

    if (!k)
    {
        LOG_ERR("Key not found");
        return PB_ERR;
    }

    switch (bp_header.hash_kind)
    {
        case BPAK_HASH_SHA256:
            hash_kind = PB_HASH_SHA256;
        break;
        case BPAK_HASH_SHA384:
            hash_kind = PB_HASH_SHA384;
        break;
        case BPAK_HASH_SHA512:
            hash_kind = PB_HASH_SHA512;
        break;
        default:
            err = PB_ERR;
    }

    if (err != PB_OK)
        return err;

    switch (bp_header.signature_kind)
    {
        case BPAK_SIGN_PRIME256v1:
            sign_kind = PB_SIGN_PRIME256v1;
        break;
        case BPAK_SIGN_SECP384r1:
            sign_kind = PB_SIGN_SECP384r1;
        break;
        case BPAK_SIGN_SECP521r1:
            sign_kind = PB_SIGN_SECP521r1;
        break;
        case BPAK_SIGN_RSA4096:
            sign_kind = PB_SIGN_RSA4096;
        break;
        default:
            LOG_ERR("Unknown signature format");
            err = PB_ERR;
    }

    if (err != PB_OK)
        return err;

    plat_hash_init(hash_kind);
    recovery_send_result_code(dev, err);

    bool found_signature = false;

    /* Copy and zero out the signature metadata before hasing header */
    bpak_foreach_meta(&bp_header, m)
    {
                    /* bpak-signature */
        if (m->id == 0xe5679b94)
        {
            LOG_DBG("sig zero");

            if (m->size > sizeof(signature_data))
            {
                LOG_ERR("Signature metadata is too large\n");
                return PB_ERR;
            }

            uint8_t *ptr = &(&bp_header)->metadata[m->offset];
            memcpy(signature_data, ptr, m->size);
            memset(ptr, 0, m->size);
            memset(m, 0, sizeof(*m));
            found_signature = true;
            break;
        }
    }

    if (!found_signature)
    {
        LOG_ERR("Could not find a valid signature");
        return PB_ERR;
    }

    plat_hash_update((uintptr_t)&bp_header, sizeof(struct bpak_header));

    uint64_t *load_addr = NULL;
    uint64_t chunk = 0;
    uint64_t remainder = 0;
    uint64_t offset = 0;

    bpak_foreach_part(&bp_header, p)
    {
        if (!p->id)
            break;

        load_addr = NULL;
                                              /* pb-load-addr */
        err = bpak_get_meta_with_ref(&bp_header, 0xd1e64a4b,
                                        p->id, (void **) &load_addr);

        if (err != BPAK_OK)
        {
            LOG_ERR("Could not read pb-entry for part %x", p->id);
            break;
        }
        LOG_DBG("Loading part %x --> %p, %llu bytes", p->id,
                        (void *)(uintptr_t) (*load_addr),
                        p->size);


        remainder = p->size + p->pad_bytes;
        chunk = 0;
        offset = 0;

        while (remainder)
        {
            if (remainder > 1024*1024)
                chunk = 1024*1024;
            else
                chunk = remainder;

            uintptr_t addr = ((*load_addr) + offset);

            err = plat_usb_transfer(dev, USB_EP1_OUT, (uint8_t *) addr, chunk);

            if (err != PB_OK)
            {
                LOG_ERR("Xfer error");
                break;
            }

            plat_usb_wait_for_ep_completion(dev, USB_EP1_OUT);

            err = plat_hash_update(addr, chunk);

            if (err != PB_OK)
            {
                LOG_ERR("Hash error");
                break;
            }

            offset += chunk;
            remainder -= chunk;
        }
    }

    if (err != BPAK_OK)
    {
        LOG_ERR("Loading failed");
        return err;
    }

    LOG_INFO("Loading done");

    err = PB_OK;

    char hash[64];
    plat_hash_finalize((uintptr_t) NULL, 0, (uintptr_t) hash,
                            sizeof(hash));

    err = plat_verify_signature(signature_data, sign_kind,
                                (uint8_t *) hash, hash_kind, k);
    if (err == PB_OK)
    {
        LOG_INFO("Booting image... %u", cmd->arg0);
        recovery_send_result_code(dev, err);
        pb_boot(&bp_header, cmd->arg0, cmd->arg1);
    }
    else
    {
        LOG_ERR("Image verification failed");
        err = PB_ERR;
    }

    return err;
}
#endif


static void send_result(struct usb_device *dev, struct pb_result *result)
{
    /* Send result code */
    memcpy(recovery_cmd_buffer, result, sizeof(*result));
    plat_usb_transfer(dev, USB_EP3_IN, recovery_cmd_buffer, sizeof(*result));
    plat_usb_wait_for_ep_completion(dev, USB_EP3_IN);
}

static void command_parse(struct usb_device *dev, struct pb_command *cmd)
{
    struct pb_result result;
    uint32_t err = PB_OK;
    uint32_t security_state = -1;

    if (!pb_wire_valid_command(cmd))
    {
        LOG_ERR("Invalid command: %i\n", cmd->command);
        pb_wire_init_result(&result, -PB_RESULT_INVALID_COMMAND);
        goto err_out;
    }

    LOG_DBG("Parse command: %i", cmd->command);

    switch (cmd->command)
    {
        case PB_CMD_BOOTLOADER_VERSION_READ:
        {
            char version_string[30];

            LOG_INFO("Get version");
            snprintf(version_string, sizeof(version_string), "PB %s", PB_VERSION);

            pb_wire_init_result2(&result, PB_RESULT_OK, version_string,
                                                        strlen(version_string));
        }
        break;
        case PB_CMD_DEVICE_RESET:
        {
            LOG_INFO("Board reset");
            pb_wire_init_result(&result, PB_RESULT_OK);
            send_result(dev, &result);
            plat_reset();

            while (1)
            {
                __asm__ volatile("wfi");
            }
        }
        break;
        case PB_CMD_PART_TBL_READ:
        {
            LOG_DBG("TBL read %i", storage->no_of_drivers);
            struct pb_result_part_table_read tbl_read_result;
            struct pb_result_part_table_entry result_tbl[32];
            int entries = 0;

            for (int i = 0; i < storage->no_of_drivers; i++)
            {
                struct pb_storage_driver *drv = storage->drivers[i];

                pb_storage_foreach_part(drv->map->map_data, part)
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
                    result_tbl[entries].block_size = drv->block_size;
                    result_tbl[entries].flags = (part->flags & 0xFF);

                    entries++;
                }
            }

            tbl_read_result.no_of_entries = entries;
            LOG_DBG("%i entries", entries);
            pb_wire_init_result2(&result, PB_RESULT_OK, &tbl_read_result,
                                                     sizeof(tbl_read_result));

            send_result(dev, &result);

            if (entries)
            {
                size_t bytes = sizeof(struct pb_result_part_table_entry)*(entries);
                LOG_DBG("Bytes %li", bytes);
                memcpy(recovery_cmd_buffer, result_tbl, bytes);
                plat_usb_transfer(dev, USB_EP3_IN, recovery_cmd_buffer, bytes);
                plat_usb_wait_for_ep_completion(dev, USB_EP3_IN);
            }

            pb_wire_init_result(&result, PB_RESULT_OK);
        }
        break;
#ifdef __NOPE
        case PB_CMD_PREP_BULK_BUFFER:
        {
            struct pb_cmd_prep_buffer cmd_prep;

            recovery_read_data(dev, (uint8_t *) &cmd_prep,
                                sizeof(struct pb_cmd_prep_buffer));

            LOG_INFO("Preparing buffer %u [%u]",
                            cmd_prep.buffer_id, cmd_prep.no_of_blocks);

            if ( (cmd_prep.no_of_blocks*512) > RECOVERY_BULK_BUFFER_SZ)
            {
                err = PB_ERR;
                break;
            }
            if (cmd_prep.buffer_id > 1)
            {
                err = PB_ERR;
                break;
            }

            uint8_t *bfr = recovery_bulk_buffer[cmd_prep.buffer_id];
            err = plat_usb_transfer(dev, USB_EP1_OUT, bfr,
                                                cmd_prep.no_of_blocks*512);
        }
        break;
        case PB_CMD_FLASH_BOOTLOADER:
        {
            char hash_data_in[32];
            LOG_INFO("Flash BL %u, %u", cmd->arg0, cmd->arg1);

            recovery_read_data(dev,(uint8_t *) hash_data_in, 64);

            err = recovery_flash_bootloader(recovery_bulk_buffer[0],
                        cmd->arg0, cmd->arg1, hash_data_in);
        }
        break;
        case PB_CMD_GET_GPT_TBL:
        {
            err = PB_OK;
            recovery_send_result_code(dev, err);

            err = recovery_send_response(dev, (uint8_t*) gpt_get_table(),
                                        sizeof (struct gpt_primary_tbl));
        }
        break;
        case PB_CMD_BOOT_ACTIVATE:
        {
            struct gpt_part_hdr *part_sys_a, *part_sys_b;

            gpt_get_part_by_uuid(PB_PARTUUID_SYSTEM_A, &part_sys_a);
            gpt_get_part_by_uuid(PB_PARTUUID_SYSTEM_B, &part_sys_b);

            if (cmd->arg0 == SYSTEM_NONE)
            {
                config_system_enable(0);
            }
            else if (cmd->arg0 == SYSTEM_A)
            {
                LOG_INFO("Activating System A");
                config_system_enable(SYSTEM_A);
                config_system_set_verified(SYSTEM_A, true);
            }
            else if (cmd->arg0 == SYSTEM_B)
            {
                LOG_INFO("Activating System B");
                config_system_set_verified(SYSTEM_B, true);
                config_system_enable(SYSTEM_B);
            }
            else
            {
                err = PB_ERR;
                goto recovery_error_out;
            }

            err = config_commit();
        }
        break;
        case PB_CMD_WRITE_PART:
        {
            struct pb_cmd_write_part wr_part;

            recovery_read_data(dev, (uint8_t *) &wr_part,
                                    sizeof(struct pb_cmd_write_part));

            LOG_INFO("Writing %u blks to part %u" \
                        " with offset %x using bfr %u",
                        wr_part.no_of_blocks, wr_part.part_no,
                        wr_part.lba_offset, wr_part.buffer_id);

            err = recovery_flash_part(wr_part.part_no,
                                wr_part.lba_offset,
                                wr_part.no_of_blocks,
                                recovery_bulk_buffer[wr_part.buffer_id]);
            LOG_INFO("Write done");
        }
        break;
        case PB_CMD_WRITE_PART_FINAL:
        {
            LOG_DBG("Part final, flush");
            err = plat_flush_block();
        }
        break;
        case PB_CMD_VERIFY_PART:
        {
            err = recovery_verify_part(dev, cmd);
        }
        break;
        case PB_CMD_BOOT_PART:
        {
            struct gpt_part_hdr *boot_part_a, *boot_part_b;


            err = gpt_get_part_by_uuid(PB_PARTUUID_SYSTEM_A, &boot_part_a);

            if (err != PB_OK)
            {
                LOG_ERR("System A not found");
                break;
            }

            err = gpt_get_part_by_uuid(PB_PARTUUID_SYSTEM_B, &boot_part_b);

            if (err != PB_OK)
            {
                LOG_ERR("System B not found");
                break;
            }

            if (cmd->arg0 == SYSTEM_A)
            {
                LOG_INFO("Loading System A");
                err = pb_image_load_from_fs(boot_part_a->first_lba,
                                            boot_part_a->last_lba,
                                            &bp_header,
                                            (const char*) hash_buffer);
            }
            else if (cmd->arg0 == SYSTEM_B)
            {
                LOG_INFO("Loading System B");
                err = pb_image_load_from_fs(boot_part_b->first_lba,
                                            boot_part_b->last_lba,
                                            &bp_header,
                                            (const char *)hash_buffer);
            }
            else
            {
                LOG_ERR("Invalid boot partition");
                err = PB_ERR;
            }

            if (err != PB_OK)
            {
                LOG_ERR("Could not load image");
                break;
            }

            LOG_INFO("Booting image...");
            recovery_send_result_code(dev, err);
            pb_boot(&bp_header, cmd->arg0, cmd->arg1);
        }

        break;
        case PB_CMD_BOOT_RAM:
        {
            err = recovery_ram_boot(dev, cmd);
        }
        break;
        case PB_CMD_WRITE_DFLT_GPT:
        {
            LOG_INFO("Installing default GPT table");

            err = gpt_init_tbl(1, plat_get_lastlba());

            if (err != PB_OK)
                break;

            uint32_t part_count = 0;
            for (const struct partition_table *p = pb_partition_table;
                    (p->no_of_blocks != 0); p++)
            {
                unsigned char guid[16];
                uuid_to_guid((uint8_t *)p->uuid, guid);
                err = gpt_add_part(part_count++, p->no_of_blocks,
                                                 (const char *)guid,
                                                 p->name);

                if (err != PB_OK)
                    break;
            }

            if (err != PB_OK)
                break;

            err = gpt_write_tbl();

            if (err != PB_OK)
                break;

            err = config_init();
        }
        break;
        case PB_CMD_SETUP:
        {
            if (cmd->arg0 > RECOVERY_MAX_PARAMS)
            {
                err = PB_ERR;
                LOG_ERR("To many parameters");
                break;
            }

            LOG_INFO("Performing device setup, %u", cmd->arg0);

            params[0].kind = PB_PARAM_END;

            if (cmd->arg0 > 0)
            {
                if (cmd->arg0 > RECOVERY_MAX_PARAMS)
                {
                    LOG_ERR("Too many parameters");
                    err = PB_ERR;
                    break;
                }
                recovery_read_data(dev, (uint8_t *) params,
                        sizeof(struct param) * cmd->arg0);
            }

            err = plat_setup_device(params);
        }
        break;
        case PB_CMD_SETUP_LOCK:
        {
            LOG_INFO("Locking device setup");
            err = plat_setup_lock();

            if (err == PB_OK)
                recovery_authenticated = false;
        }
        break;
        case PB_CMD_GET_PARAMS:
        {
            uint32_t param_count = 0;
            struct param *p = params;

            plat_get_params(&p);
            board_get_params(&p);
            param_terminate(p++);
            p = params;


            while (p->kind != PB_PARAM_END)
            {
                p++;
                param_count++;
            }

            param_count++;

            recovery_send_response(dev, (uint8_t*) params,
                            (sizeof(struct param) * param_count));
            err = PB_OK;
        }
        break;
        case PB_CMD_AUTHENTICATE:
        {
            if (cmd->size > PB_RECOVERY_AUTH_COOKIE_SZ)
            {
                LOG_ERR("Authentication cookie is to large");
                err = PB_ERR;
                break;
            }

            recovery_read_data(dev, authentication_cookie, cmd->size);

            LOG_DBG("Got auth cmd, key_id = %x", cmd->arg0);

            err = recovery_authenticate(cmd->arg0,
                                        authentication_cookie,
                                        cmd->size);

            if (err == PB_OK)
                recovery_authenticated = true;
        }
        break;
        case PB_CMD_BOARD_COMMAND:
        {
            err = board_recovery_command(cmd->arg0,
                                         cmd->arg1,
                                         cmd->arg2,
                                         cmd->arg3);
        }
        break;
#endif
        default:
            LOG_ERR("Got unknown command: %u", cmd->command);
    }

err_out:
    send_result(dev, &result);
}

int command_initialize(struct pb_storage *_storage)
{
    recovery_authenticated = false;
    storage = _storage;

    if (usb_init() != PB_OK)
        return -PB_ERR;

    usb_set_on_command_handler(command_parse);

    return PB_OK;
}

