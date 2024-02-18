#include "api.h"
#include <bpak/bpak.h>
#include <errno.h>
#include <pb-tools/compat.h>
#include <pb-tools/wire.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifdef _MSC_VER
#include <windows.h>
#else
#include <unistd.h>
#endif

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

    size_t bytes_to_read =
        (tbl_read_result.no_of_entries * sizeof(struct pb_result_part_table_entry));

    ctx->d(ctx,
           2,
           "%s: %i partitions, %i bytes\n",
           __func__,
           tbl_read_result.no_of_entries,
           bytes_to_read);

    if (tbl_read_result.no_of_entries > (*entries))
        return -PB_RESULT_NO_MEMORY;

    ctx->d(ctx, 2, "%s: reading table %p\n", __func__, out);

    struct pb_result_part_table_entry *tbl = malloc(bytes_to_read + 1);

    if (!tbl_read_result.no_of_entries) {
        goto skip_tbl_read;
    }

    rc = ctx->read(ctx, tbl, bytes_to_read);

    if (rc != PB_RESULT_OK) {
        free(tbl);
        return rc;
    }

skip_tbl_read:

    *entries = tbl_read_result.no_of_entries;

    ctx->d(ctx, 2, "%s: read table\n", __func__);

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK) {
        ctx->d(ctx, 0, "%s: read error (%i)\n", __func__, rc);
        free(tbl);
        return rc;
    }

    if (!pb_wire_valid_result(&result)) {
        ctx->d(ctx, 0, "%s: result error\n", __func__);
        free(tbl);
        return -PB_RESULT_ERROR;
    }

    for (int i = 0; i < tbl_read_result.no_of_entries; i++) {
        memcpy(out[i].uuid, tbl[i].uuid, 16);
        strncpy(out[i].description, tbl[i].description, 36);
        out[i].first_block = tbl[i].first_block;
        out[i].last_block = tbl[i].last_block;
        out[i].flags = tbl[i].flags;
        out[i].block_size = tbl[i].block_size;
    }

    free(tbl);
    ctx->d(ctx,
           2,
           "%s: return %i (%s)\n",
           __func__,
           result.result_code,
           pb_error_string(result.result_code));

    return result.result_code;
}

int pb_api_partition_install_table(struct pb_context *ctx, const uint8_t *uu, uint8_t variant)
{
    int rc;
    struct pb_command cmd;
    struct pb_command_install_part_table install_tbl;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);
    memset(&install_tbl, 0, sizeof(install_tbl));
    memcpy(install_tbl.uu, uu, 16);
    install_tbl.variant = variant;

    pb_wire_init_command2(&cmd, PB_CMD_PART_TBL_INSTALL, &install_tbl, sizeof(install_tbl));

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

    ctx->d(ctx,
           2,
           "%s: return %i (%s)\n",
           __func__,
           result.result_code,
           pb_error_string(result.result_code));
    return result.result_code;
}

int pb_api_partition_read_bpak(struct pb_context *ctx, uint8_t *uuid, struct bpak_header *header)
{
    int rc;
    struct pb_command cmd;
    struct pb_command_read_bpak read_command;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    memset(&read_command, 0, sizeof(read_command));
    memcpy(read_command.uuid, uuid, 16);

    pb_wire_init_command2(&cmd, PB_CMD_PART_BPAK_READ, &read_command, sizeof(read_command));

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

    ctx->d(ctx,
           2,
           "%s: return %i (%s)\n",
           __func__,
           result.result_code,
           pb_error_string(result.result_code));
    return result.result_code;
}

