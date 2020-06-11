#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include "uuid/uuid.h"

#include "tool.h"

static int dev_show(struct pb_context *ctx)
{
    int rc;
    char version[64];
    uuid_t device_uu;
    char device_uu_str[37];
    char board_name[17];

    rc = pb_api_bootloader_version(ctx, version, sizeof(version));

    if (rc == PB_RESULT_OK)
    {
        printf("Bootloader version: %s\n", version);
    }


    rc = pb_api_device_read_identifier(ctx, device_uu, sizeof(device_uu),
                                            board_name, sizeof(board_name));

    if (rc == PB_RESULT_OK)
    {
        uuid_unparse(device_uu, device_uu_str);
        printf("Device UUID:        %s\n", device_uu_str);
        printf("Board name:         %s\n", board_name);
    }

    return rc;
}

static int dev_reset(struct pb_context *ctx)
{
    return pb_api_device_reset(ctx);
}

int action_dev(int argc, char **argv)
{
    int opt;
    int long_index = 0;
    int rc = 0;
    long int wait_timeout_seconds = 0;
    const char *transport = NULL;
    bool flag_show = false;
    bool flag_reset = false;
    bool flag_wait = false;
    struct pb_context *ctx = NULL;
    const char *device_uuid = NULL;

    struct option long_options[] =
    {
        {"help",        no_argument,       0,  'h' },
        {"verbose",     no_argument,       0,  'v' },
        {"transport",   required_argument, 0,  't' },
        {"device",      required_argument, 0,  'd' },
        {"show",        no_argument,       0,  'S' },
        {"reset",       no_argument,       0,  'r' },
        {"wait",        required_argument, 0,  'w' },
        {0,             0,                 0,   0  }
    };

    while ((opt = getopt_long(argc, argv, "hvt:SC:L:ab:rd:w:",
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
            case 'd':
                device_uuid = (const char *) optarg;
            break;
            case 'S':
                flag_show = true;
            break;
            case 'r':
                flag_reset = true;
            break;
            case 'w':
                flag_wait = true;
                wait_timeout_seconds = strtol(optarg, NULL, 0);
            break;
            case '?':
                fprintf(stderr, "Unknown option: %c\n", optopt);
                return -1;
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
        help_dev();
        return 0;
    }

    rc = transport_init_helper(&ctx, transport, device_uuid);

    if (rc != PB_RESULT_OK)
    {
        fprintf(stderr, "Error: Could not initialize context\n");
        return rc;
    }

    if (flag_wait)
    {
        if (pb_get_verbosity() > 1)
        {
            printf("Waiting for device (timeout %li seconds)\n",
                                    wait_timeout_seconds);
        }

        for (int i = 0; i < wait_timeout_seconds; i++)
        {
            rc = ctx->connect(ctx);

            if (rc == PB_RESULT_OK)
            {
                if (pb_get_verbosity())
                {
                    printf("Found device\n");
                }

                rc = 0;
                goto err_free_ctx_out;
            }

            sleep(1);
        }

        fprintf(stderr, "Error: Could not find device, timeout\n");
        rc = -1;
        goto err_free_ctx_out;
    }

    rc = ctx->connect(ctx);

    if (rc != PB_RESULT_OK)
    {
        fprintf(stderr, "Error: Could not connect to device\n");
        goto err_free_ctx_out;
    }

    if (flag_show)
        rc = dev_show(ctx);
    else if (flag_reset)
        rc = dev_reset(ctx);

    if (rc != PB_RESULT_OK)
    {
        fprintf(stderr, "Error: Command failed %i (%s)\n", rc,
                        pb_error_string(rc));
    }

err_free_ctx_out:
    pb_api_free_context(ctx);
    return rc;
}
