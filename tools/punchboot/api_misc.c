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