int pb_api_partition_erase(struct pb_context *ctx,
                           uint8_t *uuid,
                           uint32_t start_lba,
                           uint32_t block_count)
{
    int rc;
    struct pb_command cmd;
    struct pb_command_erase_part erase_command;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    memset(&erase_command, 0, sizeof(erase_command));
    memcpy(erase_command.uuid, uuid, 16);
    erase_command.start_lba = start_lba;
    erase_command.block_count = block_count;

    pb_wire_init_command2(&cmd, PB_CMD_PART_ERASE, &erase_command, sizeof(erase_command));

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

static int read_part_table(struct pb_context *ctx,
                           struct pb_partition_table_entry **table,
                           int *entries)
{
    struct pb_partition_table_entry *tbl;
    int read_entries = 256;
    int ret;

    tbl = malloc(sizeof(*tbl) * read_entries);
    if (!tbl) {
        return -PB_RESULT_NO_MEMORY;
    }

    ret = pb_api_partition_read_table(ctx, tbl, &read_entries);
    if (ret != PB_RESULT_OK) {
        free(tbl);
        return ret;
    }

    if (read_entries == 0) {
        free(tbl);
        return -PB_RESULT_ERROR; // TODO: Better error?
    }

    *table = tbl;
    *entries = read_entries;
    return 0;
}

int pb_api_partition_write(struct pb_context *ctx, int file_fd, uint8_t *uuid)
{
    struct pb_partition_table_entry *tbl;
    int tbl_entries;
    struct pb_device_capabilities caps;
    size_t chunk_size = 0;
    uint8_t *chunk_buffer = NULL;
    uint8_t buffer_id = 0;
    struct bpak_header header;
    size_t part_size = 0;
    size_t part_block_size = 0;
    bool part_found = false;
    size_t offset = 0;
    ssize_t read_bytes = 0;
    bool bpak_file = false;
    int rc;

    if (lseek(file_fd, 0, SEEK_SET) == (off_t)-1) {
        return -PB_RESULT_IO_ERROR;
    }

    rc = pb_api_device_read_caps(ctx, &caps);
    if (rc != PB_RESULT_OK) {
        return rc;
    }

    chunk_size = caps.chunk_transfer_max_bytes;

    chunk_buffer = malloc(chunk_size);
    if (!chunk_buffer) {
        return -PB_RESULT_MEM_ERROR;
    }

    rc = read_part_table(ctx, &tbl, &tbl_entries);
    if (rc != PB_RESULT_OK) {
        goto err_free_buf;
    }

    for (int i = 0; i < tbl_entries; i++) {
        if (memcmp(tbl[i].uuid, uuid, 16) == 0) {
            part_block_size = tbl[i].block_size;
            part_size = (tbl[i].last_block - tbl[i].first_block + 1) * part_block_size;
            part_found = true;
            break;
        }
    }
    free(tbl);

    if (!part_found) {
        rc = -PB_RESULT_NOT_FOUND;
        goto err_free_buf;
    }

    rc = pb_api_stream_init(ctx, uuid);
    if (rc != PB_RESULT_OK) {
        goto err_free_buf;
    }

    read_bytes = read(file_fd, &header, sizeof(header));

    if (read_bytes < 0) {
        rc = -PB_RESULT_IO_ERROR;
        goto err_free_buf;
    } else if (read_bytes == sizeof(header) && bpak_valid_header(&header) == BPAK_OK) {
        bpak_file = true;
    } else {
        lseek(file_fd, 0, SEEK_SET);
    }

    if (bpak_file) {
        offset = part_size - sizeof(header);

        rc = pb_api_stream_prepare_buffer(ctx, buffer_id, &header, sizeof(header));

        if (rc != PB_RESULT_OK) {
            goto err_free_buf;
        }

        rc = pb_api_stream_write_buffer(ctx, buffer_id, offset, sizeof(header));

        if (rc != PB_RESULT_OK) {
            goto err_free_buf;
        }

        buffer_id = (buffer_id + 1) % caps.stream_no_of_buffers;
        offset = 0;
    }

    while ((read_bytes = read(file_fd, chunk_buffer, chunk_size)) > 0) {
        rc = pb_api_stream_prepare_buffer(ctx, buffer_id, chunk_buffer, read_bytes);

        if (rc != PB_RESULT_OK) {
            goto err_free_buf;
        }

        rc = pb_api_stream_write_buffer(ctx, buffer_id, offset, read_bytes);

        if (rc != PB_RESULT_OK) {
            goto err_free_buf;
        }

        buffer_id = (buffer_id + 1) % caps.stream_no_of_buffers;
        offset += read_bytes;
    }

    if (read_bytes < 0) {
        rc = -PB_RESULT_IO_ERROR;
        goto err_free_buf;
    }

err_free_buf:
    pb_api_stream_finalize(ctx);
    free(chunk_buffer);
    return rc;
}

int pb_api_partition_read(struct pb_context *ctx, int file_fd, uint8_t *uuid)
{
    struct pb_device_capabilities caps;
    struct pb_partition_table_entry *tbl;
    size_t chunk_size;
    size_t offset = 0;
    int buffer_id = 0;
    unsigned char *buffer;
    int entries = 128;
    bool part_found = false;
    size_t bytes_left;
    int rc = -PB_RESULT_ERROR;

    if (!uuid) {
        return -PB_RESULT_INVALID_ARGUMENT;
    }

    lseek(file_fd, 0, SEEK_SET);

    /* Read device capabilities */
    rc = pb_api_device_read_caps(ctx, &caps);

    if (rc != PB_RESULT_OK)
        return rc;

    chunk_size = caps.chunk_transfer_max_bytes;

    buffer = malloc(chunk_size);
    if (!buffer) {
        return -PB_RESULT_NO_MEMORY;
    }

    tbl = malloc(sizeof(struct pb_partition_table_entry) * entries);
    if (!tbl) {
        rc = -PB_RESULT_NO_MEMORY;
        goto err_free_buf;
    }

    rc = pb_api_partition_read_table(ctx, tbl, &entries);

    if (rc != PB_RESULT_OK)
        goto err_free_tbl;

    if (!entries)
        goto err_free_tbl;

    for (int i = 0; i < entries; i++) {
        if (memcmp(uuid, tbl[i].uuid, 16) == 0) {
            part_found = true;
            bytes_left = tbl[i].block_size * (tbl[i].last_block - tbl[i].first_block + 1);
            break;
        }
    }

    if (!part_found) {
        rc = -PB_RESULT_NOT_FOUND;
        goto err_free_tbl;
    }

    rc = pb_api_stream_init(ctx, uuid);

    if (rc != PB_RESULT_OK) {
        fprintf(stderr, "Error: Stream initialization failed (%i)\n", rc);
        goto err_free_tbl;
    }

    do {
        size_t to_read = bytes_left > chunk_size ? chunk_size : bytes_left;
        rc = pb_api_stream_read_buffer(ctx, buffer_id, offset, to_read, buffer);

        if (rc != PB_RESULT_OK)
            break;

        buffer_id = (buffer_id + 1) % caps.stream_no_of_buffers;

        ssize_t bytes_written = write(file_fd, buffer, to_read);

        if (bytes_written != (ssize_t)to_read) {
            rc = -PB_RESULT_IO_ERROR;
            fprintf(stderr, "Error: Write failed (%i)\n", -errno);
            break;
        }

        offset += to_read;
        bytes_left -= to_read;
    } while (bytes_left > 0);

    pb_api_stream_finalize(ctx);

err_free_tbl:
    free(tbl);
err_free_buf:
    free(buffer);
    return rc;
}
