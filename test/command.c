#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pb-tools/wire.h>
#include <pb-tools/error.h>

#include "command.h"

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

    return PB_RESULT_OK;
}

int pb_command_free(struct pb_command_ctx *ctx)
{
    ctx->authenticated = false;
    return PB_RESULT_OK;
}

int pb_command_configure(struct pb_command_ctx *ctx,
                         enum pb_commands command_index,
                         pb_command_t command)
{
    if (command_index > PB_MAX_COMMANDS)
        return -PB_RESULT_ERROR;

    ctx->commands[command_index] = command;

    return PB_RESULT_OK;
}

int pb_command_process(struct pb_command_ctx *ctx, struct pb_command *command)
{
    int rc = PB_RESULT_OK;
    struct pb_result result;
    pb_command_t cmd;

    memset(&result, 0, sizeof(result));
    pb_wire_init_result(&result, PB_RESULT_ERROR);

    /* Check for invalid commands */
    if (!pb_wire_valid_command(command))
    {
        pb_wire_init_result(&result, -PB_RESULT_INVALID_COMMAND);
        goto command_error_out;
    }

    /* Check if command requires authentication */
    //if (pb_slc_get() == PB_SLC_CONFIGURATION_LOCKED)
    //{
        if (pb_wire_requires_auth(command) && (!ctx->authenticated))
        {
            pb_wire_init_result(&result, -PB_RESULT_NOT_AUTHENTICATED);
            goto command_error_out;
        }
    //}

    cmd = ctx->commands[command->command];

    if (!cmd)
    {
        printf("Error: Not supported\n");
        rc = -PB_RESULT_ERROR;
        pb_wire_init_result(&result, -PB_RESULT_NOT_SUPPORTED);
        goto command_error_out;
    }

    rc = cmd(ctx, command, &result);

command_error_out:
    ctx->send_result(ctx, &result);
    return rc;
}
