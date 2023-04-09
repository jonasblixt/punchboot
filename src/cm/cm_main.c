/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/board.h>
#include <pb/cm.h>
#include <pb/delay.h>
#include <bpak/bpak.h>
#include <bpak/keystore.h>
#include <pb-tools/wire.h>
#include <uuid.h>
#include <pb/crypto.h>
#include <boot/boot.h>
#include <drivers/block/bio.h>
#include <pb/device_uuid.h>

/**
 * Alignment notes:
 *
 * At least some USB controllers requires 4k buffer alignment (for example imx_ehci)
 * Currently we just ensure that memory that is read/written to/from has 4k alignment.
 *
 * We should probably allocate 'CONFIG_CM_BUF_SIZE_KB' + worst case alignment and
 * offset the buffer pointers as needed by hal
 *
 */

extern struct bpak_keystore keystore_pb;
static struct pb_command cmd __section(".no_init") __aligned(4096);
static struct pb_result result __section(".no_init") __aligned(4096);
static bool authenticated = false;
static uint8_t buffer[2][CONFIG_CM_BUF_SIZE_KB*1024] __section(".no_init") __aligned(4096);
static enum pb_slc slc;
static uint8_t hash[CRYPTO_MD_MAX_SZ];
static uuid_t device_uu;
static bio_dev_t block_dev;
static const struct cm_config *cfg;

#ifdef CONFIG_CM_AUTH_TOKEN
static int auth_token(uint32_t key_id, uint8_t *sig, size_t size)
{
    int rc = -PB_ERR;
    struct bpak_key *k = NULL;
    bool verified = false;
    bool active = false;
    char device_uu_str[37];

    rc = plat_slc_key_active(key_id, &active);

    if (rc != PB_OK)
        return rc;

    if (!active) {
        LOG_ERR("Invalid or revoked key (%x)", key_id);
        return -PB_ERR_KEY_REVOKED;
    }

    LOG_DBG("uuid: %p %p", device_uu, device_uu_str);
    uuid_unparse(device_uu, device_uu_str);

    for (int i = 0; i < keystore_pb.no_of_keys; i++) {
        if (keystore_pb.keys[i]->id == key_id) {
            k = keystore_pb.keys[i];
            break;
        }
    }

    if (!k) {
        LOG_ERR("Key not found");
        return -PB_ERR_NOT_FOUND;
    }

    LOG_DBG("Found key %x", k->id);

    hash_t hash_kind = 0;
    dsa_t dsa_kind = 0;

    switch (k->kind) {
        case BPAK_KEY_PUB_PRIME256v1:
            hash_kind = HASH_SHA256;
            dsa_kind = DSA_EC_SECP256r1;
        break;
        case BPAK_KEY_PUB_SECP384r1:
            hash_kind = HASH_SHA384;
            dsa_kind = DSA_EC_SECP384r1;
        break;
        case BPAK_KEY_PUB_SECP521r1:
            hash_kind = HASH_SHA512;
            dsa_kind = DSA_EC_SECP521r1;
        break;
        default:
            return -PB_ERR_NOT_SUPPORTED;
    }

    LOG_DBG("Hash init (%s)", device_uu_str);
    rc = hash_init(hash_kind);

    if (rc != PB_OK)
        return rc;

    rc = hash_update((uintptr_t) device_uu_str, 36);

    if (rc != PB_OK)
        return rc;

    LOG_DBG("Hash final");
    rc = hash_final(hash, sizeof(hash));

    if (rc != PB_OK)
        return rc;

    rc = dsa_verify(dsa_kind,
                    sig, size,
                    (uint8_t *) k->data, k->size,
                    hash_kind, hash, sizeof(hash),
                    &verified);

    if (rc == 0 && verified) {
        LOG_INFO("Authentication successful");
        return PB_OK;
    } else {
        LOG_ERR("Authentication failed (%i) %i", rc, verified);
        return -PB_ERR_AUTHENTICATION_FAILED;
    }

    return -PB_ERR_AUTHENTICATION_FAILED;
}
#endif

