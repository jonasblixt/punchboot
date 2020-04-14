#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <bpak/bpak.h>
#include <bpak/utils.h>
#include "tool.h"
#include "uuid/uuid.h"
#include "sha256.h"



static int part_verify(struct pb_context *ctx, const char *filename,
                        const char *part_uuid, size_t offset)
{
    struct pb_device_capabilities caps;
    struct bpak_header header;
    bool bpak_file = false;
    uuid_t uu_part;
    uint8_t hash_data[32];
    int rc;
    FILE *fp = fopen(filename, "rb");
    mbedtls_sha256_context sha256;
    size_t file_size = 0;

    mbedtls_sha256_init(&sha256);
    mbedtls_sha256_starts_ret(&sha256, 0);

    if (!fp)
    {
        printf("Error: Could not open '%s'\n", filename);
        return -PB_RESULT_ERROR;
    }

    uuid_parse(part_uuid, uu_part);

    size_t read_bytes = fread(&header, 1, sizeof(header), fp);

    if (read_bytes == sizeof(header) &&
        (bpak_valid_header(&header) == BPAK_OK))
    {
        printf("Detected bpak header\n");
        bpak_file = true;

        rc = mbedtls_sha256_update_ret(&sha256, (char *) &header,
                                                sizeof(header));

        if (rc != 0)
        {
            rc = -PB_RESULT_ERROR;
            goto err_out;
        }

        file_size += sizeof(header);
    }
    else
    {
        fseek(fp, 0, SEEK_SET);
    }

    char *chunk_buffer = malloc(1024*1024);

    if (!chunk_buffer)
    {
        rc = -PB_RESULT_NO_MEMORY;
        goto err_out;
    }

    while ((read_bytes = fread(chunk_buffer, 1, 1024*1024, fp)) > 0)
    {
        rc = mbedtls_sha256_update_ret(&sha256, chunk_buffer, read_bytes);

        if (rc != 0)
        {
            rc = -PB_RESULT_ERROR;
            goto err_free_out;
        }

        file_size += read_bytes;
    }

    mbedtls_sha256_finish_ret(&sha256, hash_data);

    rc = pb_api_partition_verify(ctx, uu_part, hash_data, file_size, bpak_file);

err_free_out:
    free(chunk_buffer);
err_out:
    fclose(fp);
    return rc;
}

static int part_write(struct pb_context *ctx, const char *filename,
                        const char *part_uuid, size_t block_write_offset)
{
    uint8_t *chunk_buffer = NULL;
    struct pb_device_capabilities caps;
    struct bpak_header header;
    uuid_t uu_part;
    int rc;
    int buffer_id = 0;
    size_t chunk_size = 0;
    struct pb_partition_table_entry *tbl;
    int entries = 128;
    FILE *fp = fopen(filename, "rb");

    if (!fp)
    {
        printf("Error: Could not open '%s'\n", filename);
        return -PB_RESULT_ERROR;
    }

    /* Read device capabilities */
    rc = pb_api_device_read_caps(ctx, &caps);

    if (rc != PB_RESULT_OK)
        return rc;

    uuid_parse(part_uuid, uu_part);
    chunk_size = caps.chunk_transfer_max_bytes;

    /* Read partition table */

    tbl = malloc(sizeof(struct pb_partition_table_entry) * entries);

    rc = pb_api_partition_read_table(ctx, tbl, &entries);

    if (rc != PB_RESULT_OK)
        goto err_free_entries;

    if (!entries)
        goto err_free_entries;

    printf("Writing '%s' to '%s'\n", filename, part_uuid);

    chunk_buffer = malloc(chunk_size + 1);

    if (!chunk_buffer)
        return -PB_RESULT_NO_MEMORY;

    rc = pb_api_stream_init(ctx, uu_part);

    if (rc != PB_RESULT_OK)
    {
        printf("Error: Stream initialization failed (%i)\n", rc);
        goto err_out;
    }

    size_t read_bytes = 0;
    size_t offset = 0;
    bool bpak_file = false;

    read_bytes = fread(&header, 1, sizeof(header), fp);

    if (read_bytes == sizeof(header) &&
        (bpak_valid_header(&header) == BPAK_OK))
    {
        printf("Detected bpak header\n");
        bpak_file = true;
    }
    else
    {
        fseek(fp, 0, SEEK_SET);
    }


    if (bpak_file)
    {
        rc = pb_api_stream_prepare_buffer(ctx, buffer_id,
                                            &header, sizeof(header));

        if (rc != PB_RESULT_OK)
        {
            printf("Error: Could not write header");
            goto err_out;
        }

        for (int i = 0; i < entries; i++)
        {
            if (uuid_compare(tbl[i].uuid, uu_part) == 0)
            {
                offset = tbl[i].block_size * \
                         (tbl[i].last_block - tbl[i].first_block + 1) - \
                         sizeof(header);
            }
        }
        printf("Writing header at 0x%lx\n", offset);
        rc = pb_api_stream_write_buffer(ctx, buffer_id, offset, sizeof(header));

        if (rc != PB_RESULT_OK)
        {
            printf("Error: Could not write header");
            goto err_out;
        }

        buffer_id = (buffer_id + 1) % caps.stream_no_of_buffers;
        offset = 0;
    }

    if (bpak_file && block_write_offset)
    {
        printf("Detected bpak header and offset parameter, ignoring offset\n");
    }
    else
    {
        offset += (block_write_offset * 512);
    }

    while ((read_bytes = fread(chunk_buffer, 1, chunk_size, fp)) > 0)
    {
        rc = pb_api_stream_prepare_buffer(ctx, buffer_id, chunk_buffer,
                                                read_bytes);

        if (rc != PB_RESULT_OK)
            break;

        rc = pb_api_stream_write_buffer(ctx, buffer_id, offset,
                                                read_bytes);

        if (rc != PB_RESULT_OK)
            break;

        buffer_id = (buffer_id + 1) % caps.stream_no_of_buffers;
        offset += read_bytes;
    }

    pb_api_stream_finalize(ctx);

err_free_entries:
    free(tbl);
err_out:
    fclose(fp);
    free(chunk_buffer);
    return rc;
}

