#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "uuid/uuid.h"

#include "tool.h"

int action_boot(int argc, char **argv)
{
    int opt;
    int long_index = 0;
    int rc = 0;
    const char *transport = NULL;
    struct pb_context *ctx = NULL;
    const char *filename = NULL;
    const char *part_uuid = NULL;
    bool flag_load = false;
    bool flag_verbose_boot = false;
    bool flag_boot = false;
    const char *device_uuid = NULL;

    struct option long_options[] =
    {
        {"help",        no_argument,       0,  'h' },
        {"verbose",     no_argument,       0,  'v' },
        {"transport",   required_argument, 0,  't' },
        {"device",      required_argument, 0,  'd' },
        {"load",        required_argument, 0,  'l' },
        {"boot",        required_argument, 0,  'b' },
        {"verbose-boot", no_argument,      0,  'W' },
        {0,             0,                 0,   0  }
    };

    while ((opt = getopt_long(argc, argv, "hvt:l:",
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
            case 'd':
                device_uuid = (const char *) optarg;
            break;
            case 'l':
                flag_load = true;
                filename = (const char *) optarg;
            break;
            case 'b':
                flag_boot = true;
                part_uuid = (const char *) optarg;
            break;
            case 'W':
                flag_verbose_boot = true;
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
        help_boot();
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

    if (flag_load)
    {
        if (!filename)
        {
            printf("Error: No filename supplied\n");
            rc = -PB_RESULT_ERROR;
            goto err_free_ctx_out;
        }
        struct stat stat_buffer;

        if (stat(filename, &stat_buffer) != 0)
        {
            printf("Error: Could not open '%s'\n", filename);
            rc = -PB_RESULT_ERROR;
            goto err_free_ctx_out;
        }

        void *image_buffer = malloc(stat_buffer.st_size + 1);

        if (!image_buffer)
        {
            printf("Error: could not allocate buffer\n");
            rc = -PB_RESULT_ERROR;
            goto err_free_ctx_out;
        }

        FILE *fp = fopen(filename, "rb");

        if (!fp)
        {
            printf("Error: Could not open '%s' for reading\n", filename);
            rc = -PB_RESULT_ERROR;
            goto err_free_ctx_out;
        }

        size_t read_bytes = fread(image_buffer, 1, stat_buffer.st_size, fp);

        if (read_bytes != stat_buffer.st_size)
        {
            printf("Error: Could not read data\n");
            rc = -PB_RESULT_ERROR;
            fclose(fp);
            goto err_free_ctx_out;
        }

        fclose(fp);

        rc = pb_api_boot_ram(ctx, image_buffer, flag_verbose_boot);
    }
    else if (flag_boot)
    {
        uuid_t uu;
        uuid_parse(part_uuid, uu);

        rc = pb_api_boot_part(ctx, uu, flag_verbose_boot);
    }
    else
    {
        printf("Error: Unknown command\n");
        rc = -PB_RESULT_ERROR;
    }

    if (rc != PB_RESULT_OK)
    {
        printf("Error: Command failed %i\n", rc);
    }

err_free_ctx_out:
    pb_api_free_context(ctx);
    return rc;
}
