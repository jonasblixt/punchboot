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

static int part_activate(struct pb_context *ctx, const char *part_uuid)
{
    int rc;
    uuid_t uu;

    if (strcmp(part_uuid, "none") == 0)
        memset(uu, 0, sizeof(uu));
    else
        uuid_parse(part_uuid, uu);

    rc = pb_api_boot_activate(ctx, uu);

    return rc;
}

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
    bool flag_activate = false;
    bool flag_part = false;
    const char *device_uuid = NULL;

    struct option long_options[] =
    {
        {"help",        no_argument,       0,  'h' },
        {"verbose",     no_argument,       0,  'v' },
        {"transport",   required_argument, 0,  't' },
        {"device",      required_argument, 0,  'd' },
        {"load",        required_argument, 0,  'l' },
        {"boot",        required_argument, 0,  'b' },
        {"part",        required_argument, 0,  'p' },
        {"activate",    required_argument, 0,  'a' },
        {"verbose-boot", no_argument,      0,  'W' },
        {0,             0,                 0,   0  }
    };

    while ((opt = getopt_long(argc, argv, "hvt:d:l:a:b:Wp:",
                   long_options, &long_index )) != -1)
    {
        switch (opt)
        {
            case 'h':
                help_boot();
                return 0;
            case 'v':
                pb_inc_verbosity();
            break;
            case 'p':
                flag_part = true;
                part_uuid = (const char *) optarg;
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
                fprintf(stderr, "Unknown option: %c\n", optopt);
                return -1;
            break;
            case 'a':
                flag_activate = true;
                part_uuid = (const char *) optarg;
            break;
            case ':':
                fprintf(stderr, "Missing arg for %c\n", optopt);
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
        fprintf(stderr, "Error: Could not initialize context\n");
        return rc;
    }

    rc = ctx->connect(ctx);

    if (rc != PB_RESULT_OK)
    {
        fprintf(stderr, "Error: Could not connect to device\n");
        goto err_free_ctx_out;
    }

    if (flag_load)
    {
        if (!filename)
        {
            fprintf(stderr, "Error: No filename supplied\n");
            rc = -PB_RESULT_ERROR;
            goto err_free_ctx_out;
        }
        struct stat stat_buffer;

        if (stat(filename, &stat_buffer) != 0)
        {
            fprintf(stderr, "Error: Could not open '%s'\n", filename);
            rc = -PB_RESULT_ERROR;
            goto err_free_ctx_out;
        }

        void *image_buffer = malloc(stat_buffer.st_size + 1);

        if (!image_buffer)
        {
            fprintf(stderr, "Error: could not allocate buffer\n");
            rc = -PB_RESULT_ERROR;
            goto err_free_ctx_out;
        }

        FILE *fp = fopen(filename, "rb");

        if (!fp)
        {
            fprintf(stderr, "Error: Could not open '%s' for reading\n", filename);
            rc = -PB_RESULT_ERROR;
            goto err_free_ctx_out;
        }

        size_t read_bytes = fread(image_buffer, 1, stat_buffer.st_size, fp);

        if (read_bytes != stat_buffer.st_size)
        {
            fprintf(stderr, "Error: Could not read data\n");
            rc = -PB_RESULT_ERROR;
            fclose(fp);
            goto err_free_ctx_out;
        }

        fclose(fp);

        uuid_t uu;
        memset(uu, 0, sizeof(uu));

        if (flag_part) {
            uuid_parse(part_uuid, uu);
        }

        rc = pb_api_boot_ram(ctx, image_buffer, (uint8_t *) &uu,
                                    flag_verbose_boot);
    }
    else if (flag_boot)
    {
        uuid_t uu;
        uuid_parse(part_uuid, uu);

        rc = pb_api_boot_part(ctx, uu, flag_verbose_boot);
    }
    else if (flag_activate)
        rc = part_activate(ctx, part_uuid);
    else
    {
        fprintf(stderr, "Error: Unknown command\n");
        rc = -PB_RESULT_ERROR;
    }

    if (rc != PB_RESULT_OK)
    {
        fprintf(stderr, "Error: Command failed %i (%s)\n", rc,
                        pb_error_string(rc));
    }

err_free_ctx_out:
    pb_api_free_context(ctx);
    return rc;
}
