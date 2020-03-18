#include <stdint.h>
#include <string.h>
#include <pb/protocol.h>
#include <pb/command.h>
#include <pb/error.h>

int pb_command_init(struct pb_command_ctx *ctx,
                    pb_command_send_result_t send_result,
                    pb_command_io_t read,
                    pb_command_io_t write)
{
    memset(ctx, 0, sizeof(*ctx));

    ctx->authenticated = false;
    ctx->send_result = send_result;
    ctx->read = read;
    ctx->write = write;

    return PB_OK;
}

int pb_command_free(struct pb_command_ctx *ctx)
{
    ctx->authenticated = false;
    return PB_OK;
}

int pb_command_configure(struct pb_command_ctx *ctx,
                         enum pb_commands command_index,
                         pb_command_t command)
{
    if (command_index > PB_MAX_COMMANDS)
        return -PB_ERR;

    ctx->commands[command_index] = command;

    return PB_OK;
}

int pb_command_process(struct pb_command_ctx *ctx, struct pb_command *command)
{
    int rc = PB_OK;
    struct pb_result result;
    pb_command_t cmd;

    memset(&result, 0, sizeof(result));
    pb_protocol_init_result(&result, PB_RESULT_ERROR);

    /* Check for invalid commands */
    if (!pb_protocol_valid_command(command))
    {
        pb_protocol_init_result(&result, PB_RESULT_INVALID_COMMAND);
        goto command_error_out;
    }

    /* Check if command requires authentication */
    //if (pb_slc_get() == PB_SLC_CONFIGURATION_LOCKED)
    //{
        if (pb_protocol_requires_auth(command) && (!ctx->authenticated))
        {
            pb_protocol_init_result(&result, PB_RESULT_NOT_AUTHENTICATED);
            goto command_error_out;
        }
    //}

    cmd = ctx->commands[command->command];

    if (!cmd)
    {
        rc = -PB_ERR;
        pb_protocol_init_result(&result, PB_RESULT_NOT_SUPPORTED);
        goto command_error_out;
    }

    rc = cmd(ctx, command, &result);

command_error_out:
    ctx->send_result(ctx, &result);
    return rc;
}
