#include "api.h"
#include <pb-tools/wire.h>
#include <stdlib.h>
#include <string.h>

static int internal_pb_api_authenticate(struct pb_context *ctx,
                                        enum pb_auth_method method,
                                        uint32_t key_id,
                                        uint8_t *data,
                                        size_t size)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;
    struct pb_command_authenticate auth;
    uint8_t auth_buf[1024];

    ctx->d(ctx, 2, "%s: call, method: %i, size: %i\n", __func__, method, size);

    memset(&auth, 0, sizeof(auth));
    auth.method = method;
    auth.key_id = key_id;
    auth.size = size;

    rc = pb_wire_init_command2(&cmd, PB_CMD_AUTHENTICATE, &auth, sizeof(auth));

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->write(ctx, &cmd, sizeof(cmd));

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    if (result.result_code != PB_RESULT_OK)
        return result.result_code;

    if (size) {
        memset(auth_buf, 0, sizeof(auth_buf));
        memcpy(auth_buf, data, size);

        rc = ctx->write(ctx, auth_buf, sizeof(auth_buf));

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

int pb_api_authenticate_password(struct pb_context *ctx, uint8_t *data, size_t size)
{
    ctx->d(ctx, 2, "%s: call\n", __func__);

    return internal_pb_api_authenticate(ctx, PB_AUTH_PASSWORD, 0, data, size);
}

int pb_api_authenticate_key(struct pb_context *ctx, uint32_t key_id, uint8_t *data, size_t size)
{
    ctx->d(ctx, 2, "%s: call\n", __func__);

    return internal_pb_api_authenticate(ctx, PB_AUTH_ASYM_TOKEN, key_id, data, size);
}

int pb_api_auth_set_password(struct pb_context *ctx, const char *password, size_t size)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    rc = pb_wire_init_command2(&cmd, PB_CMD_AUTH_SET_PASSWORD, (char *)password, size);

    if (rc != PB_RESULT_OK) {
        ctx->d(ctx, 0, "%s: Password too long\n", __func__);
        return -PB_RESULT_ERROR;
    }

    rc = ctx->write(ctx, &cmd, sizeof(cmd));

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    ctx->d(ctx,
           2,
           "%s: return %i (%s)\n",
           __func__,
           result.result_code,
           pb_error_string(result.result_code));

    return result.result_code;
}
