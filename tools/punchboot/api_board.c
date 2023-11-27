#include "api.h"
#include <pb-tools/wire.h>
#include <stdlib.h>
#include <string.h>

int pb_api_board_command(struct pb_context *ctx,
                         uint32_t board_command_id,
                         const void *request,
                         size_t request_size,
                         void *response,
                         size_t *response_size)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;
    struct pb_command_board board_command;
    struct pb_result_board board_result;
    uint8_t request_buffer[1024];

    ctx->d(ctx, 2, "%s: call\n", __func__);

    if (request_size > sizeof(request_buffer))
        return -PB_RESULT_NO_MEMORY;

    memset(&board_command, 0, sizeof(board_command));
    board_command.command = board_command_id;
    board_command.request_size = request_size;
    board_command.response_buffer_size = *response_size;

    pb_wire_init_command2(&cmd, PB_CMD_BOARD_COMMAND, &board_command, sizeof(board_command));

    rc = ctx->write(ctx, &cmd, sizeof(cmd));

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;
    /*
        if (result.result_code != PB_RESULT_OK)
            return result.result_code;
    */
    if (request_size) {
        memset(request_buffer, 0, sizeof(request_buffer));
        memcpy(request_buffer, request, request_size);

        rc = ctx->write(ctx, request_buffer, sizeof(request_buffer));

        if (rc != PB_RESULT_OK)
            return rc;
    }

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    memcpy(&board_result, result.response, sizeof(board_result));

    if (board_result.size > *response_size) {
        return -PB_RESULT_ERROR;
    }

    *response_size = board_result.size;

    if (board_result.size) {
        rc = ctx->read(ctx, response, board_result.size);

        if (rc != PB_RESULT_OK)
            return rc;

        rc = ctx->read(ctx, &result, sizeof(result));

        if (rc != PB_RESULT_OK)
            return rc;

        if (!pb_wire_valid_result(&result))
            return -PB_RESULT_ERROR;
    }

    ctx->d(ctx,
           2,
           "%s: return %i (%s)\n",
           __func__,
           result.result_code,
           pb_error_string(result.result_code));

    return result.result_code;
}

int pb_api_board_status(struct pb_context *ctx, char *status, size_t size)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;
    struct pb_result_board_status status_result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_BOARD_STATUS_READ);

    rc = ctx->write(ctx, &cmd, sizeof(cmd));

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    memcpy(&status_result, result.response, sizeof(status_result));

    if (result.result_code != PB_RESULT_OK)
        return result.result_code;

    if (status_result.size > size) {
        return -PB_RESULT_ERROR;
    }

    rc = ctx->read(ctx, status, status_result.size);

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    if (result.result_code != PB_RESULT_OK)
        return result.result_code;

    ctx->d(ctx,
           2,
           "%s: return %i (%s)\n",
           __func__,
           result.result_code,
           pb_error_string(result.result_code));

    return result.result_code;
}