static int error_to_wire(int error_code)
{
    switch (error_code) {
        case PB_OK:
            return PB_RESULT_OK;
        case -PB_ERR:
            return -PB_RESULT_ERROR;
        case -PB_ERR_TIMEOUT:
            return -PB_RESULT_TIMEOUT;
        case -PB_ERR_KEY_REVOKED:
            return -PB_RESULT_KEY_REVOKED;
        case -PB_ERR_SIGNATURE:
            return -PB_RESULT_SIGNATURE_ERROR;
        case -PB_ERR_CHECKSUM:
            return -PB_RESULT_ERROR;
        case -PB_ERR_MEM:
            return -PB_RESULT_MEM_ERROR;
        case -PB_ERR_IO:
            return -PB_RESULT_IO_ERROR;
        case -PB_ERR_NOT_FOUND:
            return -PB_RESULT_NOT_FOUND;
        case -PB_ERR_NOT_AUTHENTICATED:
            return -PB_RESULT_NOT_AUTHENTICATED;
        case -PB_ERR_AUTHENTICATION_FAILED:
            return -PB_RESULT_AUTHENTICATION_FAILED;
        case -PB_ERR_INVALID_ARGUMENT:
            return -PB_RESULT_INVALID_ARGUMENT;
        case -PB_ERR_INVALID_COMMAND:
            return -PB_RESULT_INVALID_COMMAND;
        case -PB_ERR_PART_NOT_FOUND:
            return -PB_RESULT_NOT_FOUND;
        case -PB_ERR_PART_NOT_BOOTABLE:
            return -PB_RESULT_PART_NOT_BOOTABLE;
        default:
            return -PB_RESULT_ERROR;
    }
}

static int cmd_board(void)
{
    int rc;

    struct pb_command_board *board_cmd = \
        (struct pb_command_board *) cmd.request;

    struct pb_result_board board_result;

    LOG_DBG("Board command %x", board_cmd->command);
    memset(&board_result, 0, sizeof(board_result));

    uint8_t *bfr = buffer[0];

    pb_wire_init_result(&result, PB_RESULT_OK);
    cfg->tops.write((uintptr_t) &result, sizeof(result));

    if (board_cmd->request_size) {
        LOG_DBG("Reading request data");
        rc = cfg->tops.read((uintptr_t) bfr, 1024);

        if (rc != PB_OK) {
            pb_wire_init_result(&result, error_to_wire(rc));
            return rc;
        }
    }

    uint8_t *bfr_response = buffer[1];

    size_t response_size = CONFIG_CM_BUF_SIZE_KB*1024;

    rc = cfg->command(board_cmd->command,
                        bfr, board_cmd->request_size,
                        bfr_response, &response_size);

    board_result.size = response_size;
    pb_wire_init_result2(&result, error_to_wire(rc),
                    &board_result, sizeof(board_result));

    LOG_DBG("Response: %zu bytes", response_size);

    if (response_size) {
        LOG_DBG("Sending response");
        cfg->tops.write((uintptr_t) &result, sizeof(result));
        cfg->tops.write((uintptr_t) bfr_response, response_size);
    }

    return rc;
}

static int cmd_bpak_read(void)
{
    int rc;
    struct pb_command_read_bpak *read_cmd = \
        (struct pb_command_read_bpak *) cmd.request;

    LOG_DBG("Read bpak");

    bio_dev_t dev = bio_get_part_by_uu(read_cmd->uuid);

    if (dev < 0) {
        LOG_ERR("Could not find partition");
        pb_wire_init_result(&result, error_to_wire(dev));
        return dev;
    }

    int header_lba = (bio_size(dev) - sizeof(struct bpak_header)) / bio_block_size(dev);

    LOG_DBG("Reading bpak header at lba %i", header_lba);

    rc = bio_read(dev, header_lba, sizeof(struct bpak_header), (uintptr_t) &buffer);

    if (rc != PB_OK) {
        pb_wire_init_result(&result, error_to_wire(rc));
        return rc;
    }

    rc = bpak_valid_header((struct bpak_header *) buffer);

    if (rc != BPAK_OK) {
        LOG_ERR("Invalid bpak header");
        pb_wire_init_result(&result, -PB_RESULT_NOT_FOUND);
        return rc;
    }

    pb_wire_init_result(&result, error_to_wire(rc));
    cfg->tops.write((uintptr_t) &result, sizeof(result));

    rc = cfg->tops.write((uintptr_t) buffer, sizeof(struct bpak_header));

    pb_wire_init_result(&result, error_to_wire(rc));
    return rc;
}

