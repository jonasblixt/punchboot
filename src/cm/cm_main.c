/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <boot/boot.h>
#include <boot/linux_ramoops.h>
#include <bpak/bpak.h>
#include <bpak/keystore.h>
#include <inttypes.h>
#include <pb-tools/wire.h>
#include <pb/bio.h>
#include <pb/cm.h>
#include <pb/crypto.h>
#include <pb/delay.h>
#include <pb/device_uuid.h>
#include <pb/log_source.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <pb/task.h>
#include <stdio.h>
#include <string.h>
#include <uuid.h>

#define CM_BUF_SIZE       (CONFIG_CM_BUF_SIZE_KiB * 1024)
/* Hosts send authentication and board command data in one transfer of this
 * size, whatever the data's size */
#define CM_DATA_XFER_SIZE 1024

static struct pb_command cmd __section(".no_init") __aligned(64);
static struct pb_result result __section(".no_init") __aligned(64);
static bool authenticated = false;
static uint8_t buffer[2][CM_BUF_SIZE] __section(".no_init") __aligned(4096);

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

/* What the handlers put in the buffers without a check at run time fits a
 * build's buffers: the 1024 bytes of AUTHENTICATE and of a board's request
 * in one, a partition table entry per block device in the two, whose count
 * is a byte on the wire */
_Static_assert(CM_BUF_SIZE >= CM_DATA_XFER_SIZE, "CONFIG_CM_BUF_SIZE_KiB: below a data transfer");
_Static_assert(CONFIG_BIO_MAX_DEVS <= UINT8_MAX,
               "CONFIG_BIO_MAX_DEVS: the table's count is a byte");
_Static_assert(CONFIG_BIO_MAX_DEVS * sizeof(struct pb_result_part_table_entry) <= sizeof(buffer),
               "CONFIG_CM_BUF_SIZE_KiB: no room for CONFIG_BIO_MAX_DEVS table entries");
static slc_t slc;

/* The part table response sends the low byte of the bio flags and the SLC
 * response sends slc_t, so both must match the wire values (D-008). */
_Static_assert(BIO_FLAG_BOOTABLE == PB_WIRE_PART_FLAG_BOOTABLE, "bio flag");
_Static_assert(BIO_FLAG_WRITABLE == PB_WIRE_PART_FLAG_WRITABLE, "bio flag");
_Static_assert(BIO_FLAG_ERASE_BEFORE_WRITE == PB_WIRE_PART_FLAG_ERASE_BEFORE_WRITE, "bio flag");
_Static_assert((int)SLC_NOT_CONFIGURED == (int)PB_SLC_NOT_CONFIGURED, "slc value");
_Static_assert((int)SLC_CONFIGURATION == (int)PB_SLC_CONFIGURATION, "slc value");
_Static_assert((int)SLC_CONFIGURATION_LOCKED == (int)PB_SLC_CONFIGURATION_LOCKED, "slc value");
_Static_assert((int)SLC_EOL == (int)PB_SLC_EOL, "slc value");
static uint8_t hash[CRYPTO_MD_MAX_SZ];
static uuid_t device_uu;
static bio_dev_t block_dev;
/* Whether a write in the current stream succeeded, and whether one failed */
static bool stream_written;
static bool stream_failed;
static bool reboot_requested = false;
static const struct cm_config *cfg;
/* A transfer inside a command failed or timed out (command-mode.md 4.4,
 * 4.6): the session is over, the command's other transfers fail at once and
 * the task restarts the transport */
static bool transfer_lost;

#ifdef CONFIG_BOOT_LINUX_RAMOOPS
static const struct linux_ramoops_config ramoops = {
    .addr = CONFIG_BOOT_LINUX_RAMOOPS_ADDR,
    .size = CONFIG_BOOT_LINUX_RAMOOPS_SIZE,
    .record_size = CONFIG_BOOT_LINUX_RAMOOPS_RECORD_SIZE,
    .console_size = CONFIG_BOOT_LINUX_RAMOOPS_CONSOLE_SIZE,
    .ftrace_size = CONFIG_BOOT_LINUX_RAMOOPS_FTRACE_SIZE,
    .pmsg_size = CONFIG_BOOT_LINUX_RAMOOPS_PMSG_SIZE,
};
#endif

#ifdef CONFIG_CM_AUTH_TOKEN
static int auth_token(uint32_t key_id, uint8_t *sig, size_t size)
{
    int rc = -PB_ERR;
    bool verified = false;
    char device_uu_str[37];
    const uint8_t *key_der_data = NULL;
    size_t key_der_data_length = 0;
    hash_t hash_kind = 0;
    dsa_t dsa_kind = 0;

    rc = rot_read_key_status(key_id);

    if (rc != PB_OK)
        return rc;

    rc = rot_get_dsa_key(key_id, &dsa_kind, &key_der_data, &key_der_data_length);

    if (rc != PB_OK)
        return rc;

    uuid_unparse(device_uu, device_uu_str);

    switch (dsa_kind) {
    case DSA_EC_SECP256r1:
        hash_kind = HASH_SHA256;
        break;
    case DSA_EC_SECP384r1:
        hash_kind = HASH_SHA384;
        break;
    case DSA_EC_SECP521r1:
        hash_kind = HASH_SHA512;
        break;
    default:
        return -PB_ERR_NOT_SUPPORTED;
    }

    rc = hash_init(hash_kind);

    if (rc != PB_OK)
        return rc;

    rc = hash_update(device_uu_str, 36);

    if (rc != PB_OK)
        return rc;

    rc = hash_final(hash, sizeof(hash));

    if (rc != PB_OK)
        return rc;

    rc = dsa_verify(dsa_kind,
                    sig,
                    size,
                    key_der_data,
                    key_der_data_length,
                    hash_kind,
                    hash,
                    sizeof(hash),
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
    case -PB_ERR_PARAM:
        return -PB_RESULT_INVALID_ARGUMENT;
    case -PB_ERR_NOT_SUPPORTED:
        return -PB_RESULT_NOT_SUPPORTED;
    default:
        return -PB_RESULT_ERROR;
    }
}

