#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <uuid.h>
#include "tool.h"

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

int action_part(int argc, char **argv)
{
    int opt;
    int long_index = 0;
    int rc = 0;
    const char *transport = NULL;
    struct pb_context *ctx = NULL;
    bool flag_list = false;


    struct option long_options[] =
    {
        {"help",        no_argument,       0,  'h' },
        {"verbose",     no_argument,       0,  'v' },
        {"transport",   required_argument, 0,  't' },
        {"write",       required_argument, 0,  'w' },
        {"show",        optional_argument, 0,  's' },
        {"install",     no_argument,       0,  'i' },
        {"list",        optional_argument, 0,  'l' },
        {0,             0,                 0,   0  }
    };

    while ((opt = getopt_long(argc, argv, "hvt:w:s:il",
                   long_options, &long_index )) != -1)
    {
        switch (opt)
        {
            case 'h':
                help_dev();
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

    rc = transport_init_helper(&ctx, transport);

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

    if (rc != PB_RESULT_OK)
    {
        printf("Error: Command failed %i (%s)\n", rc, pb_error_string(rc));
    }

err_free_ctx_out:
    pb_api_free_context(ctx);
    return rc;
}