static int cmd_auth(void)
{
    int rc = -PB_ERR_NOT_IMPLEMENTED;

    struct pb_command_authenticate *auth_cmd = \
        (struct pb_command_authenticate *) cmd.request;

    pb_wire_init_result(&result, -PB_RESULT_NOT_SUPPORTED);

#ifdef CONFIG_CM_AUTH_TOKEN
    if (auth_cmd->method == PB_AUTH_ASYM_TOKEN) {
        pb_wire_init_result(&result, PB_RESULT_OK);
        cfg->tops.write((uintptr_t) &result, sizeof(result));

        cfg->tops.read((uintptr_t) buffer[0], 1024);

        rc = auth_token(auth_cmd->key_id, buffer[0], auth_cmd->size);

        if (rc == PB_OK)
            authenticated = true;
        else
            authenticated = false;

        pb_wire_init_result(&result, error_to_wire(rc));
    }
#endif

#if CONFIG_CM_AUTH_PASSWORD
    if (auth_cmd->method == PB_AUTH_PASSWORD && cfg->password_auth) {
        pb_wire_init_result(&result, PB_RESULT_OK);
        cfg->tops.write((uintptr_t) &result, sizeof(result));
        cfg->tops.read((uintptr_t) buffer[0], 1024);
        rc = cfg->password_auth((char *) buffer[0], auth_cmd->size);

        if (rc == PB_OK)
            authenticated = true;
        else
            authenticated = false;

        pb_wire_init_result(&result, error_to_wire(rc));
    }
#endif

    return rc;
}

static int cmd_stream_read(void)
{
    int rc = -PB_ERR;
    struct pb_command_stream_read_buffer *stream_read = \
        (struct pb_command_stream_read_buffer *) cmd.request;

    LOG_DBG("Stream read %u, %llu, %i", stream_read->buffer_id,
                                        stream_read->offset,
                                        stream_read->size);

    if (!(bio_get_flags(block_dev) & BIO_FLAG_READABLE)) {
        LOG_ERR("Partition may not be read");
        pb_wire_init_result(&result, -PB_RESULT_IO_ERROR);
        return -PB_ERR_IO;
    }

    size_t start_lba = (stream_read->offset / bio_block_size(block_dev));

    LOG_DBG("Reading %u bytes at lba offset %zu", stream_read->size,
                                                   start_lba);

    uintptr_t bfr = ((uintptr_t) buffer) +
              ((CONFIG_CM_BUF_SIZE_KB*1024)*stream_read->buffer_id);

    rc = bio_read(block_dev, start_lba, stream_read->size, bfr);
    pb_wire_init_result(&result, error_to_wire(rc));
    LOG_DBG("Result = %i", rc);

    cfg->tops.write((uintptr_t) &result, sizeof(result));

    if (rc == PB_OK) {
        rc = cfg->tops.write((uintptr_t) bfr, stream_read->size);
        pb_wire_init_result(&result, error_to_wire(rc));
    }
    LOG_DBG("Data sent");
    return rc;
}

static int cmd_stream_write(void)
{
    int rc;
    struct pb_command_stream_write_buffer *stream_write = \
        (struct pb_command_stream_write_buffer *) cmd.request;

    LOG_DBG("Stream write %u, %llu, %i", stream_write->buffer_id,
                                        stream_write->offset,
                                        stream_write->size);

    if (!(bio_get_flags(block_dev) & BIO_FLAG_WRITABLE)) {
        LOG_ERR("Partition may not be written");
        rc = -PB_ERR_IO;
        pb_wire_init_result(&result, error_to_wire(rc));
        return rc;
    }

    size_t start_lba = (stream_write->offset / bio_block_size(block_dev));

    LOG_DBG("Writing %u bytes to lba offset %zu", stream_write->size, start_lba);

    uintptr_t bfr = ((uintptr_t) buffer) +
              ((CONFIG_CM_BUF_SIZE_KB*1024)*stream_write->buffer_id);

    rc = bio_write(block_dev, start_lba, stream_write->size, bfr);

    pb_wire_init_result(&result, error_to_wire(rc));
    return rc;
}