static int part_list(struct pb_context *ctx)
{
    struct pb_partition_table_entry *tbl;
    char uuid_str[37];
    int entries = 128;
    int rc = -PB_RESULT_ERROR;

    tbl = malloc(sizeof(struct pb_partition_table_entry) * entries);

    rc = pb_api_partition_read_table(ctx, tbl, &entries);

    if (rc != PB_RESULT_OK)
        goto err_out;

    if (!entries)
        goto err_out;

    printf("%-37s   %-8s   %-7s   %-16s\n",
                "Partition UUID",
                "Flags",
                "Size",
                "Name");
    printf("%-37s   %-8s   %-7s   %-16s\n",
                "--------------",
                "-----",
                "----",
                "----");

    for (int i = 0; i < entries; i++)
    {
        size_t part_size = (tbl[i].last_block - tbl[i].first_block + 1) * \
                            tbl[i].block_size;
        char size_str[16];
        char flags_str[9] = "--------";

        uuid_unparse(tbl[i].uuid, uuid_str);
        bytes_to_string(part_size, size_str, sizeof(size_str));
        uint8_t flags = tbl[i].flags;

        if (flags & PB_PART_FLAG_BOOTABLE)
            flags_str[0] = 'B';
        else
            flags_str[0] = '-';

        if (flags & PB_PART_FLAG_OTP)
            flags_str[1] = 'o';
        else
            flags_str[1] = '-';

        if (flags & PB_PART_FLAG_WRITABLE)
            flags_str[2] = 'W';
        else
            flags_str[2] = 'r';

        if (flags & PB_PART_FLAG_ERASE_BEFORE_WRITE)
            flags_str[3] = 'E';
        else
            flags_str[3] = '-';

        printf("%-37s   %-8s   %-7s   %-16s\n", uuid_str, flags_str, size_str,
                                        tbl[i].description);
    }

err_out:
    free(tbl);
    return rc;
}

static int print_bpak_header(struct bpak_header *h,
                             char *part_uuid_str,
                             char *part_description)
{
    printf("Partition: %s (%s)\n", part_uuid_str, part_description);
    printf("Hash:      %s\n", bpak_hash_kind(h->hash_kind));
    printf("Signature: %s\n", bpak_signature_kind(h->signature_kind));

    printf("\nMetadata:\n");
    printf("    ID         Size   Meta ID              Part Ref   Data\n");

    char string_output[128];

    bpak_foreach_meta(h, m)
    {
        if (m->id)
        {
            bpak_meta_to_string(h, m, string_output, sizeof(string_output));
            printf("    %8.8x   %-3u    %-20s ", m->id, m->size,
                         bpak_known_id(m->id));

            if (m->part_id_ref)
                printf("%8.8x", m->part_id_ref);
            else
                printf("        ");
            printf("   %s", string_output);
            printf("\n");
        }
    }

    printf("\nParts:\n");
    printf("    ID         Size         Z-pad  Flags          Transport Size\n");

    char flags_str[9] = "--------";

    bpak_foreach_part(h, p)
    {
        if (p->id)
        {
            if (p->flags & BPAK_FLAG_EXCLUDE_FROM_HASH)
                flags_str[0] = 'h';
            else
                flags_str[0] = '-';

            if (p->flags & BPAK_FLAG_TRANSPORT)
                flags_str[1] = 'T';
            else
                flags_str[1] = '-';

            printf("    %8.8x   %-12lu %-3u    %s",p->id, p->size, p->pad_bytes,
                                                flags_str);

            if (p->flags & BPAK_FLAG_TRANSPORT)
                printf("       %-12lu", p->transport_size);
            else
                printf("       %-12lu", p->size);

            printf("\n");
        }
    }

    printf("\n\n");
}

