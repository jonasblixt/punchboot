#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include "uuid/uuid.h"

#include "tool.h"

#define PB_MAX_TOKEN_SIZE 4096

static int auth_token(struct pb_context *ctx,
                      const char *token_file_name,
                      uint32_t key_id)
{
    int rc;
    uint8_t *token_buffer = malloc(PB_MAX_TOKEN_SIZE);

    if (!token_buffer)
    {
        printf("Error: Could not allocate memory\n");
        rc = -PB_RESULT_NO_MEMORY;
        goto err_out;
    }

    FILE *fp = fopen(token_file_name, "rb");

    if (!fp)
    {
        printf("Error: Could not open '%s'\n", token_file_name);
        rc = -PB_RESULT_ERROR;
        goto err_free_out;
    }

    size_t read = fread(token_buffer, 1, PB_MAX_TOKEN_SIZE, fp);

    if (!read)
    {
        printf("Error: Empty file\n");
        rc = -PB_RESULT_ERROR;
        goto err_close_fp;
    }

    rc = pb_api_authenticate_key(ctx, key_id, token_buffer, read);

err_close_fp:
    fclose(fp);
err_free_out:
    free(token_buffer);
err_out:
    return rc;
}

static int auth_password(struct pb_context *ctx, const char *password)
{
    return pb_api_authenticate_password(ctx, (uint8_t *)password,
                                strlen(password));
}

static int auth_set_password(struct pb_context *ctx, const char *password)
{
    return pb_api_auth_set_otp_password(ctx, (uint8_t *)password,
                                strlen(password));
}

int action_auth(int argc, char **argv)
{
    int opt;
    int long_index = 0;
    int rc = 0;
    bool flag_token_auth = false;
    bool flag_password_auth = false;
    bool flag_set_otp_password = false;
    const char *password = NULL;
    const char *transport = NULL;
    const char *device_uuid = NULL;
    const char *token_file_name = NULL;
    uint32_t key_id = 0;
    struct pb_context *ctx = NULL;

    struct option long_options[] =
    {
        {"help",        no_argument,       0,  'h' },
        {"verbose",     no_argument,       0,  'v' },
        {"transport",   required_argument, 0,  't' },
        {"device",      required_argument, 0,  'd' },
        {"token",       required_argument, 0,  'T' },
        {"key-id",      required_argument, 0,  'K' },
        {"password",    required_argument, 0,  'P' },
        {"set-otp-password", required_argument, 0,  'L' },
        {0,             0,                 0,   0  }
    };

    while ((opt = getopt_long(argc, argv, "hvt:d:T:K:P:L:",
                   long_options, &long_index )) != -1)
    {
        switch (opt)
        {
            case 'h':
                help_auth();
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
            case 'T':
                token_file_name = (const char *) optarg;
                flag_token_auth = true;
            break;
            case 'K':
                key_id = strtol(optarg, NULL, 0);
            break;
            case 'L':
                flag_set_otp_password = true;
                password = (const char *) optarg;
            break;
            case 'P':
                flag_password_auth = true;
                password = (const char *) optarg;
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
        help_auth();
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

    if (flag_token_auth)
        rc = auth_token(ctx, token_file_name, key_id);
    else if (flag_password_auth)
        rc = auth_password(ctx, password);
    else if (flag_set_otp_password)
        rc = auth_set_password(ctx, password);
    else
    {
        printf("Error: Unknown auth method\n");
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