static int cmd_part_verify(void)
{
    int rc;
    int buffer_id = 0;
    size_t chunk_len;
    struct pb_command_verify_part *verify_cmd = \
        (struct pb_command_verify_part *) cmd.request;
    size_t bytes_to_verify = verify_cmd->size;
    int lba_offset = 0;

    LOG_DBG("Verify part");

    block_dev = bio_get_part_by_uu(verify_cmd->uuid);

    if (block_dev < 0) {
        LOG_ERR("Could not find partition");
        pb_wire_init_result(&result, error_to_wire(block_dev));
        return block_dev;
    }

    rc = hash_init(HASH_SHA256);

    if (rc != PB_OK) {
        pb_wire_init_result(&result, error_to_wire(rc));
        return rc;
    }

    if (verify_cmd->bpak) {
        LOG_DBG("Bpak header");

        int header_lba = (bio_size(block_dev) - sizeof(struct bpak_header)) / bio_block_size(block_dev);

        rc = bio_read(block_dev, header_lba, sizeof(struct bpak_header),
                        (uintptr_t) buffer);

        if (rc != PB_OK) {
            pb_wire_init_result(&result, error_to_wire(rc));
            return rc;
        }

        rc = hash_update((uintptr_t) buffer, sizeof(struct bpak_header));

        if (rc != PB_OK) {
            pb_wire_init_result(&result, -PB_RESULT_PART_VERIFY_FAILED);
            return rc;
        }

        bytes_to_verify -= sizeof(struct bpak_header);
    }

    LOG_DBG("Reading %zu bytes", bytes_to_verify);

    while (bytes_to_verify) {
        chunk_len = bytes_to_verify>(CONFIG_CM_BUF_SIZE_KB*1024)? \
                   (CONFIG_CM_BUF_SIZE_KB*1024):bytes_to_verify;

        buffer_id = !buffer_id;

        rc = bio_read(block_dev, lba_offset, chunk_len,
                                    (uintptr_t) buffer[buffer_id]);

        if (rc != PB_OK) {
            LOG_ERR("read error");
            pb_wire_init_result(&result, -PB_RESULT_PART_VERIFY_FAILED);
            break;
        }

        rc = hash_update((uintptr_t) buffer[buffer_id], chunk_len);

        if (rc != PB_OK) {
            LOG_ERR("Hash update error");
            pb_wire_init_result(&result, error_to_wire(rc));
            break;
        }

        bytes_to_verify -= chunk_len;
        lba_offset += chunk_len / bio_block_size(block_dev);
    }

    if (rc != PB_OK)
        return rc;

    rc = hash_final(hash, sizeof(hash));

    if (rc != PB_OK) {
        pb_wire_init_result(&result, error_to_wire(rc));
        return rc;
    }


    if (memcmp(hash, verify_cmd->sha256, 32) == 0) {
        rc = PB_OK;
        pb_wire_init_result(&result, error_to_wire(rc));
    } else {
#if LOGLEVEL > 2
        printf("Expected hash:");
        for (int i = 0; i < 32; i++)
            printf("%x", verify_cmd->sha256[i] & 0xff);
        printf("\n\r");
#endif
        LOG_ERR("Verification failed");
        rc = PB_ERR_CHECKSUM;
        pb_wire_init_result(&result, -PB_RESULT_PART_VERIFY_FAILED);
    }

    return rc;
}

static int cmd_part_tbl_read(void)
{

    LOG_DBG("TBL read");
    struct pb_result_part_table_read tbl_read_result = {0};

    struct pb_result_part_table_entry *result_tbl = \
        (struct pb_result_part_table_entry *) buffer;

    int entries = 0;

    for (bio_dev_t dev = 0; bio_valid(dev); dev++) {
        if (!(bio_get_flags(dev) & BIO_FLAG_VISIBLE))
            continue;

        uuid_copy(result_tbl[entries].uuid, bio_get_uu(dev));
        strncpy(result_tbl[entries].description, bio_get_description(dev),
                    sizeof(result_tbl[entries].description) - 1);
        result_tbl[entries].first_block = bio_get_first_block(dev);
        result_tbl[entries].last_block = bio_get_last_block(dev);
        result_tbl[entries].block_size = bio_block_size(dev);
        result_tbl[entries].flags = (bio_get_flags(dev) & 0xFF);

        entries++;
    }

    tbl_read_result.no_of_entries = entries;
    LOG_DBG("%i entries", entries);
    pb_wire_init_result2(&result, PB_RESULT_OK, &tbl_read_result,
                                             sizeof(tbl_read_result));

    cfg->tops.write((uintptr_t) &result, sizeof(result));

    if (entries) {
        size_t bytes = sizeof(struct pb_result_part_table_entry)*(entries);
        LOG_DBG("Bytes %zu", bytes);
        cfg->tops.write((uintptr_t) result_tbl, bytes);
    }

    pb_wire_init_result(&result, PB_RESULT_OK);
    return PB_RESULT_OK;
}

