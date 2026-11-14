/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Command mode's board commands (command-mode.md 5.6): BOARD_COMMAND, the
 * named commands' list and the board's status.
 */

#include "cm_internal.h"
#include <inttypes.h>
#include <pb/command.h>
#include <string.h>

int cm_cmd_board(void)
{
    int rc;

    struct pb_command_board *board_cmd = (struct pb_command_board *)cm_cmd.request;

    struct pb_result_board board_result;

    LOG_DBG("Board command %x", board_cmd->command);
    memset(&board_result, 0, sizeof(board_result));

    uint8_t *bfr = cm_buffer[0];

    pb_wire_init_result(&cm_result, PB_RESULT_OK);
    cm_write(&cm_result, sizeof(cm_result));

    if (board_cmd->request_size) {
        LOG_DBG("Reading request data");
        rc = cm_read(bfr, CM_DATA_XFER_SIZE);

        if (rc != PB_OK) {
            pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
            return rc;
        }
    }

    /* The request transfer is read also when it's refused here, since 2.x
     * hosts don't check the first result */
    if (board_cmd->request_size > CM_DATA_XFER_SIZE) {
        LOG_ERR("Bad board command request size: %" PRIu32, board_cmd->request_size);
        pb_wire_init_result(&cm_result, cm_error_to_wire(-PB_ERR_PARAM));
        return -PB_ERR_PARAM;
    }

    uint8_t *bfr_response = cm_buffer[1];

    /* The response is at most what the host said it can take and at most a
     * buffer (command-mode.md 5.6): a host leaves a larger one on the link.
     * The board is told, and what it reports beyond that isn't sent. */
    size_t response_max = board_cmd->response_buffer_size;

    if (response_max > CM_BUF_SIZE)
        response_max = CM_BUF_SIZE;

    size_t response_size = response_max;

    /* The named command of that id (command-mode.md 5.6). An id no command
     * has is NOT_SUPPORTED, the answer of every release to one the board
     * didn't have */
    const struct cm_command *named = cm_command_find(board_cmd->command);

    if (named != NULL) {
        rc = named->run(bfr, board_cmd->request_size, bfr_response, &response_size);
    } else {
        LOG_ERR("Unknown command %" PRIx32, board_cmd->command);
        response_size = 0;
        rc = -PB_ERR_NOT_SUPPORTED;
    }

    if (response_size > response_max)
        response_size = response_max;

    board_result.size = response_size;
    pb_wire_init_result2(&cm_result, cm_error_to_wire(rc), &board_result, sizeof(board_result));

    LOG_DBG("Response: %zu bytes", response_size);

    if (response_size) {
        LOG_DBG("Sending response");
        cm_write(&cm_result, sizeof(cm_result));
        cm_write(bfr_response, response_size);
    }

    return rc;
}

/* The named commands' list (command-mode.md 4.4, 5.6): the result with the
 * count, then the entries. The set up checked that they fit a buffer */
int cm_cmd_command_list(void)
{
    struct pb_result_command_list list_result = { 0 };
    struct pb_result_command_list_entry *entries =
        (struct pb_result_command_list_entry *)cm_buffer[0];
    size_t count;
    const struct cm_command *list = cm_commands(&count);

    /* The reserved bytes and the end of each field are zero (4.3) */
    memset(entries, 0, sizeof(*entries) * count);

    for (size_t i = 0; i < count; i++) {
        strncpy(entries[i].name, list[i].name, sizeof(entries[i].name) - 1);
        strncpy(entries[i].help, list[i].help, sizeof(entries[i].help) - 1);
    }

    list_result.count = (uint32_t)count;
    pb_wire_init_result2(&cm_result, PB_RESULT_OK, &list_result, sizeof(list_result));
    cm_write(&cm_result, sizeof(cm_result));

    if (count)
        cm_write(entries, sizeof(*entries) * count);

    pb_wire_init_result(&cm_result, PB_RESULT_OK);
    return PB_OK;
}

/* BOARD_STATUS_READ: the board's status() as text (board.md 4.3) */
int cm_cmd_board_status(void)
{
    int rc = PB_OK;

    struct pb_result_board_status status_result = { 0 };

    uint8_t *bfr_response = cm_buffer[0];

    size_t response_size = CM_BUF_SIZE;
    rc = cm_cfg->status(bfr_response, &response_size);

    /* Released hosts stop at an error here: the result is the verdict,
     * without data or a second result (command-mode.md 4.4) */
    if (rc != PB_OK) {
        pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
        return rc;
    }

    status_result.size = response_size;
    pb_wire_init_result2(&cm_result, cm_error_to_wire(rc), &status_result, sizeof(status_result));

    cm_write(&cm_result, sizeof(cm_result));
    cm_write(bfr_response, response_size);

    return rc;
}
