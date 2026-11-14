/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Command mode's partitions and stream (command-mode.md 5.3, 5.4): the
 * partition table, bpak headers, verify, erase, and the stream's buffers.
 */

#include "cm_internal.h"
#include <boot/policy.h>
#include <bpak/bpak.h>
#include <inttypes.h>
#include <pb/bio.h>
#include <pb/crypto.h>
#include <pb/plat.h>
#include <string.h>
#include <uuid.h>

/*
 * PART_VERIFY and PART_BPAK_READ read the partition into two buffers of
 * their own: the stream's two keep what the host put there (command-mode.md
 * 5.3). Two, since the hash of one chunk runs while the next is read. Each
 * is a quarter of a stream buffer, and at least a bpak header and a block
 * of any device.
 */
#define CM_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define CM_PART_BUF_SIZE \
    CM_MAX(CM_BUF_SIZE / 4, CM_MAX(sizeof(struct bpak_header), CONFIG_BIO_MAX_BLOCK_SIZE))

static uint8_t part_buffer_0[CM_PART_BUF_SIZE] __section(".no_init") __aligned(4096);
static uint8_t part_buffer_1[CM_PART_BUF_SIZE] __section(".no_init") __aligned(4096);
static uint8_t *const part_buffer[2] = { part_buffer_0, part_buffer_1 };

static bio_dev_t block_dev;
/* Whether a write in the current stream succeeded, and whether one failed */
static bool stream_written;
static bool stream_failed;

/* The stream ends: no partition is selected, and nothing of a write is left
 * to finalize (command-mode.md 5.4) */
void cm_stream_end(void)
{
    /* No stream: not a bio, and no bio_* call accepts it */
    block_dev = -PB_ERR_STATE;
    stream_written = false;
    stream_failed = false;
}

/* A partition without VISIBLE is internal to pb: it doesn't exist for the
 * host, and every command that names it answers NOT_FOUND (command-mode.md
 * 5.3, D-058) */
bool cm_part_hidden(const uuid_t uu)
{
    bio_dev_t dev = bio_get_part_by_uu(uu);

    return (dev >= 0) && !(bio_get_flags(dev) & BIO_FLAG_VISIBLE);
}

static bio_dev_t cm_part_by_uu(const uuid_t uu)
{
    if (cm_part_hidden(uu))
        return -PB_ERR_NOT_FOUND;

    return bio_get_part_by_uu(uu);
}

int cm_cmd_bpak_read(void)
{
    int rc;
    struct pb_command_read_bpak *read_cmd = (struct pb_command_read_bpak *)cm_cmd.request;

    LOG_DBG("Read bpak");

    bio_dev_t dev = cm_part_by_uu(read_cmd->uuid);

    if (dev < 0) {
        LOG_ERR("Could not find partition");
        pb_wire_init_result(&cm_result, cm_error_to_wire(dev));
        return dev;
    }

    /* No room for a header: there is none (command-mode.md 5.3) */
    if (bio_size(dev) < (int64_t)sizeof(struct bpak_header)) {
        LOG_ERR("Partition is smaller than a bpak header");
        pb_wire_init_result(&cm_result, -PB_RESULT_NOT_FOUND);
        return -PB_ERR_NOT_FOUND;
    }

    lba_t header_lba =
        bio_get_no_of_blocks(dev) - (sizeof(struct bpak_header) / bio_block_size(dev));

    LOG_DBG("Reading bpak header at lba %i", header_lba);

    rc = bio_read(dev, header_lba, sizeof(struct bpak_header), part_buffer[0]);

    if (rc != PB_OK) {
        pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
        return rc;
    }

    rc = bpak_valid_header((struct bpak_header *)part_buffer[0]);

    /* bpak's code is logged here, as bpak's: the command's error is pb's */
    if (rc != BPAK_OK) {
        LOG_ERR("Invalid bpak header (bpak: %i)", rc);
        pb_wire_init_result(&cm_result, -PB_RESULT_NOT_FOUND);
        return -PB_ERR_NOT_FOUND;
    }

    pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
    cm_write(&cm_result, sizeof(cm_result));

    rc = cm_write(part_buffer[0], sizeof(struct bpak_header));

    pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
    return rc;
}