static int ram_boot_read_f(int block_offset, size_t length, uintptr_t buf)
{
    (void) block_offset;
    LOG_DBG("%zu %"PRIxPTR, length, buf);
    return cfg->tops.read((uintptr_t) buf, length);
}

static int ram_boot_result_f(int rc)
{
    LOG_DBG("%i", rc);
    pb_wire_init_result(&result, error_to_wire(rc));
    return cfg->tops.write((uintptr_t) &result, sizeof(result));
}

static int cmd_boot_ram(void)
{
    int rc;
    struct pb_command_ram_boot *ram_boot_cmd = \
                       (struct pb_command_ram_boot *) &cmd.request;

    boot_clear_set_flags(0,
            BOOT_FLAG_CMD |
            (ram_boot_cmd->verbose?BOOT_FLAG_VERBOSE:0));

    pb_wire_init_result(&result, PB_RESULT_OK);
    rc = cfg->tops.write((uintptr_t) &result, sizeof(result));

    if (rc != PB_OK)
        return rc;

    boot_set_source(BOOT_SOURCE_CB);
    boot_configure_load_cb(ram_boot_read_f,
                           ram_boot_result_f);

    return boot(ram_boot_cmd->uuid);
}

static int cmd_slc_read(void)
{
    int rc;
    struct pb_result_slc slc_status = {0};
    rc = plat_slc_read((enum pb_slc *) &slc_status.slc);
    pb_wire_init_result2(&result, error_to_wire(rc), &slc_status,
                        sizeof(slc_status));

    cfg->tops.write((uintptr_t) &result, sizeof(result));

    struct pb_result_slc_key_status *key_status;

    rc = plat_slc_get_key_status(&key_status);
    cfg->tops.write((uintptr_t) key_status, sizeof(*key_status));
    pb_wire_init_result(&result, error_to_wire(rc));

    return rc;
}

static int cmd_stream_init(void)
{
    struct pb_command_stream_initialize *stream_init = \
            (struct pb_command_stream_initialize *) cmd.request;

    block_dev = bio_get_part_by_uu(stream_init->part_uuid);

    if (block_dev < 0)
        pb_wire_init_result(&result, error_to_wire(block_dev));
    else
        pb_wire_init_result(&result, 0);

    if (block_dev < 0)
        return block_dev;
    else
        return PB_OK;
}

static int cmd_stream_prep_buffer(void)
{
    struct pb_command_stream_prepare_buffer *stream_prep = \
        (struct pb_command_stream_prepare_buffer *) cmd.request;

    LOG_DBG("Stream prep %u, %i", stream_prep->size, stream_prep->id);

    if (stream_prep->size > (CONFIG_CM_BUF_SIZE_KB*1024)) {
        pb_wire_init_result(&result, -PB_RESULT_NO_MEMORY);
        return -PB_ERR_MEM;
    }

    if (stream_prep->id > 1) {
        pb_wire_init_result(&result, -PB_RESULT_NO_MEMORY);
        return -PB_ERR_MEM;
    }

    pb_wire_init_result(&result, PB_RESULT_OK);
    cfg->tops.write((uintptr_t) &result, sizeof(result));

    uint8_t *bfr = ((uint8_t *) buffer) +
                    ((CONFIG_CM_BUF_SIZE_KB*1024)*stream_prep->id);

    return cfg->tops.read((uintptr_t) bfr, stream_prep->size);
}

static int cmd_stream_final(void)
{
    pb_wire_init_result(&result, PB_RESULT_OK);
    return PB_OK;
}

