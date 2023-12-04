#include "api.h"
#include <pb-tools/wire.h>
#include <stdlib.h>
#include <string.h>

int pb_api_device_reset(struct pb_context *ctx)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_DEVICE_RESET);

    rc = ctx->write(ctx, &cmd, sizeof(cmd));

    if (rc != PB_RESULT_OK) {
        return rc;
    }

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK) {
        return rc;
    }

    if (!pb_wire_valid_result(&result)) {
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

int pb_api_device_read_identifier(struct pb_context *ctx,
                                  uint8_t *device_uuid,
                                  size_t device_uuid_size,
                                  char *board_id,
                                  size_t board_id_size)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;
    struct pb_result_device_identifier id;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_DEVICE_IDENTIFIER_READ);

    rc = ctx->write(ctx, &cmd, sizeof(cmd));

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    memcpy(&id, result.response, sizeof(id));

    if (board_id_size < sizeof(id.board_id))
        return -PB_RESULT_NO_MEMORY;
    if (device_uuid_size < sizeof(id.device_uuid))
        return -PB_RESULT_NO_MEMORY;

    memcpy(device_uuid, id.device_uuid, sizeof(id.device_uuid));
    memcpy(board_id, id.board_id, sizeof(id.board_id));

    ctx->d(ctx,
           2,
           "%s: return %i (%s)\n",
           __func__,
           result.result_code,
           pb_error_string(result.result_code));
    return result.result_code;
}

int pb_api_device_read_caps(struct pb_context *ctx, struct pb_device_capabilities *caps)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;
    struct pb_result_device_caps result_caps;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_DEVICE_READ_CAPS);

    rc = ctx->write(ctx, &cmd, sizeof(cmd));

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    memcpy(&result_caps, result.response, sizeof(result_caps));

    memset(caps, 0, sizeof(*caps));

    caps->stream_no_of_buffers = result_caps.stream_no_of_buffers;
    caps->stream_buffer_size = result_caps.stream_buffer_size;
    caps->operation_timeout_ms = result_caps.operation_timeout_ms;
    caps->part_erase_timeout_ms = result_caps.part_erase_timeout_ms;
    caps->chunk_transfer_max_bytes = result_caps.chunk_transfer_max_bytes;

    ctx->d(ctx,
           2,
           "%s: return %i (%s)\n",
           __func__,
           result.result_code,
           pb_error_string(result.result_code));
    return result.result_code;
}
