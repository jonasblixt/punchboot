#include <stdlib.h>
#include <string.h>
#include <pb-tools/api.h>
#include <pb-tools/wire.h>
#include <bpak/bpak.h>

int pb_api_partition_read_table(struct pb_context *ctx,
                                struct pb_partition_table_entry *out,
                                int *entries)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;
    struct pb_result_part_table_read tbl_read_result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_PART_TBL_READ);

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

    memcpy(&tbl_read_result, result.response, sizeof(tbl_read_result));

    size_t bytes_to_read = (tbl_read_result.no_of_entries * \
            sizeof(struct pb_result_part_table_entry));

    ctx->d(ctx, 2, "%s: %i partitions, %i bytes\n", __func__,
                tbl_read_result.no_of_entries, bytes_to_read);

    if (tbl_read_result.no_of_entries > (*entries))
        return -PB_RESULT_NO_MEMORY;

    ctx->d(ctx, 2, "%s: reading table %p\n", __func__, out);

    struct pb_result_part_table_entry *tbl = malloc(bytes_to_read+1);

    rc = ctx->read(ctx, tbl, bytes_to_read);

    if (rc != PB_RESULT_OK)
    {
        free(tbl);
        return rc;
    }

    *entries = tbl_read_result.no_of_entries;

    ctx->d(ctx, 2, "%s: read table\n", __func__);

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
    {
        ctx->d(ctx, 0, "%s: read error (%i)\n", __func__, rc);
        free(tbl);
        return rc;
    }

    if (!pb_wire_valid_result(&result))
    {
        ctx->d(ctx, 0, "%s: result error\n", __func__);
        free(tbl);
        return -PB_RESULT_ERROR;
    }

    for (int i = 0; i < tbl_read_result.no_of_entries; i++)
    {
        memcpy(out[i].uuid, tbl[i].uuid, 16);
        strncpy(out[i].description, tbl[i].description, 36);
        out[i].first_block = tbl[i].first_block;
        out[i].last_block = tbl[i].last_block;
        out[i].flags = tbl[i].flags;
        out[i].block_size = tbl[i].block_size;
    }

    free(tbl);
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

    rc = ctx->write(ctx, &cmd, sizeof(cmd));

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
                            uint8_t *sha256,
                            uint32_t size,
                            bool bpak)
{
    int rc;
    struct pb_command cmd;
    struct pb_command_verify_part verify;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    memset(&verify, 0, sizeof(verify));
    memcpy(verify.uuid, uuid, 16);
    memcpy(verify.sha256, sha256, 32);
    verify.size = size;

    if (bpak)
        verify.bpak = 1;
    else
        verify.bpak = 0;

    pb_wire_init_command2(&cmd, PB_CMD_PART_VERIFY, &verify, sizeof(verify));

    rc = ctx->write(ctx, &cmd, sizeof(cmd));

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

    rc = ctx->read(ctx, header, sizeof(*header));

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

int pb_api_partition_erase(struct pb_context *ctx, uint8_t *uuid)
{
    int rc;
    struct pb_command cmd;
    struct pb_command_erase_part erase_command;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    memset(&erase_command, 0, sizeof(erase_command));
    memcpy(erase_command.uuid, uuid, 16);

    pb_wire_init_command2(&cmd, PB_CMD_PART_ERASE, &erase_command,
                                    sizeof(erase_command));

    rc = ctx->write(ctx, &cmd, sizeof(cmd));

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

int pb_api_partition_resize(struct pb_context *ctx, uint8_t *uuid,
                                size_t blocks)
{
    int rc;
    struct pb_command cmd;
    struct pb_command_resize_part resize_command;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    memset(&resize_command, 0, sizeof(resize_command));
    memcpy(resize_command.uuid, uuid, 16);
    resize_command.blocks = blocks;

    pb_wire_init_command2(&cmd, PB_CMD_PART_RESIZE, &resize_command,
                                    sizeof(resize_command));

    rc = ctx->write(ctx, &cmd, sizeof(cmd));

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