/* Validates a stream read/write request and returns the buffer and the
 * first block to transfer */
static int
stream_check(uint8_t buffer_id, uint32_t size, uint64_t offset, uint8_t **bfr, lba_t *start_lba)
{
    if (buffer_id >= ARRAY_SIZE(cm_buffer) || size > sizeof(cm_buffer[0])) {
        LOG_ERR("Invalid buffer %u or size %u", buffer_id, size);
        pb_wire_init_result(&cm_result, -PB_RESULT_NO_MEMORY);
        return -PB_ERR_MEM;
    }

    ssize_t block_size = bio_block_size(block_dev);

    if (block_size <= 0) {
        pb_wire_init_result(&cm_result, -PB_RESULT_STREAM_NOT_INITIALIZED);
        return -PB_ERR_PARAM;
    }

    uint64_t lba = offset / (uint64_t)block_size;

    /* The offset is a whole number of blocks, as hosts send it
     * (command-mode.md 5.4) */
    if (offset % (uint64_t)block_size != 0) {
        LOG_ERR("Offset %" PRIu64 " isn't a multiple of the block", offset);
        pb_wire_init_result(&cm_result, -PB_RESULT_INVALID_ARGUMENT);
        return -PB_ERR_PARAM;
    }

    if (lba > UINT32_MAX) {
        LOG_ERR("Invalid offset");
        pb_wire_init_result(&cm_result, -PB_RESULT_INVALID_ARGUMENT);
        return -PB_ERR_PARAM;
    }

    *bfr = cm_buffer[buffer_id];
    *start_lba = (lba_t)lba;
    return PB_OK;
}

/*
 * No stream (command-mode.md 5.4): before a STREAM_INITIALIZE that succeeded
 * in this session, and after one that failed, 'block_dev' is an error code
 * and no device. A read and a write answer that first: an error code read
 * as flags has the bits set (F-074).
 */
static bool stream_missing(void)
{
    if (bio_valid(block_dev))
        return false;

    LOG_ERR("No stream");
    pb_wire_init_result(&cm_result, -PB_RESULT_STREAM_NOT_INITIALIZED);
    return true;
}

int cm_cmd_stream_read(void)
{
    int rc = -PB_ERR;
    struct pb_command_stream_read_buffer *stream_read =
        (struct pb_command_stream_read_buffer *)cm_cmd.request;

    LOG_DBG("Stream read %u, %" PRIu64 ", %i",
            stream_read->buffer_id,
            stream_read->offset,
            stream_read->size);

    if (stream_missing())
        return -PB_ERR_STATE;

    if (!(bio_get_flags(block_dev) & BIO_FLAG_READABLE)) {
        LOG_ERR("Partition may not be read");
        pb_wire_init_result(&cm_result, -PB_RESULT_IO_ERROR);
        return -PB_ERR_IO;
    }

    uint8_t *bfr;
    lba_t start_lba;

    rc = stream_check(
        stream_read->buffer_id, stream_read->size, stream_read->offset, &bfr, &start_lba);

    if (rc != PB_OK)
        return rc;

    LOG_DBG("Reading %u bytes at lba offset %u", stream_read->size, start_lba);

    rc = bio_read(block_dev, start_lba, stream_read->size, bfr);
    LOG_DBG("Result = %i", rc);

    if (rc != PB_OK) {
        /* The result is sent by pb_command_parse */
        pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
        return rc;
    }

    pb_wire_init_result(&cm_result, PB_RESULT_OK);
    cm_write(&cm_result, sizeof(cm_result));

    rc = cm_write(bfr, stream_read->size);
    pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
    LOG_DBG("Data sent");
    return rc;
}

