#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include "uuid/uuid.h"

#include "tool.h"
#include "crc.h"

static int board_show_status(struct pb_context *ctx)
{
    char *status_buffer = malloc(4096);
    int rc;

    rc = pb_api_board_status(ctx, status_buffer, 4096);

    if (rc == PB_RESULT_OK)
        printf("%s\n", status_buffer);

    free(status_buffer);
    return rc;
}

static int board_command(struct pb_context *ctx, int command,
                                                 const char *args)
{
    int rc;
    char *result_buffer = malloc(4096);
    size_t args_len = 0;

    if (args)
    {
        args_len = strlen(args);
    }

    memset(result_buffer, 0, 4096);

    rc = pb_api_board_command(ctx, command, args, args_len,
                                   result_buffer, 4096);

    if (rc == PB_RESULT_OK)
        printf("%s\n", result_buffer);

    free(result_buffer);
    return rc;
}

int action_board(int argc, char **argv)
{
    int opt;
    int long_index = 0;
    int rc = 0;
    bool flag_show_status = false;
    bool flag_command = false;
    uint32_t command = 0;
    const char *command_args = NULL;
    const char *transport = NULL;
    const char *device_uuid = NULL;
    struct pb_context *ctx = NULL;

    struct option long_options[] =
    {
        {"help",        no_argument,       0,  'h' },
        {"verbose",     no_argument,       0,  'v' },
        {"transport",   required_argument, 0,  't' },
        {"device",      required_argument, 0,  'd' },
        {"command",     required_argument, 0,  'b' },
        {"args",        required_argument, 0,  'A' },
        {"show",        no_argument,       0,  's' },
        {0,             0,                 0,   0  }
    };

    while ((opt = getopt_long(argc, argv, "hvt:d:sb:A:",
                   long_options, &long_index )) != -1)
    {
        switch (opt)
        {
            case 'h':
                help_board();
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
            case 's':
                flag_show_status = true;
            break;
            case 'b':
                flag_command = true;
                command = crc32(0, optarg, strlen(optarg));
            break;
            case 'A':
                command_args = (const char *) optarg;
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
        help_board();
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

    if (flag_show_status)
    {
        rc = board_show_status(ctx);
    }
    else if (flag_command)
    {
        rc = board_command(ctx, command, command_args);
    }
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

