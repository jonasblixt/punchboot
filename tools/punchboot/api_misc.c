#include "api.h"
#include <pb-tools/wire.h>
#include <stdlib.h>
#include <string.h>

int pb_api_bootloader_version(struct pb_context *ctx, char *version, size_t size)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;
    char *p;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_BOOTLOADER_VERSION_READ);

    rc = ctx->write(ctx, &cmd, sizeof(cmd));

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    p = (char *)result.response;

    if ((strlen(p) + 1) > size)
        return -PB_RESULT_NO_MEMORY;

    memset(version, 0, size);
    memcpy(version, p, strlen(p));

    ctx->d(ctx,
           2,
           "%s: return %i (%s)\n",
           __func__,
           result.result_code,
           pb_error_string(result.result_code));

    return result.result_code;
}

int pb_api_log_read(struct pb_context *ctx,
                    uint32_t *position,
                    void *buf,
                    size_t *size,
                    uint32_t *head)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;
    struct pb_command_log_read log_cmd;
    struct pb_result_log_read log_result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    memset(&log_cmd, 0, sizeof(log_cmd));
    log_cmd.position = *position;
    log_cmd.size = (*size > UINT32_MAX) ? UINT32_MAX : (uint32_t)*size;

    pb_wire_init_command2(&cmd, PB_CMD_LOG_READ, &log_cmd, sizeof(log_cmd));

    rc = ctx->write(ctx, &cmd, sizeof(cmd));

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    /* Devices older than the command */
    if (result.result_code == -PB_RESULT_INVALID_COMMAND)
        return -PB_RESULT_NOT_SUPPORTED;

    if (result.result_code != PB_RESULT_OK)
        return result.result_code;

    memcpy(&log_result, result.response, sizeof(log_result));

    if (log_result.size > *size)
        return -PB_RESULT_ERROR;

    if (log_result.size) {
        rc = ctx->read(ctx, buf, log_result.size);

        if (rc != PB_RESULT_OK)
            return rc;

        rc = ctx->read(ctx, &result, sizeof(result));

        if (rc != PB_RESULT_OK)
            return rc;

        if (!pb_wire_valid_result(&result))
            return -PB_RESULT_ERROR;
    }

    *position = log_result.position;
    *size = log_result.size;
    *head = log_result.head;

    ctx->d(ctx,
           2,
           "%s: return %i (%s)\n",
           __func__,
           result.result_code,
           pb_error_string(result.result_code));

    return result.result_code;
}