static int stream_write(void)
{
    int rc;
    struct pb_command_stream_write_buffer *stream_write =
        (struct pb_command_stream_write_buffer *)cm_cmd.request;

    LOG_DBG("Stream write %u, %" PRIu64 ", %i",
            stream_write->buffer_id,
            stream_write->offset,
            stream_write->size);

    if (stream_missing())
        return -PB_ERR_STATE;

    if (!(bio_get_flags(block_dev) & BIO_FLAG_WRITABLE)) {
        LOG_ERR("Partition may not be written");
        rc = -PB_ERR_IO;
        pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
        return rc;
    }

    uint8_t *bfr;
    lba_t start_lba;

    rc = stream_check(
        stream_write->buffer_id, stream_write->size, stream_write->offset, &bfr, &start_lba);

    if (rc != PB_OK)
        return rc;

    LOG_DBG("Writing %u bytes to lba offset %u", stream_write->size, start_lba);

    rc = bio_write(block_dev, start_lba, stream_write->size, bfr);

    pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
    return rc;
}

int cm_cmd_part_verify(void)
{
    int rc;
    int buffer_id = 0;
    size_t chunk_len;
    size_t chunk_max;
    struct pb_command_verify_part *verify_cmd = (struct pb_command_verify_part *)cm_cmd.request;
    size_t bytes_to_verify = verify_cmd->size;
    int lba_offset = 0;

    LOG_DBG("Verify part");

    /* The stream stays on its partition (command-mode.md 5.3) */
    bio_dev_t dev = cm_part_by_uu(verify_cmd->uuid);

    if (dev < 0) {
        LOG_ERR("Could not find partition");
        pb_wire_init_result(&cm_result, cm_error_to_wire(dev));
        return dev;
    }

    /* With bpak the size is at least a header, inside the partition
     * (command-mode.md 5.3): a partition smaller than that takes none */
    if (verify_cmd->bpak && bio_size(dev) < (int64_t)sizeof(struct bpak_header)) {
        LOG_ERR("Partition is smaller than a bpak header");
        pb_wire_init_result(&cm_result, -PB_RESULT_INVALID_ARGUMENT);
        return -PB_ERR_PARAM;
    }

    /* The header is part of the size: less than one would wrap below */
    if (verify_cmd->bpak && verify_cmd->size < sizeof(struct bpak_header)) {
        LOG_ERR("Size %" PRIu32 " is less than a bpak header", verify_cmd->size);
        pb_wire_init_result(&cm_result, -PB_RESULT_INVALID_ARGUMENT);
        return -PB_ERR_PARAM;
    }

    rc = hash_init(HASH_SHA256);

    if (rc != PB_OK) {
        pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
        return rc;
    }

    if (verify_cmd->bpak) {
        LOG_DBG("Bpak header");

        lba_t header_lba =
            bio_get_no_of_blocks(dev) - (sizeof(struct bpak_header) / bio_block_size(dev));

        rc = bio_read(dev, header_lba, sizeof(struct bpak_header), part_buffer[buffer_id]);

        if (rc != PB_OK) {
            pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
            return rc;
        }

        rc = hash_update_async(part_buffer[buffer_id], sizeof(struct bpak_header));

        if (rc != PB_OK) {
            pb_wire_init_result(&cm_result, -PB_RESULT_PART_VERIFY_FAILED);
            return rc;
        }

        plat_wdog_kick(); /* Once per chunk (command-mode.md 4.6) */

        bytes_to_verify -= sizeof(struct bpak_header);
    }

    LOG_DBG("Reading %zu bytes", bytes_to_verify);

    /* A chunk is whole blocks of the device, so that the next one starts on
     * a block. At least one: no device has a block above
     * CONFIG_BIO_MAX_BLOCK_SIZE (bio.md 2.3) */
    chunk_max = CM_PART_BUF_SIZE - (CM_PART_BUF_SIZE % (size_t)bio_block_size(dev));

    while (bytes_to_verify) {
        chunk_len = bytes_to_verify > chunk_max ? chunk_max : bytes_to_verify;

        buffer_id = !buffer_id;

        rc = bio_read(dev, lba_offset, chunk_len, part_buffer[buffer_id]);

        if (rc != PB_OK) {
            LOG_ERR("read error");
            pb_wire_init_result(&cm_result, -PB_RESULT_PART_VERIFY_FAILED);
            break;
        }

        rc = hash_update_async(part_buffer[buffer_id], chunk_len);

        if (rc != PB_OK) {
            LOG_ERR("Hash update error");
            pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
            break;
        }

        /* Once per chunk, after it is read and its hash started, as the
         * boot's load does (command-mode.md 4.6) */
        plat_wdog_kick();

        bytes_to_verify -= chunk_len;
        lba_offset += chunk_len / bio_block_size(dev);
    }

    if (rc != PB_OK)
        return rc;

    rc = hash_final(cm_hash, sizeof(cm_hash));

    if (rc != PB_OK) {
        pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
        return rc;
    }

    if (memcmp(cm_hash, verify_cmd->sha256, 32) == 0) {
        rc = PB_OK;
        pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
    } else {
#if LOG_MAX_LEVEL >= 3
        static const char hex[] = "0123456789abcdef";
        char hash_str[65];

        /* Not snprintf(), F-089 */
        for (int i = 0; i < 32; i++) {
            hash_str[i * 2] = hex[verify_cmd->sha256[i] >> 4];
            hash_str[i * 2 + 1] = hex[verify_cmd->sha256[i] & 0xf];
        }
        hash_str[64] = '\0';
        LOG_DBG("Expected hash: %s", hash_str);
#endif
        LOG_ERR("Verification failed");
        rc = -PB_ERR_CHECKSUM;
        pb_wire_init_result(&cm_result, -PB_RESULT_PART_VERIFY_FAILED);
    }

    return rc;
}

