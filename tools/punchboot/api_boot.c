#include "api.h"
#include <bpak/bpak.h>
#include <pb-tools/wire.h>
#include <stdlib.h>
#include <string.h>

int pb_api_boot_activate(struct pb_context *ctx, uint8_t *uuid)
{
    int rc;
    struct pb_command cmd;
    struct pb_command_activate_part activate;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    memset(&activate, 0, sizeof(activate));

    if (uuid != NULL) {
        memcpy(activate.uuid, uuid, 16);
    }

    pb_wire_init_command2(&cmd, PB_CMD_PART_ACTIVATE, &activate, sizeof(activate));

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

int pb_api_boot_part(struct pb_context *ctx, uint8_t *uuid, bool verbose)
{
    int rc;
    struct pb_command_boot_part boot_part_command;
    struct pb_command cmd;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    memset(&boot_part_command, 0, sizeof(boot_part_command));
    memcpy(boot_part_command.uuid, uuid, 16);

    if (verbose)
        boot_part_command.verbose = 1;
    else
        boot_part_command.verbose = 0;

    pb_wire_init_command2(&cmd, PB_CMD_BOOT_PART, &boot_part_command, sizeof(boot_part_command));
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

int pb_api_boot_bpak(struct pb_context *ctx, const void *bpak_image, uint8_t *uuid, bool verbose)
{
    int rc;
    struct pb_command cmd;
    struct pb_command_bpak_boot boot_cmd;
    struct pb_result result;
    struct pb_device_capabilities caps;
    struct bpak_header *header = (struct bpak_header *)bpak_image;

    rc = pb_api_device_read_caps(ctx, &caps);

    if (rc != PB_RESULT_OK)
        return rc;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    if (bpak_valid_header(header) != BPAK_OK) {
        ctx->d(ctx, 0, "%s: Invalid BPAK header\n", __func__);
        return -PB_RESULT_ERROR;
    }

    memset(&boot_cmd, 0, sizeof(boot_cmd));

    if (verbose)
        boot_cmd.verbose = 1;
    else
        boot_cmd.verbose = 0;

    if (uuid != NULL)
        memcpy(boot_cmd.uuid, uuid, 16);

    pb_wire_init_command2(&cmd, PB_CMD_BOOT_BPAK, &boot_cmd, sizeof(boot_cmd));

    ctx->d(ctx, 1, "%s: Loading image..\n", __func__);

    /* Send command and check result */
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

    /* Write header and check result */
    rc = ctx->write(ctx, header, sizeof(*header));

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    if (result.result_code != PB_RESULT_OK)
        return result.result_code;

    /* Stream image */
    bpak_foreach_part(header, p) {
        if (!p->id)
            break;

        uint8_t *partp = ((uint8_t *)header) + bpak_part_offset(header, p);
        size_t part_size = bpak_part_size(p);

        ctx->d(ctx, 1, "%s: Loading part %x, %li bytes\n", __func__, p->id, part_size);

        size_t bytes_to_transfer = part_size;
        size_t chunk = 0;

        ctx->d(
            ctx, 2, "%s: Transfer chunk size: %i bytes\n", __func__, caps.chunk_transfer_max_bytes);

        while (bytes_to_transfer) {
            chunk = bytes_to_transfer > caps.chunk_transfer_max_bytes
                        ? caps.chunk_transfer_max_bytes
                        : bytes_to_transfer;

            ctx->d(ctx, 2, "%s: writing %li bytes\n", __func__, chunk);
            rc = ctx->write(ctx, partp, chunk);

            if (rc != PB_RESULT_OK) {
                ctx->d(ctx, 0, "%s: write error %i\n", __func__, rc);
                return rc;
            }

            bytes_to_transfer -= chunk;
            partp += chunk;
        }

        rc = ctx->read(ctx, &result, sizeof(result));

        if (rc != PB_RESULT_OK)
            return rc;

        if (!pb_wire_valid_result(&result))
            return -PB_RESULT_ERROR;
    }

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

int pb_api_boot_status(struct pb_context *ctx, uint8_t *uuid, char *status_message, size_t len)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;
    struct pb_result_boot_status *boot_status;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_BOOT_STATUS);

    rc = ctx->write(ctx, &cmd, sizeof(cmd));

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    boot_status = (struct pb_result_boot_status *)result.response;

    if (len > sizeof(boot_status->status))
        memcpy(status_message, boot_status->status, sizeof(boot_status->status));
    else
        memcpy(status_message, boot_status->status, len);

    memcpy(uuid, boot_status->uuid, 16);

    ctx->d(ctx,
           2,
           "%s: return %i (%s)\n",
           __func__,
           result.result_code,
           pb_error_string(result.result_code));

    return result.result_code;
}