static int part_show(struct pb_context *ctx, const char *part_uuid)
{
    struct pb_partition_table_entry *tbl;
    int entries = 128;
    int rc = -PB_RESULT_ERROR;
    char uuid_str[37];

    tbl = malloc(sizeof(struct pb_partition_table_entry) * entries);

    rc = pb_api_partition_read_table(ctx, tbl, &entries);

    if (rc != PB_RESULT_OK)
        goto err_out;

    if (!entries)
        goto err_out;

    for (int i = 0; i < entries; i++)
    {
        struct bpak_header header;
        uuid_unparse(tbl[i].uuid, uuid_str);

        if (part_uuid)
            if (strcmp(uuid_str, part_uuid) != 0)
                continue;

        rc = pb_api_partition_read_bpak(ctx, tbl[i].uuid, &header);

        if (rc == PB_RESULT_OK)
            print_bpak_header(&header, uuid_str, tbl[i].description);
    }

    rc = PB_RESULT_OK;

err_out:
    free(tbl);
    return rc;
}

int action_part(int argc, char **argv)
{
    int opt;
    int long_index = 0;
    int rc = 0;
    const char *transport = NULL;
    const char *device_uuid = NULL;
    struct pb_context *ctx = NULL;
    bool flag_list = false;
    bool flag_install = false;
    bool flag_write = false;
    bool flag_verify = false;
    bool flag_show = false;
    size_t block_write_offset = 0;
    const char *part_uuid = NULL;
    const char *filename = NULL;

    struct option long_options[] =
    {
        {"help",        no_argument,       0,  'h' },
        {"verbose",     no_argument,       0,  'v' },
        {"transport",   required_argument, 0,  't' },
        {"device",      required_argument, 0,  'd' },
        {"write",       required_argument, 0,  'w' },
        {"verify",      required_argument, 0,  'c' },
        {"show",        no_argument,       0,  's' },
        {"part",        required_argument, 0,  'p' },
        {"install",     no_argument,       0,  'i' },
        {"list",        no_argument,       0,  'l' },
        {"offset",      required_argument, 0,  'O' },
        {0,             0,                 0,   0  }
    };

    while ((opt = getopt_long(argc, argv, "hvt:w:silp:c:d:",
                   long_options, &long_index )) != -1)
    {
        switch (opt)
        {
            case 'h':
                help_part();
                return 0;
            case 'v':
                pb_inc_verbosity();
            break;
            case 't':
                transport = (const char *) optarg;
            break;
            case 'l':
                flag_list = true;
            break;
            case 'O':
                block_write_offset = strtol(optarg, NULL, 0);
            break;
            case 'i':
                flag_install = true;
            break;
            case 'w':
                flag_write = true;
                filename = (const char *) optarg;
            break;
            case 'c':
                flag_verify = true;
                filename = (const char *) optarg;
            break;
            case 'p':
                part_uuid = (const char *) optarg;
            break;
            case 's':
                flag_show = true;
            break;
            case 'd':
                device_uuid = (const char *) optarg;
            break;
            case '?':
                printf("Unknown option: %c\n", optopt);
                return -1;
            break;
            case ':':
                printf("Missing arg for %c\n", optopt);
                return -1;
            break;
            default:
               return -1;
        }
    }

    if (argc <= 1)
    {
        help_part();
        return 0;
    }

    rc = transport_init_helper(&ctx, transport, device_uuid);

    if (rc != PB_RESULT_OK)
    {
        printf("Error: Could not initialize context\n");
        return rc;
    }

    rc = ctx->connect(ctx);

    if (rc != PB_RESULT_OK)
    {
        printf("Error: Could not connect to device\n");
        goto err_free_ctx_out;
    }

    if (flag_list)
        rc = part_list(ctx);
    else if (flag_install)
        rc = pb_api_partition_install_table(ctx);
    else if (flag_write)
        rc = part_write(ctx, filename, part_uuid, block_write_offset);
    else if (flag_verify)
        rc = part_verify(ctx, filename, part_uuid, block_write_offset);
    else if (flag_show)
        rc = part_show(ctx, part_uuid);

    if (rc != PB_RESULT_OK)
    {
        printf("Error: Command failed %i (%s)\n", rc, pb_error_string(rc));
    }

err_free_ctx_out:
    pb_api_free_context(ctx);
    return rc;
}