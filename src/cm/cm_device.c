/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Command mode's device commands: the version, the capabilities, the
 * identifier and the logs (command-mode.md 5.1, 5.7).
 */

#include "cm_internal.h"
#include <inttypes.h>
#include <pb/log_source.h>
#include <stdio.h>
#include <string.h>

int cm_cmd_log_read(void)
{
    struct pb_command_log_read *log_cmd = (struct pb_command_log_read *)cm_cmd.request;
    struct pb_result_log_read log_result = { 0 };
    uint32_t id = (log_cmd->log != 0) ? log_cmd->log : PB_LOG_ID_PB;
    const struct log_source *src = log_source_get(id);
    uint32_t head, oldest, position;
    int rc;

    LOG_DBG("Log %x read %" PRIu32 ", %" PRIu32, id, log_cmd->position, log_cmd->size);

    if (src == NULL) {
        /* Without CONFIG_RAMLOG pb's log answers as it did before there were other logs */
        rc = (id == PB_LOG_ID_PB) ? -PB_ERR_NOT_SUPPORTED : -PB_ERR_NOT_FOUND;
        pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
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
        src->read(position - oldest, cm_buffer[0], MIN(log_cmd->size, (uint32_t)CM_BUF_SIZE));

    pb_wire_init_result2(&cm_result, PB_RESULT_OK, &log_result, sizeof(log_result));

    /* With no data the result is the only one, pb_command_parse sends it */
    if (log_result.size == 0)
        return PB_OK;

    cm_write(&cm_result, sizeof(cm_result));
    rc = cm_write(cm_buffer[0], log_result.size);
    pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
    return rc;
}

/* BOOTLOADER_VERSION_READ: pb's version, in the result */
int cm_cmd_version_read(void)
{
    char version_string[30];

    LOG_INFO("Get version");
    snprintf(version_string, sizeof(version_string), "%s", PB_VERSION);

    pb_wire_init_result2(&cm_result, PB_RESULT_OK, version_string, strlen(version_string));

    return PB_OK;
}

/* DEVICE_READ_CAPS: the stream's buffers and pb's transfer timeout */
int cm_cmd_read_caps(void)
{
    LOG_INFO("Read caps");
    struct pb_result_device_caps caps = { 0 };
    caps.stream_no_of_buffers = 2;
    caps.stream_buffer_size = CM_BUF_SIZE;
    caps.chunk_transfer_max_bytes = CM_BUF_SIZE;
    /* pb's timeout for a transfer (command-mode.md 4.6). pb sets no bound
     * on an erase, so a host uses its own (F-375) */
    caps.operation_timeout_ms = CONFIG_CM_TRANSFER_TIMEOUT * 1000;

    pb_wire_init_result2(&cm_result, PB_RESULT_OK, &caps, sizeof(caps));

    return PB_OK;
}

/* DEVICE_IDENTIFIER_READ: the board's name and the device's UUID */
int cm_cmd_device_identifier_read(void)
{
    int rc = PB_OK;

    LOG_INFO("Read identifier");
    /* Its own, zeroed: the reserved bytes are zero, and nothing of the
     * buffers is sent before authentication (command-mode.md 4.3) */
    struct pb_result_device_identifier ident = { 0 };
    memcpy(ident.board_id, cm_cfg->name, strlen(cm_cfg->name));
    memcpy(ident.device_uuid, cm_device_uu, 16);
    pb_wire_init_result2(&cm_result, cm_error_to_wire(rc), &ident, sizeof(ident));

    return rc;
}