void cm_board_cmd_request_reboot_on_success(void)
{
    reboot_requested = true;
}

/*
 * The session ends with the transport (command-mode.md 3.1): when it is
 * (re)started and when a transfer is lost, which is how the transport
 * reports an unplug and a bus reset (usb.md 2.4). The next host starts
 * without authentication and without a stream (5.4).
 */
static void cm_session_end(void)
{
#ifdef CONFIG_CM_AUTH
    authenticated = false;
#else
    authenticated = true;
#endif
    /* No stream: not a bio, and no bio_* call accepts it */
    block_dev = -PB_ERR_STATE;
    stream_written = false;
    stream_failed = false;
}

/*
 * Waits for the transfer that was started, for at most
 * CONFIG_CM_TRANSFER_TIMEOUT, and kicks the watchdog while it waits
 * (command-mode.md 4.6): a host that stops in the middle of an exchange
 * neither holds pb for ever nor resets it.
 */
static int cm_transfer_wait(void)
{
    struct pb_timeout to;
    int rc;

    pb_timeout_init_us(&to, CONFIG_CM_TRANSFER_TIMEOUT * 1000000UL);

    do {
        plat_wdog_kick();
        rc = cm_transport_complete();

        if (rc == -PB_ERR_AGAIN && pb_timeout_has_expired(&to)) {
            LOG_ERR("Transfer timed out");
            rc = -PB_ERR_TIMEOUT;
        }
    } while (rc == -PB_ERR_AGAIN);

    return rc;
}

static int cm_write(const void *buf, size_t length)
{
    int rc;

    if (transfer_lost)
        return -PB_ERR_IO;

    rc = cm_transport_write(buf, length);

    if (rc == PB_OK)
        rc = cm_transfer_wait();

    /* A transfer that failed ends the session (command-mode.md 4.4) */
    if (rc != PB_OK) {
        transfer_lost = true;
        cm_session_end();
    }

    return rc;
}

static int cm_read(void *buf, size_t length)
{
    int rc;

    if (transfer_lost)
        return -PB_ERR_IO;

    rc = cm_transport_read(buf, length);

    if (rc == PB_OK)
        rc = cm_transfer_wait();

    /* A transfer that failed ends the session (command-mode.md 4.4) */
    if (rc != PB_OK) {
        transfer_lost = true;
        cm_session_end();
    }

    return rc;
}

/*
 * Reads a transfer of 'length' bytes that the host sends whatever the first
 * result said, and drops it: a refused command's exchange still ends as the
 * host expects (command-mode.md 3.2, 4.4). It is read in pieces into the
 * result, which the caller sets afterwards, so that no buffer changes.
 */
static int cm_drain(size_t length)
{
    int rc;

    do {
        size_t chunk = (length > sizeof(result)) ? sizeof(result) : length;

        rc = cm_read(&result, chunk);

        if (rc != PB_OK)
            return rc;

        length -= chunk;
    } while (length > 0);

    return PB_OK;
}

/* A partition without VISIBLE is internal to pb: it doesn't exist for the
 * host, and every command that names it answers NOT_FOUND (command-mode.md
 * 5.3, D-058) */
static bool cm_part_hidden(const uuid_t uu)
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

static int cmd_board(void)
{
    int rc;

    struct pb_command_board *board_cmd = (struct pb_command_board *)cmd.request;

    struct pb_result_board board_result;

    LOG_DBG("Board command %x", board_cmd->command);
    memset(&board_result, 0, sizeof(board_result));

    uint8_t *bfr = buffer[0];

    pb_wire_init_result(&result, PB_RESULT_OK);
    cm_write(&result, sizeof(result));

    if (board_cmd->request_size) {
        LOG_DBG("Reading request data");
        rc = cm_read(bfr, CM_DATA_XFER_SIZE);

        if (rc != PB_OK) {
            pb_wire_init_result(&result, error_to_wire(rc));
            return rc;
        }
    }

    /* The request transfer is read also when it's refused here, since 2.x
     * hosts don't check the first result */
    if (board_cmd->request_size > CM_DATA_XFER_SIZE) {
        LOG_ERR("Bad board command request size: %" PRIu32, board_cmd->request_size);
        pb_wire_init_result(&result, error_to_wire(-PB_ERR_PARAM));
        return -PB_ERR_PARAM;
    }

    uint8_t *bfr_response = buffer[1];

    /* The response is at most what the host said it can take and at most a
     * buffer (command-mode.md 5.6): a host leaves a larger one on the link.
     * The board is told, and what it reports beyond that isn't sent. */
    size_t response_max = board_cmd->response_buffer_size;

    if (response_max > CM_BUF_SIZE)
        response_max = CM_BUF_SIZE;

    size_t response_size = response_max;

    rc = cfg->command(
        board_cmd->command, bfr, board_cmd->request_size, bfr_response, &response_size);

    if (response_size > response_max)
        response_size = response_max;

    board_result.size = response_size;
    pb_wire_init_result2(&result, error_to_wire(rc), &board_result, sizeof(board_result));

    LOG_DBG("Response: %zu bytes", response_size);

    if (response_size) {
        LOG_DBG("Sending response");
        cm_write(&result, sizeof(result));
        cm_write(bfr_response, response_size);
    }

    return rc;
}