int cm_cmd_part_erase(struct pb_command_erase_part *erase_cmd)
{
    LOG_DBG("Erase: lba=%" PRIu32 " count=%" PRIu32, erase_cmd->start_lba, erase_cmd->block_count);
    bio_dev_t dev = cm_part_by_uu(erase_cmd->uuid);

    if (dev < 0)
        return dev;

    /* Hosts send the partition's first block, from the partition table, plus
     * the blocks already erased: an LBA of the parent device. bio_erase()
     * wants it relative to the partition (F-055). A count of 0, which hosts
     * older than 2.0 send (F-075), erases nothing, as before. */
    if (erase_cmd->block_count == 0)
        return bio_erase(dev, 0, 0);

    /* As a stream write: the partition's flags forbid it (command-mode.md
     * 5.3). The device a partition table is on has only VISIBLE */
    if (!(bio_get_flags(dev) & BIO_FLAG_WRITABLE)) {
        LOG_ERR("Partition may not be erased");
        return -PB_ERR_IO;
    }

    lba_t first = (lba_t)bio_get_first_block(dev);

    if (erase_cmd->start_lba < first) {
        LOG_ERR("Erase before the partition, lba=%" PRIu32, erase_cmd->start_lba);
        return -PB_ERR_PARAM;
    }

    return bio_erase(dev, erase_cmd->start_lba - first, erase_cmd->block_count);
}

/*
 * PART_TBL_INSTALL (command-mode.md 5.3): the new table is in effect for the
 * commands that follow. bio replaces the partitions that changed (bio.md 4),
 * and the core sets up again what is its own: the stream ends, and the boot
 * source policy's init runs again, for its state's partitions and its
 * candidates. No hook of the board's runs.
 */
int cm_cmd_part_tbl_install(const struct pb_command_install_part_table *install_cmd)
{
    int rc;

    if (cm_part_hidden(install_cmd->uu))
        return -PB_ERR_NOT_FOUND;

    rc = bio_install_partition_table(install_cmd->uu, install_cmd->variant);

    /* An install that failed after it wrote the device released the old
     * partitions: a stream on one of them has ended too */
    if (rc == PB_OK || !bio_valid(block_dev))
        cm_stream_end();

    if (rc == PB_OK) {
        /* The table is installed, whatever the policy makes of it: a layout
         * without the policy's partitions answered OK in every release, and
         * the boot commands report the policy's state (board.md 2.1) */
        int policy_rc = boot_policy_reinit();

        if (policy_rc != PB_OK)
            LOG_ERR("Boot policy init failed (%i)", policy_rc);
    }

    return rc;
}

