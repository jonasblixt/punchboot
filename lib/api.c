#include <stdlib.h>
#include <string.h>
#include <pb-tools/api.h>
#include <pb-tools/protocol.h>

int pb_api_create_context(struct pb_context **ctxp)
{
    struct pb_context *ctx = malloc(sizeof(struct pb_context));

    if (!ctx)
        return -PB_NO_MEMORY;

    memset(ctx, 0, sizeof(*ctx));

    *ctxp = ctx;
    return PB_OK;
}

int pb_api_free_context(struct pb_context *ctx)
{
    if (ctx->free)
        ctx->free(ctx);

    free(ctx);
    return PB_OK;
}

int pb_api_read_bootloader_version(struct pb_context *ctx, char *version,
                                    size_t size)
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

int pb_api_device_reset(struct pb_context *ctx)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;

    pb_protocol_init_command(&cmd, PB_CMD_DEVICE_RESET);

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_OK)
    {
        return rc;
    }

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_OK)
    {
        return rc;
    }

    if (!pb_protocol_valid_result(&result))
    {
        return -PB_ERR;
    }

    if (result.result_code != PB_RESULT_OK)
    {
        return -PB_ERR;
    }

    return PB_OK;
}

int pb_api_device_read_slc(struct pb_context *ctx, uint32_t *slc)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;

    pb_protocol_init_command(&cmd, PB_CMD_DEVICE_SLC_READ);

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_OK)
    {
        return rc;
    }

    if (!pb_protocol_valid_result(&result))
        return -PB_ERR;

    if (result.result_code != PB_RESULT_OK)
        return -PB_ERR;

    memcpy(slc, result.response, sizeof(uint32_t));

    return PB_OK;
}

int pb_api_device_configure(struct pb_context *ctx, const void *data,
                                  size_t size)
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

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_OK)
        return rc;

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

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_OK)
        return rc;

    if (!pb_protocol_valid_result(&result))
        return -PB_ERR;

    if (result.result_code != PB_RESULT_OK)
        return -PB_ERR;

    return PB_OK;
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

    pb_protocol_init_command(&cmd, PB_CMD_DEVICE_IDENTIFIER_READ);

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_OK)
        return rc;

    if (!pb_protocol_valid_result(&result))
        return -PB_ERR;

    if (result.result_code != PB_RESULT_OK)
        return -PB_ERR;

    memcpy(&id, result.response, sizeof(id));
    
    if (board_id_size < sizeof(id.board_id))
        return -PB_NO_MEMORY;
    if (device_uuid_size < sizeof(id.device_uuid))
        return -PB_NO_MEMORY;

    memcpy(device_uuid, id.device_uuid, sizeof(id.device_uuid));
    memcpy(board_id, id.board_id, sizeof(id.board_id));

    return PB_OK;
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

    auth.method = method;
    auth.size = size;

    pb_protocol_init_command(&cmd, PB_CMD_AUTHENTICATE);

    rc = ctx->command(ctx, &cmd, data, size);

    if (rc != PB_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_OK)
        return rc;

    if (!pb_protocol_valid_result(&result))
        return -PB_ERR;

    if (result.result_code != PB_RESULT_OK)
        return -PB_ERR;

    return PB_OK;
}

int pb_api_authenticate_password(struct pb_context *ctx,
                                    uint8_t *data,
                                    size_t size)
{
    return internal_pb_api_authenticate(ctx, PB_AUTH_PASSWORD,
                                        0, data, size);
}

int pb_api_authenticate_key(struct pb_context *ctx,
                            uint32_t key_id,
                            uint8_t *data,
                            size_t size)
{
    return internal_pb_api_authenticate(ctx, PB_AUTH_ASYM_TOKEN,
                                        key_id, data, size);
}

/*
int pb_api_partition_read(struct pb_context *ctx,
                          struct pb_partition_table_entry *out,
                          size_t size)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;
    struct pb_result_part_table_read part_tbl_info;

    pb_protocol_init_command(&cmd, PB_CMD_PART_TBL_READ);

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_OK)
        return rc;

    if (!pb_protocol_valid_result(&result))
        return -PB_ERR;

    if (result.result_code != PB_RESULT_OK)
        return -PB_ERR;

    memcpy(&part_tbl_info, result.response, sizeof(part_tbl_info));

    if (size < (sizeof(*out) * part_tbl_info.no_of_entries))
        return -PB_ERR;


    rc = ctx->read(ctx, out, sizeof(*out) * part_tbl_info.no_of_entries);

    if (rc != PB_OK)
    {
        memset(&ctx->last_result, 0, sizeof(ctx->last_result));
        return rc;
    }

    if (!pb_protocol_valid_result(&result))
        return -PB_ERR;

    if (result.result_code != PB_RESULT_OK)
        return -PB_ERR;

    return PB_OK;
}*/