static int cmd_bpak_read(void)
{
    int rc;
    struct pb_command_read_bpak *read_cmd = (struct pb_command_read_bpak *)cmd.request;

    LOG_DBG("Read bpak");

    bio_dev_t dev = cm_part_by_uu(read_cmd->uuid);

    if (dev < 0) {
        LOG_ERR("Could not find partition");
        pb_wire_init_result(&result, error_to_wire(dev));
        return dev;
    }

    /* No room for a header: there is none (command-mode.md 5.3) */
    if (bio_size(dev) < (int64_t)sizeof(struct bpak_header)) {
        LOG_ERR("Partition is smaller than a bpak header");
        pb_wire_init_result(&result, -PB_RESULT_NOT_FOUND);
        return -PB_ERR_NOT_FOUND;
    }

    lba_t header_lba =
        bio_get_no_of_blocks(dev) - (sizeof(struct bpak_header) / bio_block_size(dev));

    LOG_DBG("Reading bpak header at lba %i", header_lba);

    rc = bio_read(dev, header_lba, sizeof(struct bpak_header), part_buffer[0]);

    if (rc != PB_OK) {
        pb_wire_init_result(&result, error_to_wire(rc));
        return rc;
    }

    rc = bpak_valid_header((struct bpak_header *)part_buffer[0]);

    /* bpak's code is logged here, as bpak's: the command's error is pb's */
    if (rc != BPAK_OK) {
        LOG_ERR("Invalid bpak header (bpak: %i)", rc);
        pb_wire_init_result(&result, -PB_RESULT_NOT_FOUND);
        return -PB_ERR_NOT_FOUND;
    }

    pb_wire_init_result(&result, error_to_wire(rc));
    cm_write(&result, sizeof(result));

    rc = cm_write(part_buffer[0], sizeof(struct bpak_header));

    pb_wire_init_result(&result, error_to_wire(rc));
    return rc;
}

#if CONFIG_CM_AUTH_PASSWORD || CONFIG_CM_AUTH_TOKEN
/* Hosts send the authentication data after an OK result, and check that
 * result first. A bad size is refused before the data and sets *refused:
 * nothing was tried, so the session stays as it was (command-mode.md 3.3). */
static int auth_read_data(const struct pb_command_authenticate *auth_cmd, bool *refused)
{
    *refused = false;

    if (auth_cmd->size == 0 || auth_cmd->size > CM_DATA_XFER_SIZE) {
        LOG_ERR("Bad authentication data size: %u", auth_cmd->size);
        *refused = true;
        return -PB_ERR_PARAM;
    }

    pb_wire_init_result(&result, PB_RESULT_OK);
    cm_write(&result, sizeof(result));

    return cm_read(buffer[0], CM_DATA_XFER_SIZE);
}

#define CM_AUTH_FAILED_DELAY_US 1000000
#define CM_AUTH_FAILED_KICK_US  100000

/* The result of a failed attempt is sent 1 s after the data arrived, with
 * every method: the check's own time is part of it (command-mode.md 3.3). No
 * counter, no lockout. The tick wraps, so only differences count (plat.h).
 * pb_delay_*() and pb_timeout_*() never kick (platform.md 3.3), this does:
 * at once, after the check, and every 100 ms (the shortest watchdog is 5 s).
 * The transport isn't touched: a host that leaves gets nothing sooner. */
static void auth_failed_wait(unsigned int arrived)
{
    unsigned int now = plat_get_us_tick();
    unsigned int kicked = now;

    plat_wdog_kick();

    while ((now - arrived) < CM_AUTH_FAILED_DELAY_US) {
        if ((now - kicked) >= CM_AUTH_FAILED_KICK_US) {
            plat_wdog_kick();
            kicked = now;
        }

        now = plat_get_us_tick();
    }
}
#endif

static int cmd_auth(void)
{
    int rc = -PB_ERR_NOT_IMPLEMENTED;

#if CONFIG_CM_AUTH_PASSWORD || CONFIG_CM_AUTH_TOKEN
    struct pb_command_authenticate *auth_cmd = (struct pb_command_authenticate *)cmd.request;
    bool refused;
#endif
    /* A method that isn't in the build, or a board without a password
     * check, is refused before the data and changes nothing */
    pb_wire_init_result(&result, -PB_RESULT_NOT_SUPPORTED);

#ifdef CONFIG_CM_AUTH_TOKEN
    if (auth_cmd->method == PB_AUTH_ASYM_TOKEN) {
        rc = auth_read_data(auth_cmd, &refused);

        if (rc == PB_OK) {
            unsigned int arrived = plat_get_us_tick();

            rc = auth_token(auth_cmd->key_id, buffer[0], auth_cmd->size);

            if (rc != PB_OK)
                auth_failed_wait(arrived);
        }

        if (rc == PB_OK)
            authenticated = true;
        else if (!refused)
            authenticated = false;

        pb_wire_init_result(&result, error_to_wire(rc));
    }
#endif

#if CONFIG_CM_AUTH_PASSWORD
    if (auth_cmd->method == PB_AUTH_PASSWORD && cfg->password_auth) {
        rc = auth_read_data(auth_cmd, &refused);

        if (rc == PB_OK) {
            unsigned int arrived = plat_get_us_tick();

            rc = cfg->password_auth((char *)buffer[0], auth_cmd->size);

            if (rc != PB_OK)
                auth_failed_wait(arrived);
        }

        if (rc == PB_OK)
            authenticated = true;
        else if (!refused)
            authenticated = false;

        pb_wire_init_result(&result, error_to_wire(rc));
    }
#endif

    return rc;
}

/* Validates a stream read/write request and returns the buffer and the
 * first block to transfer */
static int
stream_check(uint8_t buffer_id, uint32_t size, uint64_t offset, uint8_t **bfr, lba_t *start_lba)
{
    if (buffer_id >= ARRAY_SIZE(buffer) || size > sizeof(buffer[0])) {
        LOG_ERR("Invalid buffer %u or size %u", buffer_id, size);
        pb_wire_init_result(&result, -PB_RESULT_NO_MEMORY);
        return -PB_ERR_MEM;
    }

    ssize_t block_size = bio_block_size(block_dev);

    if (block_size <= 0) {
        pb_wire_init_result(&result, -PB_RESULT_STREAM_NOT_INITIALIZED);
        return -PB_ERR_PARAM;
    }

    uint64_t lba = offset / (uint64_t)block_size;

    /* The offset is a whole number of blocks, as hosts send it
     * (command-mode.md 5.4) */
    if (offset % (uint64_t)block_size != 0) {
        LOG_ERR("Offset %" PRIu64 " isn't a multiple of the block", offset);
        pb_wire_init_result(&result, -PB_RESULT_INVALID_ARGUMENT);
        return -PB_ERR_PARAM;
    }

    if (lba > UINT32_MAX) {
        LOG_ERR("Invalid offset");
        pb_wire_init_result(&result, -PB_RESULT_INVALID_ARGUMENT);
        return -PB_ERR_PARAM;
    }

    *bfr = buffer[buffer_id];
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
    pb_wire_init_result(&result, -PB_RESULT_STREAM_NOT_INITIALIZED);
    return true;
}

