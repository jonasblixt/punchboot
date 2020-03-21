#include <stdlib.h>
#include <string.h>
#include <pb-tools/api.h>
#include <pb-tools/wire.h>
#include <bpak/bpak.h>

int pb_api_partition_read_table(struct pb_context *ctx, void *out, size_t size)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;
    struct pb_result_part_table_read tbl_read_result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_PART_TBL_READ);

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    memcpy(&tbl_read_result, result.response, sizeof(tbl_read_result));

    size_t bytes_to_read = (tbl_read_result.no_of_entries * \
            sizeof(struct pb_result_part_table_entry));

    ctx->d(ctx, 2, "%s: %i partitions, %i bytes\n", __func__,
                tbl_read_result.no_of_entries, bytes_to_read);

    if (bytes_to_read > size)
        return -PB_RESULT_NO_MEMORY;

    ctx->d(ctx, 2, "%s: reading table %p\n", __func__, out);

    rc = ctx->read(ctx, out, bytes_to_read);

    if (rc != PB_RESULT_OK)
        return rc;

    ctx->d(ctx, 2, "%s: read table\n", __func__);

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
    {

        ctx->d(ctx, 0, "%s: read error (%i)\n", __func__, rc);
        return rc;
    }

    if (!pb_wire_valid_result(&result))
    {
        ctx->d(ctx, 0, "%s: result error\n", __func__);
        return -PB_RESULT_ERROR;
    }

    ctx->d(ctx, 2, "%s: return %i (%s)\n", __func__, result.result_code,
                                        pb_error_string(result.result_code));

    return result.result_code;
};


int pb_api_partition_install_table(struct pb_context *ctx)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_PART_TBL_INSTALL);

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

int pb_api_partition_verify(struct pb_context *ctx,
                            uint8_t *uuid,
                            uint8_t *sha256)
{
    int rc;
    struct pb_command cmd;
    struct pb_command_verify_part verify;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    memset(&verify, 0, sizeof(verify));
    memcpy(verify.uuid, uuid, 16);
    memcpy(verify.sha256, sha256, 32);

    pb_wire_init_command2(&cmd, PB_CMD_PART_VERIFY, &verify, sizeof(verify));

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

int pb_api_partition_activate(struct pb_context *ctx,
                              uint8_t *uuid)
{
    int rc;
    struct pb_command cmd;
    struct pb_command_activate_part activate;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    memset(&activate, 0, sizeof(activate));
    memcpy(activate.uuid, uuid, 16);

    pb_wire_init_command2(&cmd, PB_CMD_PART_ACTIVATE, &activate,
                                    sizeof(activate));

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

int pb_api_partition_read_bpak(struct pb_context *ctx,
                              uint8_t *uuid,
                              struct bpak_header *header)
{
    int rc;
    struct pb_command cmd;
    struct pb_command_read_bpak read_command;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    memset(&read_command, 0, sizeof(read_command));
    memcpy(read_command.uuid, uuid, 16);

    pb_wire_init_command2(&cmd, PB_CMD_PART_BPAK_READ, &read_command,
                                    sizeof(read_command));

    rc = ctx->command(ctx, &cmd, NULL, 0);

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    rc = ctx->read(ctx, header, sizeof(*header));

    if (rc != PB_RESULT_OK)
        return rc;

    ctx->d(ctx, 2, "%s: return %i (%s)\n", __func__, result.result_code,
                                        pb_error_string(result.result_code));
    return result.result_code;
}
