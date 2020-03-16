#include <stdint.h>
#include <string.h>
#include <pb/protocol.h>
#include <pb/command.h>
#include <pb/error.h>

int pb_command_init(struct pb_command_ctx *ctx)
{
    ctx->authenticated = false;

    if (!ctx->send_result)
        return -PB_ERR;

    return PB_OK;
}

int pb_command_free(struct pb_command_ctx *ctx)
{
    ctx->authenticated = false;
    return PB_OK;
}

int pb_command_process(struct pb_command_ctx *ctx, struct pb_command *command)
{
    int rc = PB_OK;
    struct pb_result result;

    memset(&result, 0, sizeof(result));
    pb_protocol_init_result(&result, PB_RESULT_INVALID_COMMAND);

    /* Check for invalid commands */
    if (!pb_protocol_valid_command(command))
    {
        pb_protocol_init_result(&result, PB_RESULT_INVALID_COMMAND);
        goto command_error_out;
    }

    /* Check if command requires authentication */
    if (pb_protocol_requires_auth(command) && (!ctx->authenticated))
    {
        pb_protocol_init_result(&result, PB_RESULT_NOT_AUTHENTICATED);
        goto command_error_out;
    }

    switch (command->command)
    {
        case PB_CMD_DEVICE_RESET:
        {
            if (!ctx->device_reset)
            {
                pb_protocol_init_result(&result, PB_RESULT_NOT_SUPPORTED);
                break;
            }

            rc = ctx->device_reset(ctx);

            if (rc != PB_OK)
                pb_protocol_init_result(&result, PB_RESULT_ERROR);
            else
                pb_protocol_init_result(&result, PB_RESULT_OK);
        }
        break;
        case PB_CMD_DEVICE_UUID_READ:
        {
            if (!ctx->device_read_uuid)
            {
                pb_protocol_init_result(&result, PB_RESULT_NOT_SUPPORTED);
                break;
            }

            rc = ctx->device_read_uuid(ctx, &result);
        }
        break;
        case PB_CMD_DEVICE_SLC_READ:
        {
            if (!ctx->read_slc)
            {
                pb_protocol_init_result(&result, PB_RESULT_NOT_SUPPORTED);
                break;
            }

            rc = ctx->read_slc(ctx, &result);
        }
        break;
        case PB_CMD_AUTHENTICATE:
        {
            if (!ctx->authenticate)
            {
                pb_protocol_init_result(&result, PB_RESULT_NOT_SUPPORTED);
                break;
            }

            rc = ctx->authenticate(ctx, command);

            if (rc == PB_OK)
            {
                ctx->authenticated = true;
                pb_protocol_init_result(&result, PB_RESULT_OK);
            }
            else
            {
                ctx->authenticated = false;
                pb_protocol_init_result(&result,
                                            PB_RESULT_AUTHENTICATION_FAILED);
            }
        }
        break;
        case PB_CMD_DEVICE_CONFIG:
        {
            if (!ctx->device_config)
            {
                pb_protocol_init_result(&result, PB_RESULT_NOT_SUPPORTED);
                break;
            }

            rc = ctx->device_config(ctx, command);

            if (rc != PB_OK)
                pb_protocol_init_result(&result, PB_RESULT_ERROR);
            else
                pb_protocol_init_result(&result, PB_RESULT_OK);
        }
        break;
        case PB_CMD_DEVICE_CONFIG_LOCK:
        {
            if (!ctx->device_config_lock)
            {
                pb_protocol_init_result(&result, PB_RESULT_NOT_SUPPORTED);
                break;
            }

            rc = ctx->device_config_lock(ctx, command);

            if (rc != PB_OK)
                pb_protocol_init_result(&result, PB_RESULT_ERROR);
            else
                pb_protocol_init_result(&result, PB_RESULT_OK);
        }
        break;
        default:
        break;
    }

command_error_out:
    ctx->send_result(ctx, &result);
    return rc;
}