static int cmd_stream_read(void)
{
    int rc = -PB_ERR;
    struct pb_command_stream_read_buffer *stream_read =
        (struct pb_command_stream_read_buffer *)cmd.request;

    LOG_DBG("Stream read %u, %" PRIu64 ", %i",
            stream_read->buffer_id,
            stream_read->offset,
            stream_read->size);

    if (stream_missing())
        return -PB_ERR_STATE;

    if (!(bio_get_flags(block_dev) & BIO_FLAG_READABLE)) {
        LOG_ERR("Partition may not be read");
        pb_wire_init_result(&result, -PB_RESULT_IO_ERROR);
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
        pb_wire_init_result(&result, error_to_wire(rc));
        return rc;
    }

    pb_wire_init_result(&result, PB_RESULT_OK);
    cm_write(&result, sizeof(result));

    rc = cm_write(bfr, stream_read->size);
    pb_wire_init_result(&result, error_to_wire(rc));
    LOG_DBG("Data sent");
    return rc;
}

static int cmd_stream_write(void)
{
    int rc;
    struct pb_command_stream_write_buffer *stream_write =
        (struct pb_command_stream_write_buffer *)cmd.request;

    LOG_DBG("Stream write %u, %" PRIu64 ", %i",
            stream_write->buffer_id,
            stream_write->offset,
            stream_write->size);

    if (stream_missing())
        return -PB_ERR_STATE;

    if (!(bio_get_flags(block_dev) & BIO_FLAG_WRITABLE)) {
        LOG_ERR("Partition may not be written");
        rc = -PB_ERR_IO;
        pb_wire_init_result(&result, error_to_wire(rc));
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

    pb_wire_init_result(&result, error_to_wire(rc));
    return rc;
}

static int cmd_part_verify(void)
{
    int rc;
    int buffer_id = 0;
    size_t chunk_len;
    size_t chunk_max;
    struct pb_command_verify_part *verify_cmd = (struct pb_command_verify_part *)cmd.request;
    size_t bytes_to_verify = verify_cmd->size;
    int lba_offset = 0;

    LOG_DBG("Verify part");

    /* The stream stays on its partition (command-mode.md 5.3) */
    bio_dev_t dev = cm_part_by_uu(verify_cmd->uuid);

    if (dev < 0) {
        LOG_ERR("Could not find partition");
        pb_wire_init_result(&result, error_to_wire(dev));
        return dev;
    }

    /* With bpak the size is at least a header, inside the partition
     * (command-mode.md 5.3): a partition smaller than that takes none */
    if (verify_cmd->bpak && bio_size(dev) < (int64_t)sizeof(struct bpak_header)) {
        LOG_ERR("Partition is smaller than a bpak header");
        pb_wire_init_result(&result, -PB_RESULT_INVALID_ARGUMENT);
        return -PB_ERR_PARAM;
    }

    /* The header is part of the size: less than one would wrap below */
    if (verify_cmd->bpak && verify_cmd->size < sizeof(struct bpak_header)) {
        LOG_ERR("Size %" PRIu32 " is less than a bpak header", verify_cmd->size);
        pb_wire_init_result(&result, -PB_RESULT_INVALID_ARGUMENT);
        return -PB_ERR_PARAM;
    }

    rc = hash_init(HASH_SHA256);

    if (rc != PB_OK) {
        pb_wire_init_result(&result, error_to_wire(rc));
        return rc;
    }

    if (verify_cmd->bpak) {
        LOG_DBG("Bpak header");

        lba_t header_lba =
            bio_get_no_of_blocks(dev) - (sizeof(struct bpak_header) / bio_block_size(dev));

        rc = bio_read(dev, header_lba, sizeof(struct bpak_header), part_buffer[buffer_id]);

        if (rc != PB_OK) {
            pb_wire_init_result(&result, error_to_wire(rc));
            return rc;
        }

        rc = hash_update_async(part_buffer[buffer_id], sizeof(struct bpak_header));

        if (rc != PB_OK) {
            pb_wire_init_result(&result, -PB_RESULT_PART_VERIFY_FAILED);
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
            pb_wire_init_result(&result, -PB_RESULT_PART_VERIFY_FAILED);
            break;
        }

        rc = hash_update_async(part_buffer[buffer_id], chunk_len);

        if (rc != PB_OK) {
            LOG_ERR("Hash update error");
            pb_wire_init_result(&result, error_to_wire(rc));
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

    rc = hash_final(hash, sizeof(hash));

    if (rc != PB_OK) {
        pb_wire_init_result(&result, error_to_wire(rc));
        return rc;
    }

    if (memcmp(hash, verify_cmd->sha256, 32) == 0) {
        rc = PB_OK;
        pb_wire_init_result(&result, error_to_wire(rc));
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
        pb_wire_init_result(&result, -PB_RESULT_PART_VERIFY_FAILED);
    }

    return rc;
}

static int cmd_part_erase(struct pb_command_erase_part *erase_cmd)
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

static int cmd_part_tbl_read(void)
{
    LOG_DBG("TBL read");
    struct pb_result_part_table_read tbl_read_result = { 0 };

    struct pb_result_part_table_entry *result_tbl = (struct pb_result_part_table_entry *)buffer;

    int entries = 0;

    for (bio_dev_t dev = 0; bio_valid(dev); dev++) {
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
    pb_wire_init_result2(&result, PB_RESULT_OK, &tbl_read_result, sizeof(tbl_read_result));

    cm_write(&result, sizeof(result));

    if (entries) {
        size_t bytes = sizeof(struct pb_result_part_table_entry) * (entries);
        cm_write(result_tbl, bytes);
    }

    pb_wire_init_result(&result, PB_RESULT_OK);
    return PB_RESULT_OK;
}

/* Set when the boot has read from the host: the header comes first */
static bool bpak_boot_read_any;

static int bpak_boot_read_f(int64_t offset, size_t length, void *buf)
{
    (void)offset;
    bpak_boot_read_any = true;
    return cm_read(buf, length);
}

static int bpak_boot_result_f(int rc)
{
    LOG_DBG("%i", rc);
    pb_wire_init_result(&result, error_to_wire(rc));
    return cm_write(&result, sizeof(result));
}

static int cmd_boot_bpak(void)
{
    int rc;
    struct pb_command_bpak_boot *bpak_boot_cmd = (struct pb_command_bpak_boot *)&cmd.request;

    boot_clear_set_flags(0, BOOT_FLAG_CMD | (bpak_boot_cmd->verbose ? BOOT_FLAG_VERBOSE : 0));

    pb_wire_init_result(&result, PB_RESULT_OK);
    rc = cm_write(&result, sizeof(result));

    if (rc != PB_OK)
        return rc;

    boot_set_source(BOOT_SOURCE_CB);
    boot_configure_load_cb(bpak_boot_read_f, bpak_boot_result_f);

    bpak_boot_read_any = false;
    rc = boot_load(bpak_boot_cmd->uuid);

    if (rc != PB_OK) {
        /* Refused before the header was read (a precondition, early_boot()'s
         * stop, no handover): the host sends it after the OK all the same,
         * so it is read and its result carries the refusal (command-mode.md
         * 5.5, D-058) */
        if (!bpak_boot_read_any && cm_drain(sizeof(struct bpak_header)) == PB_OK)
            bpak_boot_result_f(rc);
        return rc;
    }

    cm_transport_disconnect();

    return boot_jump();
}

static int cmd_slc_read(void)
{
    int rc;
    struct pb_result_slc slc_status = { 0 };
    struct pb_result_slc_key_status key_status = { 0 };
    slc = slc_read_status();

    if (slc < 0) {
        return slc;
    } else {
        slc_status.slc = slc;
        rc = PB_OK;
    }
    pb_wire_init_result2(&result, error_to_wire(rc), &slc_status, sizeof(slc_status));

    cm_write(&result, sizeof(result));

    if (rc < 0)
        return rc;

    /* The wire has 16 slots for each list (command-mode.md 5.2): a keystore
     * with more keys doesn't write past them */
    size_t no_of_keys = rot_no_of_keys();

    if (no_of_keys > ARRAY_SIZE(key_status.active)) {
        LOG_ERR("The keystore has more keys than the %zu that are listed",
                ARRAY_SIZE(key_status.active));
        no_of_keys = ARRAY_SIZE(key_status.active);
    }

    for (size_t i = 0; i < no_of_keys; i++) {
        int key_rc = rot_read_key_status_by_idx(i);
        key_id_t key_id = rot_key_idx_to_id(i);

        if (key_rc == PB_OK)
            key_status.active[i] = key_id;
        else if (key_rc == -PB_ERR_KEY_REVOKED)
            key_status.revoked[i] = key_id;
        else if (rc == PB_OK)
            rc = key_rc;
        LOG_INFO("Key 0x%x status=%i", key_id, key_rc);
    }

    cm_write(&key_status, sizeof(key_status));

    /* A revoked key is in the key status, the result is only for errors
     * reading it. It used to be the last key's status (F-072). */
    return rc;
}

static int cmd_log_read(void)
{
    struct pb_command_log_read *log_cmd = (struct pb_command_log_read *)cmd.request;
    struct pb_result_log_read log_result = { 0 };
    uint32_t id = (log_cmd->log != 0) ? log_cmd->log : PB_LOG_ID_PB;
    const struct log_source *src = log_source_get(id);
    uint32_t head, oldest, position;
    int rc;

    LOG_DBG("Log %x read %" PRIu32 ", %" PRIu32, id, log_cmd->position, log_cmd->size);

    if (src == NULL) {
        /* Without CONFIG_RAMLOG pb's log answers as it did before there were other logs */
        rc = (id == PB_LOG_ID_PB) ? -PB_ERR_NOT_SUPPORTED : -PB_ERR_NOT_FOUND;
        pb_wire_init_result(&result, error_to_wire(rc));
        return rc;
    }

    /* Nothing may print between here and the read: a new line in a full
     * RAM log moves the oldest byte */
    head = src->head();
    oldest = head - src->length();
    position = MAX(log_cmd->position, oldest);
    position = MIN(position, head);

    log_result.position = position;
    log_result.head = head;
    log_result.log = id;
    log_result.size =
        src->read(position - oldest, buffer[0], MIN(log_cmd->size, (uint32_t)CM_BUF_SIZE));

    pb_wire_init_result2(&result, PB_RESULT_OK, &log_result, sizeof(log_result));

    /* With no data the result is the only one, pb_command_parse sends it */
    if (log_result.size == 0)
        return PB_OK;

    cm_write(&result, sizeof(result));
    rc = cm_write(buffer[0], log_result.size);
    pb_wire_init_result(&result, error_to_wire(rc));
    return rc;
}

static int cmd_stream_init(void)
{
    struct pb_command_stream_initialize *stream_init =
        (struct pb_command_stream_initialize *)cmd.request;

    block_dev = cm_part_by_uu(stream_init->part_uuid);
    stream_written = false;
    stream_failed = false;

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
    struct pb_command_stream_prepare_buffer *stream_prep =
        (struct pb_command_stream_prepare_buffer *)cmd.request;

    LOG_DBG("Stream prep %u, %i", stream_prep->size, stream_prep->id);

    /* Hosts send the data whatever the first result says: it is read and
     * dropped, and the second result has the refusal (command-mode.md 4.4) */
    if (stream_prep->size > CM_BUF_SIZE || stream_prep->id > 1) {
        pb_wire_init_result(&result, -PB_RESULT_NO_MEMORY);
        cm_write(&result, sizeof(result));
        (void)cm_drain(stream_prep->size);
        pb_wire_init_result(&result, -PB_RESULT_NO_MEMORY);
        return -PB_ERR_MEM;
    }

    pb_wire_init_result(&result, PB_RESULT_OK);
    cm_write(&result, sizeof(result));

    uint8_t *bfr = ((uint8_t *)buffer) + (CM_BUF_SIZE * stream_prep->id);

    return cm_read(bfr, stream_prep->size);
}

static int cmd_stream_final(void)
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

    pb_wire_init_result(&result, error_to_wire(rc));
    return rc;
}

/*
 * A locked device requires authentication, and so does one whose life cycle
 * is unknown: the SLC was never set up (the board init failed before
 * slc_init()) or couldn't be read. Only a board without SLC support
 * (-PB_ERR_NOT_SUPPORTED) and the states before locking are open, and EOL
 * (D-011).
 */
static bool slc_requires_auth(void)
{
    if (slc == PB_SLC_CONFIGURATION_LOCKED)
        return true;

    return (slc < 0) && (slc != -PB_ERR_NOT_SUPPORTED);
}

/*
 * The gate (command-mode.md 3.2), before anything looks at the frame. A
 * closed device runs two commands before authentication and answers every
 * other frame with NOT_AUTHENTICATED: also a number pb doesn't define and a
 * frame with a wrong magic, so that it doesn't tell which commands it has
 * (D-058).
 */
static bool cm_gate_allows(void)
{
    if (authenticated || !slc_requires_auth())
        return true;

    return (cmd.magic == PB_WIRE_MAGIC) && !pb_wire_requires_auth(&cmd);
}

/*
 * The gate's refusal. Hosts send BOARD_COMMAND's request and
 * STREAM_PREPARE_BUFFER's data whatever the first result says, so the device
 * sends the first result, reads the data and leaves the second to the caller,
 * both NOT_AUTHENTICATED (command-mode.md 3.2, 4.4). Every other refused
 * frame is answered once.
 */
static void cm_gate_refuse(void)
{
    const struct pb_command_board *board_cmd = (const struct pb_command_board *)cmd.request;
    const struct pb_command_stream_prepare_buffer *stream_prep =
        (const struct pb_command_stream_prepare_buffer *)cmd.request;

    pb_wire_init_result(&result, -PB_RESULT_NOT_AUTHENTICATED);

    if (cmd.magic != PB_WIRE_MAGIC)
        return;

    if (cmd.command == PB_CMD_BOARD_COMMAND) {
        cm_write(&result, sizeof(result));

        if (board_cmd->request_size)
            (void)cm_drain(CM_DATA_XFER_SIZE);

        pb_wire_init_result(&result, -PB_RESULT_NOT_AUTHENTICATED);
    } else if (cmd.command == PB_CMD_STREAM_PREPARE_BUFFER) {
        cm_write(&result, sizeof(result));
        (void)cm_drain(stream_prep->size);
        pb_wire_init_result(&result, -PB_RESULT_NOT_AUTHENTICATED);
    }
}

static int pb_command_parse(void)
{
    int rc = PB_OK;

    if (!cm_gate_allows()) {
        LOG_ERR("Not authenticated");
        cm_gate_refuse();
        goto err_out;
    }

    if (!pb_wire_valid_command(&cmd)) {
        LOG_ERR("Invalid command: %i\n", cmd.command);
        pb_wire_init_result(&result, -PB_RESULT_INVALID_COMMAND);
        goto err_out;
    }

    LOG_DBG("%i", cmd.command);

    pb_wire_init_result(&result, -PB_RESULT_NOT_SUPPORTED);

    switch (cmd.command) {
    case PB_CMD_BOOTLOADER_VERSION_READ: {
        char version_string[30];

        LOG_INFO("Get version");
        snprintf(version_string, sizeof(version_string), "%s", PB_VERSION);

        pb_wire_init_result2(&result, PB_RESULT_OK, version_string, strlen(version_string));
    } break;
    case PB_CMD_DEVICE_RESET: {
        LOG_INFO("Board reset");
        pb_wire_init_result(&result, PB_RESULT_OK);
        cm_write(&result, sizeof(result));
        return -PB_ERR_ABORT;
    } break;
    case PB_CMD_PART_TBL_READ:
        rc = cmd_part_tbl_read();
        break;
    case PB_CMD_DEVICE_READ_CAPS: {
        LOG_INFO("Read caps");
        struct pb_result_device_caps caps = { 0 };
        caps.stream_no_of_buffers = 2;
        caps.stream_buffer_size = CM_BUF_SIZE;
        caps.chunk_transfer_max_bytes = CM_BUF_SIZE;
        /* pb's timeout for a transfer (command-mode.md 4.6). pb sets no bound
         * on an erase, so a host uses its own (F-375) */
        caps.operation_timeout_ms = CONFIG_CM_TRANSFER_TIMEOUT * 1000;

        pb_wire_init_result2(&result, PB_RESULT_OK, &caps, sizeof(caps));
    } break;
    case PB_CMD_BOOT_BPAK:
        rc = cmd_boot_bpak();
        return rc; /* Should not return, we should not try to send a result */
        break;
    case PB_CMD_PART_TBL_INSTALL: /* Deprecated */
    {
        struct pb_command_install_part_table *install_cmd =
            (struct pb_command_install_part_table *)cmd.request;
        if (cm_part_hidden(install_cmd->uu))
            rc = -PB_ERR_NOT_FOUND;
        else
            rc = bio_install_partition_table(install_cmd->uu, install_cmd->variant);
        pb_wire_init_result(&result, error_to_wire(rc));
    } break;
    case PB_CMD_STREAM_INITIALIZE:
        rc = cmd_stream_init();
        break;
    case PB_CMD_STREAM_PREPARE_BUFFER:
        rc = cmd_stream_prep_buffer();
        if (rc != PB_OK)
            stream_failed = true;
        break;
    case PB_CMD_STREAM_READ_BUFFER:
        rc = cmd_stream_read();
        break;
    case PB_CMD_STREAM_WRITE_BUFFER:
        rc = cmd_stream_write();
        if (rc == PB_OK)
            stream_written = true;
        else
            stream_failed = true;
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
    case PB_CMD_BOOT_PART: {
        struct pb_command_boot_part *boot_cmd = (struct pb_command_boot_part *)cmd.request;

        if (cm_part_hidden(boot_cmd->uuid)) {
            rc = -PB_ERR_NOT_FOUND;
        } else {
            boot_set_source(BOOT_SOURCE_BIO);
            boot_clear_set_flags(0, BOOT_FLAG_CMD | (boot_cmd->verbose ? BOOT_FLAG_VERBOSE : 0));
            rc = boot_load(boot_cmd->uuid);
        }

        pb_wire_init_result(&result, error_to_wire(rc));
        cm_write(&result, sizeof(result));

        if (rc == PB_OK) {
            /* Only disconnect when booting, the task reads the next command
             * from the transport after an error */
            cm_transport_disconnect();

            rc = boot_jump();
            /* Should not return */
        }
        return rc;
    } break;
    case PB_CMD_DEVICE_IDENTIFIER_READ: {
        LOG_INFO("Read identifier");
        /* Its own, zeroed: the reserved bytes are zero, and nothing of the
         * buffers is sent before authentication (command-mode.md 4.3) */
        struct pb_result_device_identifier ident = { 0 };
        memcpy(ident.board_id, cfg->name, strlen(cfg->name));
        memcpy(ident.device_uuid, device_uu, 16);
        pb_wire_init_result2(&result, error_to_wire(rc), &ident, sizeof(ident));
    } break;
    case PB_CMD_PART_ACTIVATE: {
        struct pb_command_activate_part *activate_cmd =
            (struct pb_command_activate_part *)cmd.request;
        if (cm_part_hidden(activate_cmd->uuid))
            rc = -PB_ERR_NOT_FOUND;
        else
            rc = boot_set_boot_partition(activate_cmd->uuid);
        pb_wire_init_result(&result, error_to_wire(rc));
    } break;
    case PB_CMD_AUTHENTICATE:
        rc = cmd_auth();
        break;
    case PB_CMD_AUTH_SET_PASSWORD: {
        pb_wire_init_result(&result, -PB_RESULT_NOT_SUPPORTED);
    } break;
    case PB_CMD_BOARD_COMMAND:
        rc = cmd_board();
        break;
    case PB_CMD_BOARD_STATUS_READ: {
        struct pb_result_board_status status_result = { 0 };

        uint8_t *bfr_response = buffer[0];

        size_t response_size = CM_BUF_SIZE;
        rc = cfg->status(bfr_response, &response_size);

        /* Released hosts stop at an error here: the result is the verdict,
         * without data or a second result (command-mode.md 4.4) */
        if (rc != PB_OK) {
            pb_wire_init_result(&result, error_to_wire(rc));
            break;
        }

        status_result.size = response_size;
        pb_wire_init_result2(&result, error_to_wire(rc), &status_result, sizeof(status_result));

        cm_write(&result, sizeof(result));
        cm_write(bfr_response, response_size);
    } break;
    case PB_CMD_SLC_SET_CONFIGURATION: {
        LOG_DBG("Set configuration");
        rc = slc_set_configuration();
        pb_wire_init_result(&result, error_to_wire(rc));
        slc = slc_read_status();
    } break;
    case PB_CMD_SLC_SET_CONFIGURATION_LOCK: {
        LOG_DBG("Set configuration lock");
        rc = slc_set_configuration_locked();
        pb_wire_init_result(&result, error_to_wire(rc));
        slc = slc_read_status();
    } break;
    case PB_CMD_SLC_SET_EOL: {
        LOG_DBG("Set EOL");
        rc = slc_set_eol();
        pb_wire_init_result(&result, error_to_wire(rc));
        slc = slc_read_status();
    } break;
    case PB_CMD_SLC_REVOKE_KEY: {
        struct pb_command_revoke_key *revoke_cmd = (struct pb_command_revoke_key *)cmd.request;

        LOG_DBG("Revoke key %x", revoke_cmd->key_id);

        rc = rot_revoke_key(revoke_cmd->key_id);
        pb_wire_init_result(&result, error_to_wire(rc));
    } break;
    case PB_CMD_SLC_READ:
        rc = cmd_slc_read();
        pb_wire_init_result(&result, error_to_wire(rc));
        break;
    case PB_CMD_PART_RESIZE: {
        /* Deprecated and no longer supported,
         * Use partition table configration index instead. */
        pb_wire_init_result(&result, -PB_RESULT_NOT_SUPPORTED);
    } break;
    case PB_CMD_BOOT_STATUS: {
        struct pb_result_boot_status boot_result = { 0 };
        boot_get_boot_partition(boot_result.uuid);

        pb_wire_init_result2(&result, error_to_wire(rc), &boot_result, sizeof(boot_result));
        rc = PB_RESULT_OK;
    } break;

    case PB_CMD_PART_ERASE: {
        struct pb_command_erase_part *erase_cmd = (struct pb_command_erase_part *)cmd.request;

        rc = cmd_part_erase(erase_cmd);
        pb_wire_init_result(&result, error_to_wire(rc));
    } break;
    case PB_CMD_LOG_READ:
        rc = cmd_log_read();
        break;
    default: {
        LOG_ERR("Got unknown command: %u", cmd.command);
        pb_wire_init_result(&result, -PB_RESULT_INVALID_COMMAND);
    }
    }

err_out:
    cm_write(&result, sizeof(result));

    if (reboot_requested && (rc == 0)) {
        LOG_INFO("Requesting reboot...");
        reboot_requested = false;
        rc = -PB_ERR_ABORT;
    }

    return rc;
}

int cm_init(const struct cm_config *cfg_)
{
    cfg = cfg_;
    return PB_OK;
}

/* Where the task is (command-mode.md 2) */
enum cm_state {
    CM_SETUP, /* Its first call */
    CM_START, /* No session: the transport is to be started, or restarted */
    CM_CONNECT, /* No session: waiting for the transport (2.2) */
    CM_READ, /* A session: the read of the next command is to be started */
    CM_COMMAND, /* A session: waiting for a command */
};

static enum cm_state state = CM_SETUP;
/* The time the transport has to become ready, from when it was started */
static struct pb_timeout ready_timeout;

/* The set up, on the task's first call (command-mode.md 2.1) */
static int cm_setup(void)
{
    int rc;

    LOG_INFO("Initializing command mode");

    /* The USB device port, first. Without it there is no command mode */
    rc = plat_cm_init();

    if (rc != PB_OK) {
        LOG_ERR("Platform init failed (%i)", rc);
        return rc;
    }

#ifdef CONFIG_BOOT_LINUX_RAMOOPS
    /* Only read through command mode, so not set up on the boot path. It
     * reads the region when the log is read. */
    rc = linux_ramoops_init(&ramoops);
    if (rc != PB_OK)
        LOG_ERR("Linux's ramoops log (%i)", rc);
#endif

    rc = cm_board_init();

    if (rc != PB_OK) {
        LOG_ERR("Board init failed (%i)", rc);
        return rc;
    }

    slc = slc_read_status();
    device_uuid(device_uu);

    return PB_OK;
}

/* Starts the transport, and starts it again after a session: from here the
 * transport has CONFIG_CM_TRANSPORT_READY_TIMEOUT to become ready (2.2) */
static int cm_start(void)
{
    int rc;

    /* A session starts with nothing: the last one ended with the transport
     * that is (re)started here (command-mode.md 3.1) */
    cm_session_end();
    transfer_lost = false;

    rc = cm_transport_init();

    if (rc != PB_OK) {
        LOG_ERR("Transport init failed (%i)", rc);
        return -PB_ERR_IO;
    }

    LOG_INFO("Waiting for transport to become ready...");
    pb_timeout_init_us(&ready_timeout, CONFIG_CM_TRANSPORT_READY_TIMEOUT * 1000000L);

    return PB_OK;
}

/* Leaves command mode (command-mode.md 2.3): the device leaves the bus, and
 * the error ends the loop of the tasks, so that pb resets. A caller that
 * runs the task again all the same finds it at its first call */
static int cm_leave(int rc)
{
    cm_transport_disconnect();
    state = CM_SETUP;
    return rc;
}

/*
 * Command mode's task (command-mode.md 2, board.md 4.6): a step of what
 * was a loop. A call never waits for the host: without a session it polls
 * the transport once, with one it polls the read of the next command once,
 * and it returns PB_OK to be called again, so that the other tasks run and
 * the core kicks the watchdog meanwhile. A command that has come runs to its
 * end inside the call, with its own kicks and its transfers' timeout (4.6).
 * An error ends the loop of the tasks: the set up failed, no host came in
 * time, or a command asked for the reset.
 */
int cm_main_task(void)
{
    int rc;

    for (;;) {
        switch (state) {
        case CM_SETUP:
            rc = cm_setup();

            if (rc != PB_OK)
                return rc;

            state = CM_START;
            break;
        case CM_START:
            rc = cm_start();

            if (rc != PB_OK) {
                state = CM_SETUP;
                return rc;
            }

            state = CM_CONNECT;
            break;
        case CM_CONNECT:
            rc = cm_transport_connect();

            if (pb_timeout_has_expired(&ready_timeout))
                rc = -PB_ERR_TIMEOUT;

            if (rc == -PB_ERR_AGAIN)
                return PB_OK;

            if (rc != PB_OK) {
                LOG_ERR("Transport init error (%i)", rc);
                return cm_leave(rc);
            }

            LOG_INFO("Transport ready");
            state = CM_READ;
            break;
        case CM_READ:
            rc = cm_transport_read(&cmd, sizeof(cmd));

            /* Lost while no transfer ran is the error of the next one that
             * is started (usb.md 4.3): the session is over, the transport
             * is restarted and the device waits for a host (command-mode.md
             * 2.2, 3.1). In the next round, so that the tasks run and the
             * watchdog is kicked also when the start fails again */
            if (rc != PB_OK) {
                LOG_ERR("Read error %i", rc);
                state = CM_START;
                return PB_OK;
            }

            state = CM_COMMAND;
            return PB_OK;
        case CM_COMMAND:
            rc = cm_transport_complete();

            /* No command yet. A timeout is polled on (F-287) */
            if (rc == -PB_ERR_AGAIN || rc == -PB_ERR_TIMEOUT)
                return PB_OK;

            if (rc != PB_OK) {
                LOG_ERR("Read error %i", rc);
                state = CM_START;
                break;
            }

            rc = pb_command_parse();

            if (rc == -PB_ERR_ABORT)
                return cm_leave(rc);

            if (transfer_lost) {
                /* The session is over: the transport is restarted and the
                 * device is back at waiting for a host (command-mode.md
                 * 3.1, 4.4, 4.6) */
                LOG_ERR("Transfer lost (%i), restarting the transport", rc);
                state = CM_START;
                break;
            }

            if (rc != PB_OK)
                LOG_ERR("Command error %i", rc);

            /* Read the next command */
            state = CM_READ;
            break;
        }
    }
}

DEFINE_TASK(cm_main, cm_main_task);
