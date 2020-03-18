#include <stdlib.h>
#include <string.h>
#include <pb-tools/api.h>
#include <pb-tools/wire.h>

static int pb_api_debug_stub(struct pb_context *ctx, int level,
                                    const char *fmt, ...)
{
    return PB_RESULT_OK;
}


int pb_api_create_context(struct pb_context **ctxp, pb_debug_t debug)
{
    struct pb_context *ctx = malloc(sizeof(struct pb_context));

    if (!ctx)
        return -PB_RESULT_NO_MEMORY;

    memset(ctx, 0, sizeof(*ctx));

    if (!debug)
        ctx->d = pb_api_debug_stub;
    else
        ctx->d = debug;

    ctx->d(ctx, 2, "%s: init\n", __func__);

    *ctxp = ctx;
    return PB_RESULT_OK;
}

int pb_api_free_context(struct pb_context *ctx)
{
    ctx->d(ctx, 2, "%s: free\n", __func__);

    if (ctx->free)
        ctx->free(ctx);

    free(ctx);
    return PB_RESULT_OK;
}

int pb_api_read_bootloader_version(struct pb_context *ctx, char *version,
                                    size_t size)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_BOOTLOADER_VERSION_READ);

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_RESULT_OK)
    {
        ctx->d(ctx, 0, "%s: command failed %i (%s)\n",
                    __func__, rc, pb_error_string(rc));
        return rc;
    }

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    rc = pb_wire_valid_result(&result);

    if (rc != PB_RESULT_OK)
        return rc;

    char *version_str = (char *) result.response;

    if (strlen(version_str) > size)
        return -PB_RESULT_NO_MEMORY;

    memset(version, 0, size);
    memcpy(version, result.response, strlen(version_str));

    ctx->d(ctx, 2, "%s: return %i (%s)\n", __func__, result.result_code,
                                        pb_error_string(result.result_code));

    return result.result_code;
}

int pb_api_device_reset(struct pb_context *ctx)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_DEVICE_RESET);

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_RESULT_OK)
    {
        return rc;
    }

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
    {
        return rc;
    }

    if (!pb_wire_valid_result(&result))
    {
        return -PB_RESULT_ERROR;
    }

    ctx->d(ctx, 2, "%s: return %i (%s)\n", __func__, result.result_code,
                                        pb_error_string(result.result_code));
    return result.result_code;
}

int pb_api_device_read_slc(struct pb_context *ctx, uint32_t *slc)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_SLC_READ);

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
    {
        return rc;
    }

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    memcpy(slc, result.response, sizeof(uint32_t));

    ctx->d(ctx, 2, "%s: return %i (%s)\n", __func__, result.result_code,
                                        pb_error_string(result.result_code));
    return result.result_code;
}

int pb_api_device_configure(struct pb_context *ctx, const void *data,
                                  size_t size)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_SLC_SET_CONFIGURATION);

    rc = ctx->command(ctx, &cmd, data, size);

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


int pb_api_device_configuration_lock(struct pb_context *ctx, const void *data,
                                        size_t size)
{

    int rc;
    struct pb_command cmd;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_SLC_SET_CONFIGURATION_LOCK);

    rc = ctx->command(ctx, &cmd, data, size);

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

    rc = ctx->command(ctx, &cmd, NULL, 0);

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

    ctx->d(ctx, 2, "%s: return %i (%s)\n", __func__, result.result_code,
                                        pb_error_string(result.result_code));
    return result.result_code;
}

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

    ctx->d(ctx, 2, "%s: call, method: %i, size: %i\n", __func__, method, size);

    auth.method = method;
    auth.size = size;

    rc = pb_wire_init_command2(&cmd, PB_CMD_AUTHENTICATE, &auth, sizeof(auth));

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->command(ctx, &cmd, data, size);

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

int pb_api_authenticate_password(struct pb_context *ctx,
                                    uint8_t *data,
                                    size_t size)
{
    ctx->d(ctx, 2, "%s: call\n", __func__);

    return internal_pb_api_authenticate(ctx, PB_AUTH_PASSWORD,
                                        0, data, size);
}

int pb_api_authenticate_key(struct pb_context *ctx,
                            uint32_t key_id,
                            uint8_t *data,
                            size_t size)
{
    ctx->d(ctx, 2, "%s: call\n", __func__);

    return internal_pb_api_authenticate(ctx, PB_AUTH_ASYM_TOKEN,
                                        key_id, data, size);
}


int pb_api_device_read_caps(struct pb_context *ctx,
                            uint8_t *stream_no_of_buffers,
                            uint32_t *stream_buffer_alignment,
                            uint32_t *stream_buffer_size,
                            uint32_t *operation_timeout_us)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;
    struct pb_result_device_caps caps;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_DEVICE_READ_CAPS);

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    memcpy(&caps, result.response, sizeof(caps));

    (*stream_no_of_buffers) = caps.stream_no_of_buffers;
    (*stream_buffer_alignment) = caps.stream_buffer_alignment;
    (*stream_buffer_size) = caps.stream_buffer_size;
    (*operation_timeout_us) = caps.operation_timeout_us;

    ctx->d(ctx, 2, "%s: return %i (%s)\n", __func__, result.result_code,
                                        pb_error_string(result.result_code));
    return result.result_code;
}

int pb_api_auth_set_otp_password(struct pb_context *ctx,
                                 const char *password,
                                 size_t size)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    rc = pb_wire_init_command2(&cmd, PB_CMD_AUTH_SET_OTP_PASSWORD,
                                    (char *) password, size);

    if (rc != PB_RESULT_OK)
    {
        ctx->d(ctx, 0, "%s: Password too long\n", __func__);
        return -PB_RESULT_ERROR;
    }

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

int pb_api_bootloader_version(struct pb_context *ctx,
                              char *version,
                              size_t size)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;
    char *p;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_BOOTLOADER_VERSION_READ);

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    p = (char *) result.response;

    if (strlen(p) > size)
        return -PB_RESULT_NO_MEMORY;

    memcpy(version, p, strlen(p));

    ctx->d(ctx, 2, "%s: return %i (%s)\n", __func__, result.result_code,
                                        pb_error_string(result.result_code));

    return result.result_code;
}


