#include <stdlib.h>
#include <string.h>
#include <pb/api.h>
#include <pb/protocol.h>

int pb_create_context(struct pb_context **ctxp)
{
    struct pb_context *ctx = malloc(sizeof(struct pb_context));

    if (!ctx)
        return -PB_NO_MEMORY;

    memset(ctx, 0, sizeof(*ctx));

    *ctxp = ctx;
    return PB_OK;
}

int pb_free_context(struct pb_context *ctx)
{
    if (ctx->free)
        ctx->free(ctx);

    free(ctx);
    return PB_OK;
}

int pb_get_version(struct pb_context *ctx, char *version, size_t size)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;

    pb_protocol_init_command(&cmd, PB_CMD_VERSION_READ);

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_OK)
        return rc;

    rc = pb_protocol_valid_result(&result);

    if (rc != PB_OK)
        return rc;

    if (result.result_code != PB_RESULT_OK)
        return -PB_ERR;

    if (result.size > size)
        return -PB_ERR;

    memset(version, 0, size);
    memcpy(version, result.response, result.size);

    return PB_OK;
}

int pb_device_reset(struct pb_context *ctx)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;

    pb_protocol_init_command(&cmd, PB_CMD_DEVICE_RESET);

    memcpy(&ctx->last_command, &cmd, sizeof(cmd));

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_OK)
    {
        memset(&ctx->last_result, 0, sizeof(ctx->last_result));
        return rc;
    }

    memcpy(&ctx->last_result, &result, sizeof(result));

    if (!pb_protocol_valid_result(&result))
        return -PB_ERR;

    if (result.result_code != PB_RESULT_OK)
        return -PB_ERR;

    return PB_OK;
}

int pb_api_device_read_slc(struct pb_context *ctx, uint32_t *slc)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;

    pb_protocol_init_command(&cmd, PB_CMD_DEVICE_SLC_READ);

    memcpy(&ctx->last_command, &cmd, sizeof(cmd));

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_OK)
    {
        memset(&ctx->last_result, 0, sizeof(ctx->last_result));
        return rc;
    }

    memcpy(&ctx->last_result, &result, sizeof(result));

    if (!pb_protocol_valid_result(&result))
        return -PB_ERR;

    if (result.result_code != PB_RESULT_OK)
        return -PB_ERR;

    memcpy(slc, result.response, sizeof(uint32_t));

    return PB_OK;
}

int pb_api_device_configure(struct pb_context *ctx, const void *data, size_t size)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;

    if (size > PB_COMMAND_REQUEST_MAX_SIZE)
        return -PB_ERR;

    pb_protocol_init_command(&cmd, PB_CMD_DEVICE_CONFIG);

    if (size)
    {
        memcpy(cmd.request, data, size);
        cmd.size = size;
    }

    memcpy(&ctx->last_command, &cmd, sizeof(cmd));

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_OK)
    {
        memset(&ctx->last_result, 0, sizeof(ctx->last_result));
        return rc;
    }

    memcpy(&ctx->last_result, &result, sizeof(result));

    if (!pb_protocol_valid_result(&result))
        return -PB_ERR;

    if (result.result_code != PB_RESULT_OK)
        return -PB_ERR;

    return PB_OK;
}


int pb_api_device_configuration_lock(struct pb_context *ctx, const void *data,
                                        size_t size)
{

    int rc;
    struct pb_command cmd;
    struct pb_result result;

    if (size > PB_COMMAND_REQUEST_MAX_SIZE)
        return -PB_ERR;

    pb_protocol_init_command(&cmd, PB_CMD_DEVICE_CONFIG_LOCK);

    if (size)
    {
        memcpy(cmd.request, data, size);
        cmd.size = size;
    }

    memcpy(&ctx->last_command, &cmd, sizeof(cmd));

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_OK)
    {
        memset(&ctx->last_result, 0, sizeof(ctx->last_result));
        return rc;
    }

    memcpy(&ctx->last_result, &result, sizeof(result));

    if (!pb_protocol_valid_result(&result))
        return -PB_ERR;

    if (result.result_code != PB_RESULT_OK)
        return -PB_ERR;

    return PB_OK;
}

int pb_device_read_uuid(struct pb_context *ctx, 
                            struct pb_result_device_uuid *out)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;

    pb_protocol_init_command(&cmd, PB_CMD_DEVICE_UUID_READ);

    memcpy(&ctx->last_command, &cmd, sizeof(cmd));

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_OK)
    {
        memset(&ctx->last_result, 0, sizeof(ctx->last_result));
        return rc;
    }

    memcpy(&ctx->last_result, &result, sizeof(result));

    if (!pb_protocol_valid_result(&result))
        return -PB_ERR;

    if (result.result_code != PB_RESULT_OK)
        return -PB_ERR;

    memcpy(out, result.response, sizeof(*out));

    return PB_OK;
}

int pb_api_authenticate(struct pb_context *ctx,
                    enum pb_auth_method method,
                    uint8_t *data,
                    size_t size)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;
    struct pb_command_authenticate auth;

    auth.method = method;
    auth.size = size;

    pb_protocol_init_command(&cmd, PB_CMD_AUTHENTICATE);

    memcpy(&cmd.request, &auth, sizeof(auth));
    memcpy(&ctx->last_command, &cmd, sizeof(cmd));

    rc = ctx->command(ctx, &cmd, data, size);

    if (rc != PB_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_OK)
    {
        memset(&ctx->last_result, 0, sizeof(ctx->last_result));
        return rc;
    }

    memcpy(&ctx->last_result, &result, sizeof(result));

    if (!pb_protocol_valid_result(&result))
        return -PB_ERR;

    if (result.result_code != PB_RESULT_OK)
        return -PB_ERR;

    return PB_OK;
}