static int pb_command_parse(void)
{
    int rc = PB_OK;

    if (!pb_wire_valid_command(&cmd)) {
        LOG_ERR("Invalid command: %i\n", cmd.command);
        pb_wire_init_result(&result, -PB_RESULT_INVALID_COMMAND);
        goto err_out;
    }

    if (pb_wire_requires_auth(&cmd) &&
             (!authenticated) &&
        (slc == PB_SLC_CONFIGURATION_LOCKED)) {
        LOG_ERR("Not authenticated");
        pb_wire_init_result(&result, -PB_RESULT_NOT_AUTHENTICATED);
        goto err_out;
    }

    LOG_DBG("%i", cmd.command);

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
            cfg->tops.write((uintptr_t) &result, sizeof(result));
            return -PB_ERR_ABORT;
        }
        break;
        case PB_CMD_PART_TBL_READ:
            rc = cmd_part_tbl_read();
        break;
        case PB_CMD_DEVICE_READ_CAPS:
        {
            struct pb_result_device_caps caps = {0};
            caps.stream_no_of_buffers = 2;
            caps.stream_buffer_size = CONFIG_CM_BUF_SIZE_KB*1024;
            caps.chunk_transfer_max_bytes = CONFIG_CM_BUF_SIZE_KB*1024;

            pb_wire_init_result2(&result, PB_RESULT_OK, &caps, sizeof(caps));
        }
        break;
        case PB_CMD_BOOT_RAM:
            rc = cmd_boot_ram();
            return rc; /* Should not return, we should not try to send a result */
        break;
        case PB_CMD_PART_TBL_INSTALL: /* Deprecated */
        {
            struct pb_command_install_part_table *install_cmd = \
                (struct pb_command_install_part_table *) cmd.request;
            rc = bio_install_partition_table(install_cmd->uu,
                                             install_cmd->variant);
            pb_wire_init_result(&result, error_to_wire(rc));
        }
        break;
        case PB_CMD_STREAM_INITIALIZE:
            rc = cmd_stream_init();
        break;
        case PB_CMD_STREAM_PREPARE_BUFFER:
            rc = cmd_stream_prep_buffer();
        break;
        case PB_CMD_STREAM_READ_BUFFER:
            rc = cmd_stream_read();
        break;
        case PB_CMD_STREAM_WRITE_BUFFER:
            rc = cmd_stream_write();
        break;
        case PB_CMD_STREAM_FINALIZE:
            rc = cmd_stream_final();
        break;
        case PB_CMD_PART_BPAK_READ:
            rc = cmd_bpak_read();
        break;
        case PB_CMD_PART_VERIFY:
            rc = cmd_part_verify();
        break;
        case PB_CMD_BOOT_PART:
        {
            struct pb_command_boot_part *boot_cmd = \
                (struct pb_command_boot_part *) cmd.request;


            boot_set_source(BOOT_SOURCE_BIO);
            boot_clear_set_flags(0,
                    BOOT_FLAG_CMD |
                    (boot_cmd->verbose?BOOT_FLAG_VERBOSE:0));
            rc = boot_load(boot_cmd->uuid);

            pb_wire_init_result(&result, error_to_wire(rc));
            cfg->tops.write((uintptr_t) &result, sizeof(result));

            if (rc == PB_OK) {
                rc = boot_jump();
                /* Should not return */
            }
            return rc;
        }
        break;
        case PB_CMD_DEVICE_IDENTIFIER_READ:
        {
            struct pb_result_device_identifier *ident = \
                (struct pb_result_device_identifier *) buffer;
            memset(ident->board_id, 0, sizeof(ident->board_id));
            memcpy(ident->board_id, cfg->name, strlen(cfg->name));
            memcpy(ident->device_uuid, device_uu, 16);
            pb_wire_init_result2(&result, error_to_wire(rc),
                                    ident, sizeof(*ident));
        }
        break;
        case PB_CMD_PART_ACTIVATE:
        {
            struct pb_command_activate_part *activate_cmd = \
                (struct pb_command_activate_part *) cmd.request;
            rc = boot_set_boot_partition(activate_cmd->uuid);
            pb_wire_init_result(&result, error_to_wire(rc));
        }
        break;
        case PB_CMD_AUTHENTICATE:
            rc = cmd_auth();
        break;
        case PB_CMD_AUTH_SET_OTP_PASSWORD:
        {
            pb_wire_init_result(&result, -PB_RESULT_NOT_SUPPORTED);
        }
        break;
        case PB_CMD_BOARD_COMMAND:
            rc = cmd_board();
        break;
        case PB_CMD_BOARD_STATUS_READ:
        {
            struct pb_result_board_status status_result = {0};

            uint8_t *bfr_response = buffer[0];

            size_t response_size = CONFIG_CM_BUF_SIZE_KB*1024;
            rc = cfg->status(bfr_response, &response_size);

            status_result.size = response_size;
            pb_wire_init_result2(&result, error_to_wire(rc),
                            &status_result, sizeof(status_result));

            cfg->tops.write((uintptr_t) &result, sizeof(result));
            cfg->tops.write((uintptr_t) bfr_response, response_size);
        }
        break;
        case PB_CMD_SLC_SET_CONFIGURATION:
        {
            LOG_DBG("Set configuration");
            rc = plat_slc_set_configuration();
            pb_wire_init_result(&result, error_to_wire(rc));
            plat_slc_read(&slc);
        }
        break;
        case PB_CMD_SLC_SET_CONFIGURATION_LOCK:
        {
            LOG_DBG("Set configuration lock");
            rc = plat_slc_set_configuration_lock();
            pb_wire_init_result(&result, error_to_wire(rc));
            plat_slc_read(&slc);
        }
        break;
        case PB_CMD_SLC_SET_EOL:
        {
            LOG_DBG("Set EOL");
            rc = plat_slc_set_end_of_life();
            pb_wire_init_result(&result, error_to_wire(rc));
            plat_slc_read(&slc);
        }
        break;
        case PB_CMD_SLC_REVOKE_KEY:
        {
            struct pb_command_revoke_key *revoke_cmd = \
                (struct pb_command_revoke_key *) cmd.request;

            LOG_DBG("Revoke key %x", revoke_cmd->key_id);

            rc = plat_slc_revoke_key(revoke_cmd->key_id);
            pb_wire_init_result(&result, error_to_wire(rc));
        }
        break;
        case PB_CMD_SLC_READ:
            rc = cmd_slc_read();
        break;
        case PB_CMD_PART_RESIZE:
        {
            /* Deprecated and no longer supported,
             * Use partition table configration index instead. */
            pb_wire_init_result(&result, -PB_RESULT_NOT_SUPPORTED);
        }
        break;
        case PB_CMD_BOOT_STATUS:
        {
            struct pb_result_boot_status boot_result = {0};
            boot_get_boot_partition(boot_result.uuid);

            pb_wire_init_result2(&result, error_to_wire(rc), &boot_result,
                                    sizeof(boot_result));
            rc = PB_RESULT_OK;
        }
        break;
        default:
        {
            LOG_ERR("Got unknown command: %u", cmd.command);
            pb_wire_init_result(&result, -PB_RESULT_INVALID_COMMAND);
        }
    }

