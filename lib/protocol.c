/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <string.h>
#include <pb-tools/protocol.h>
#include <pb-tools/error.h>

bool pb_protocol_valid_result(struct pb_result *result)
{
    if (result->magic != PB_PROTO_MAGIC)
        return false;

    return true;
}

int pb_protocol_init_command(struct pb_command *command,
                                enum pb_commands command_code)
{
    memset(command, 0, sizeof(*command));
    command->magic = PB_PROTO_MAGIC;
    command->command = (uint8_t) command_code;

    return PB_OK;
}

int pb_protocol_init_command2(struct pb_command *command,
                              enum pb_commands command_code,
                              void *data,
                              size_t size)
{
    pb_protocol_init_command(command, command_code);

    if (size < PB_COMMAND_REQUEST_MAX_SIZE)
        return -PB_NO_MEMORY;

    memcpy(command->request, data, size);

    return PB_OK;
}

int pb_protocol_init_result(struct pb_result *result,
                                enum pb_results result_code)
{
    memset(result, 0, sizeof(*result));
    result->magic = PB_PROTO_MAGIC;
    result->result_code = (uint8_t) result_code;

    return PB_OK;
}

int pb_protocol_init_result2(struct pb_result *result,
                             enum pb_results result_code,
                             void *data,
                             size_t size)
{
    pb_protocol_init_result(result, result_code);

    if (size > PB_RESULT_RESPONSE_MAX_SIZE)
        return -PB_NO_MEMORY;

    memcpy(result->response, data, size);

    return PB_OK;
}

const char *pb_protocol_command_string(enum pb_commands cmd)
{
    return "";
}


bool pb_protocol_requires_auth(struct pb_command *command)
{
    bool result = true;

    /* Commands that do not require authentication */
    switch (command->command)
    {
        case PB_CMD_DEVICE_IDENTIFIER_READ:
            result = false;
        break;
        case PB_CMD_AUTHENTICATE:
            result = false;
        break;
        default:
        break;
    }

    return result;
}


bool pb_protocol_valid_command(struct pb_command *command)
{
    if (command->magic != PB_PROTO_MAGIC)
        return false;

    bool valid_command = ((command->command > PB_CMD_INVALID) &&
            (command->command < PB_CMD_END));

    return valid_command;
}
