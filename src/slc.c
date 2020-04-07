#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include "uuid/uuid.h"

#include "tool.h"

static int slc_show(struct pb_context *ctx)
{
    uint8_t slc;
    uint32_t revoked_keys[16];
    uint32_t active_keys[16];
    int rc;

    rc = pb_api_slc_read(ctx, &slc, (uint8_t *) active_keys,
                                    (uint8_t *) revoked_keys);

    if (rc != PB_RESULT_OK)
        return rc;

    printf("SLC status: ");

    if (slc == 0)
        printf("Invalid\n");
    else if (slc == 1)
        printf("Not configured\n");
    else if (slc == 2)
        printf("Configuration\n");
    else if (slc == 3)
        printf("Configuration locked\n");
    else if (slc == 4)
        printf("End of life\n");
    else
        printf("Unknown\n");

    printf("\nKey status:\n\n");
    printf("Active      Revoked\n");
    printf("------      -------\n");

    for (int i = 0; i < 16; i++)
    {
        if (active_keys[i])
            printf("0x%8.8x   ", active_keys[i]);
        else
            printf("             ");

        if (revoked_keys[i])
            printf("0x%8.8x", revoked_keys[i]);
        printf("\n");
    }

};

int action_slc(int argc, char **argv)
{
    int opt;
    int long_index = 0;
    int rc = 0;
    bool flag_show = false;
    bool flag_revoke_key = false;
    bool flag_set_configuration = false;
    bool flag_set_configuration_lock = false;
    bool flag_set_end_of_life = false;
    bool flag_set_keystore_id = false;
    bool flag_force = false;
    const char *transport = NULL;
    const char *device_uuid = NULL;
    uint32_t key_id = 0;
    uint32_t keystore_id = 0;
    struct pb_context *ctx = NULL;

    struct option long_options[] =
    {
        {"help",        no_argument,       0,  'h' },
        {"verbose",     no_argument,       0,  'v' },
        {"transport",   required_argument, 0,  't' },
        {"device",      required_argument, 0,  'd' },
        {"force",       no_argument,       0,  'F' },
        {"set-configuration", no_argument, 0,  'C' },
        {"set-configuration-lock", no_argument, 0,'L'},
        {"set-end-of-life", no_argument,   0,  'E' },
        {"show",        no_argument,       0,  's' },
        {"revoke-key",  required_argument, 0,  'R' },
        {"set-keystore-id", required_argument, 0,  'K' },
        {0,             0,                 0,   0  }
    };

    while ((opt = getopt_long(argc, argv, "hvt:d:CLEFsR:K:",
                   long_options, &long_index )) != -1)
    {
        switch (opt)
        {
            case 'h':
                help_slc();
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
            case 'F':
                flag_force = true;
            break;
            case 'C':
                flag_set_configuration = true;
            break;
            case 'L':
                flag_set_configuration_lock = true;
            break;
            case 'E':
                flag_set_end_of_life = true;
            break;
            case 'R':
                flag_revoke_key = true;
                key_id = strtol(optarg, NULL, 0);
            break;
            case 'K':
                flag_set_keystore_id = true;
                keystore_id = strtol(optarg, NULL, 0);
            break;
            case 's':
                flag_show = true;
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
        help_slc();
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

    char confirm_input[16];

    if (flag_set_configuration ||
        flag_set_configuration_lock ||
        flag_set_end_of_life ||
        flag_revoke_key ||
        flag_set_keystore_id)
    {
        if (!flag_force)
        {
            printf("\n\nWARNING: This is a permanent change, writing fuses " \
                "can not be reverted. This could brick your device.\n"
                "\n\nType 'yes' + <Enter> to proceed: ");

            if (fgets(confirm_input, 5, stdin) != confirm_input)
            {
                rc = -PB_RESULT_ERROR;
                goto err_free_ctx_out;
            }
            if (strncmp(confirm_input, "yes", 3)  != 0)
            {
                printf("Aborted\n");
                rc = -PB_RESULT_ERROR;
                goto err_free_ctx_out;
            }
        }

        if (flag_set_configuration)
            rc = pb_api_slc_set_configuration(ctx, NULL, 0);
        else if (flag_set_configuration_lock)
            rc = pb_api_slc_set_configuration_lock(ctx, NULL, 0);
        else if (flag_set_end_of_life)
            rc = pb_api_slc_set_end_of_life(ctx);
        else if (flag_revoke_key)
            rc = pb_api_slc_revoke_key(ctx, key_id);
        else if (flag_set_keystore_id)
            rc = pb_api_slc_set_keystore_id(ctx, keystore_id);
        else
        {
            rc = -PB_RESULT_ERROR;
            printf("Error: Unknown command\n");
        }
    }
    else
    {
        if (flag_show)
            rc = slc_show(ctx);
        else
        {
            rc = -PB_RESULT_ERROR;
            printf("Error: Unknown command\n");
        }
    }

    if (rc != PB_RESULT_OK)
    {
        printf("Error: Command failed %i (%s)\n", rc, pb_error_string(rc));
    }

err_free_ctx_out:
    pb_api_free_context(ctx);
    return rc;
}