int cm_cmd_part_tbl_read(void)
{
    LOG_DBG("TBL read");
    struct pb_result_part_table_read tbl_read_result = { 0 };

    struct pb_result_part_table_entry *result_tbl = (struct pb_result_part_table_entry *)cm_buffer;

    int entries = 0;

    /* Every handle: one that isn't a device, a partition that an install
     * released, can lie between two that are (bio.md 2.6) */
    for (bio_dev_t dev = 0; dev < CONFIG_BIO_MAX_DEVS; dev++) {
        if (!bio_valid(dev))
            continue;

        if (!(bio_get_flags(dev) & BIO_FLAG_VISIBLE))
            continue;

        /* The reserved bytes and the end of the description are zero, not
         * what the buffer held (command-mode.md 4.3) */
        memset(&result_tbl[entries], 0, sizeof(result_tbl[entries]));
        uuid_copy(result_tbl[entries].uuid, bio_get_uu(dev));
        strncpy(result_tbl[entries].description,
                bio_get_description(dev),
                sizeof(result_tbl[entries].description) - 1);
        result_tbl[entries].first_block = bio_get_first_block(dev);
        result_tbl[entries].last_block = bio_get_last_block(dev);
        result_tbl[entries].block_size = bio_block_size(dev);
        result_tbl[entries].flags = (bio_get_flags(dev) & 0xFF);

        entries++;
    }

    tbl_read_result.no_of_entries = entries;
    pb_wire_init_result2(&cm_result, PB_RESULT_OK, &tbl_read_result, sizeof(tbl_read_result));

    cm_write(&cm_result, sizeof(cm_result));

    if (entries) {
        size_t bytes = sizeof(struct pb_result_part_table_entry) * (entries);
        cm_write(result_tbl, bytes);
    }

    pb_wire_init_result(&cm_result, PB_RESULT_OK);
    return PB_RESULT_OK;
}

int cm_cmd_stream_init(void)
{
    struct pb_command_stream_initialize *stream_init =
        (struct pb_command_stream_initialize *)cm_cmd.request;

    block_dev = cm_part_by_uu(stream_init->part_uuid);
    stream_written = false;
    stream_failed = false;

    if (block_dev < 0)
        pb_wire_init_result(&cm_result, cm_error_to_wire(block_dev));
    else
        pb_wire_init_result(&cm_result, 0);

    if (block_dev < 0)
        return block_dev;
    else
        return PB_OK;
}

static int stream_prep_buffer(void)
{
    struct pb_command_stream_prepare_buffer *stream_prep =
        (struct pb_command_stream_prepare_buffer *)cm_cmd.request;

    LOG_DBG("Stream prep %u, %i", stream_prep->size, stream_prep->id);

    /* Hosts send the data whatever the first result says: it is read and
     * dropped, and the second result has the refusal (command-mode.md 4.4) */
    if (stream_prep->size > CM_BUF_SIZE || stream_prep->id > 1) {
        pb_wire_init_result(&cm_result, -PB_RESULT_NO_MEMORY);
        cm_write(&cm_result, sizeof(cm_result));
        (void)cm_drain(stream_prep->size);
        pb_wire_init_result(&cm_result, -PB_RESULT_NO_MEMORY);
        return -PB_ERR_MEM;
    }

    pb_wire_init_result(&cm_result, PB_RESULT_OK);
    cm_write(&cm_result, sizeof(cm_result));

    uint8_t *bfr = ((uint8_t *)cm_buffer) + (CM_BUF_SIZE * stream_prep->id);

    return cm_read(bfr, stream_prep->size);
}

int cm_cmd_stream_final(void)
{
    int rc = PB_OK;

    /*
     * The host finalizes after a failed write too, so only a stream without
     * errors is a complete write. On eMMC this makes a boot partition
     * bootable (F-082).
     */
    if (stream_written && !stream_failed)
        rc = bio_write_done(block_dev);

    stream_written = false;
    stream_failed = false;

    pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
    return rc;
}

/* A write that failed, and one that succeeded: STREAM_FINALIZE completes
 * only a stream without a failure (F-082) */
int cm_cmd_stream_prep_buffer(void)
{
    int rc = stream_prep_buffer();

    if (rc != PB_OK)
        stream_failed = true;

    return rc;
}

int cm_cmd_stream_write(void)
{
    int rc = stream_write();

    if (rc == PB_OK)
        stream_written = true;
    else
        stream_failed = true;

    return rc;
}