err_out:
    cfg->tops.write((uintptr_t) &result, sizeof(result));
    return rc;
}

int cm_run(void)
{
    int rc;
    LOG_INFO("Initializing command mode");

    cfg = cm_board_init();

    if (cfg == NULL) {
        LOG_ERR("Board init failed");
        return -PB_ERR_IO;
    }

#ifdef CONFIG_CM_AUTH
    authenticated = false;
#else
    authenticated = true;
#endif
    plat_slc_read(&slc);
    device_uuid(device_uu);

restart_command_mode:
    if (cfg->tops.init) {
        rc = cfg->tops.init();

        if (rc != PB_OK) {
            LOG_ERR("Transport init failed (%i)", rc);
            return -PB_ERR_IO;
        }
    }

    if (cfg->tops.connect) {
        LOG_INFO("Waiting for transport to become ready...");

        struct pb_timeout to;
        pb_timeout_init_us(&to, CONFIG_CM_TRANSPORT_READY_TIMEOUT * 1000000L);

        do {
            plat_wdog_kick();
            rc = cfg->tops.connect();
            if (pb_timeout_has_expired(&to)) {
                rc = -PB_ERR_TIMEOUT;
                break;
            }
        } while (rc == -PB_ERR_AGAIN);

        if (rc != PB_OK) {
            goto err_out;
        }
    }

    while (true) {
        plat_wdog_kick();
        rc = cfg->tops.read((uintptr_t) &cmd, sizeof(cmd));

        if (rc == PB_OK) {
            rc = pb_command_parse();

            if (rc == -PB_ERR_ABORT) {
                break;
            } else if (rc != PB_OK) {
                LOG_ERR("Command error %i", rc);
            }
        } else if (rc == -PB_ERR_TIMEOUT) {
            continue;
        } else if (rc == -PB_ERR_AGAIN) {
            continue;
        } else {
            LOG_ERR("Read error %i", rc);
            goto restart_command_mode;
        }
    }

err_out:
    if (cfg->tops.disconnect) {
        cfg->tops.disconnect();
    }
    return rc;
}
