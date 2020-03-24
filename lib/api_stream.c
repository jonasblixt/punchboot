#include <stdlib.h>
#include <string.h>
#include <pb-tools/api.h>
#include <pb-tools/wire.h>

int pb_api_stream_init(struct pb_context *ctx, uint8_t *uuid)
{
    int rc;
    struct pb_command_stream_initialize stream_init_command;
    struct pb_device_capabilities caps;
    struct pb_command cmd;
    struct pb_result result;
/*
    rc = pb_api_device_read_caps(ctx, &caps);

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;
*/
    ctx->d(ctx, 2, "%s: call\n", __func__);

    memset(&stream_init_command, 0, sizeof(stream_init_command));
    memcpy(stream_init_command.part_uuid, uuid, 16);

    pb_wire_init_command2(&cmd, PB_CMD_STREAM_INITIALIZE,
                                    &stream_init_command,
                                    sizeof(stream_init_command));

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    ctx->d(ctx, 2, "%s: return %i (%s)\n", __func__, result.result_code,
                                        pb_error_string(result.result_code));

    return result.result_code;
}

int pb_api_stream_prepare_buffer(struct pb_context *ctx,
                                 uint8_t buffer_id,
                                 void *data,
                                 uint32_t size)
{
    int rc;
    struct pb_command_stream_prepare_buffer buffer_command;
    struct pb_command cmd;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    memset(&buffer_command, 0, sizeof(buffer_command));

    buffer_command.id = buffer_id;
    buffer_command.size = size;

    pb_wire_init_command2(&cmd, PB_CMD_STREAM_PREPARE_BUFFER,
                                    &buffer_command,
                                    sizeof(buffer_command));

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    ctx->d(ctx, 2, "%s: writing %i bytes\n", __func__, size);

    rc = ctx->write(ctx, data, size);

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
    {
        ctx->d(ctx, 0, "%s: write failed\n", __func__);
        return rc;
    }

    if (!pb_wire_valid_result(&result))
    {
        ctx->d(ctx, 0, "%s: Invalid result\n", __func__);
        return -PB_RESULT_ERROR;
    }

    ctx->d(ctx, 2, "%s: return %i (%s)\n", __func__, result.result_code,
                                        pb_error_string(result.result_code));

    return result.result_code;
}

int pb_api_stream_write_buffer(struct pb_context *ctx,
                               uint8_t buffer_id,
                               uint64_t offset,
                               uint32_t size)
{
    int rc;
    struct pb_command_stream_write_buffer write_command;
    struct pb_command cmd;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    memset(&write_command, 0, sizeof(write_command));

    write_command.buffer_id = buffer_id;
    write_command.offset = offset;
    write_command.size = size;

    pb_wire_init_command2(&cmd, PB_CMD_STREAM_WRITE_BUFFER,
                                    &write_command,
                                    sizeof(write_command));

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    ctx->d(ctx, 2, "%s: return %i (%s)\n", __func__, result.result_code,
                                        pb_error_string(result.result_code));

    return result.result_code;
}

int pb_api_stream_finalize(struct pb_context *ctx)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_STREAM_FINALIZE);

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    ctx->d(ctx, 2, "%s: return %i (%s)\n", __func__, result.result_code,
                                        pb_error_string(result.result_code));

    return result.result_code;
}
