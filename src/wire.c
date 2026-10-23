/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <bpak/bpak.h>
#include <pb-tools/error.h>
#include <pb-tools/wire.h>
#include <string.h>

/* The wire layout is frozen (D-008). This checks it with every compiler that
 * builds the firmware or the host tools; tests/compat/abi.txt has every field. */
_Static_assert(sizeof(struct pb_command) == 512, "pb_command size");
_Static_assert(sizeof(struct pb_result) == 512, "pb_result size");
_Static_assert(sizeof(struct pb_result_device_caps) == 32, "pb_result_device_caps size");
_Static_assert(sizeof(struct pb_result_part_table_read) == 32, "pb_result_part_table_read size");
_Static_assert(sizeof(struct pb_result_part_table_entry) == 128, "pb_result_part_table_entry size");
_Static_assert(sizeof(struct pb_command_stream_initialize) == 32,
               "pb_command_stream_initialize size");
_Static_assert(sizeof(struct pb_command_stream_prepare_buffer) == 32,
               "pb_command_stream_prepare_buffer size");
_Static_assert(sizeof(struct pb_command_stream_write_buffer) == 32,
               "pb_command_stream_write_buffer size");
_Static_assert(sizeof(struct pb_command_stream_read_buffer) == 32,
               "pb_command_stream_read_buffer size");
_Static_assert(sizeof(struct pb_command_authenticate) == 32, "pb_command_authenticate size");
_Static_assert(sizeof(struct pb_command_verify_part) == 55, "pb_command_verify_part size");
_Static_assert(sizeof(struct pb_command_activate_part) == 32, "pb_command_activate_part size");
_Static_assert(sizeof(struct pb_command_read_bpak) == 32, "pb_command_read_bpak size");
_Static_assert(sizeof(struct pb_command_erase_part) == 32, "pb_command_erase_part size");
_Static_assert(sizeof(struct pb_command_install_part_table) == 17,
               "pb_command_install_part_table size");
_Static_assert(sizeof(struct pb_result_device_identifier) == 55,
               "pb_result_device_identifier size");
_Static_assert(sizeof(struct pb_result_slc) == 32, "pb_result_slc size");
_Static_assert(sizeof(struct pb_result_slc_key_status) == 128, "pb_result_slc_key_status size");
_Static_assert(sizeof(struct pb_command_revoke_key) == 32, "pb_command_revoke_key size");
_Static_assert(sizeof(struct pb_command_boot_part) == 32, "pb_command_boot_part size");
_Static_assert(sizeof(struct pb_command_bpak_boot) == 32, "pb_command_bpak_boot size");
_Static_assert(sizeof(struct pb_command_board) == 32, "pb_command_board size");
_Static_assert(sizeof(struct pb_result_board) == 32, "pb_result_board size");
_Static_assert(sizeof(struct pb_result_board_status) == 32, "pb_result_board_status size");
_Static_assert(sizeof(struct pb_result_boot_status) == 32, "pb_result_boot_status size");
_Static_assert(sizeof(struct bpak_transport_meta) == 32, "bpak_transport_meta size");
_Static_assert(sizeof(struct bpak_part_header) == 32, "bpak_part_header size");
_Static_assert(sizeof(struct bpak_meta_header) == 16, "bpak_meta_header size");
_Static_assert(sizeof(struct bpak_header) == 4096, "bpak_header size");

bool pb_wire_valid_result(struct pb_result *result)
{
    if (result->magic != PB_WIRE_MAGIC)
        return false;

    return true;
}

int pb_wire_init_result(struct pb_result *result, enum pb_results result_code)
{
    memset(result, 0, sizeof(*result));
    result->magic = PB_WIRE_MAGIC;
    result->result_code = (uint8_t)result_code;

    return PB_RESULT_OK;
}

int pb_wire_init_result2(struct pb_result *result,
                         enum pb_results result_code,
                         void *data,
                         size_t size)
{
    pb_wire_init_result(result, result_code);

    if (size > PB_RESULT_RESPONSE_MAX_SIZE)
        return -PB_RESULT_NO_MEMORY;

    memcpy(result->response, data, size);

    return PB_RESULT_OK;
}

int pb_wire_init_command(struct pb_command *command, enum pb_commands command_code)
{
    memset(command, 0, sizeof(*command));
    command->magic = PB_WIRE_MAGIC;
    command->command = (uint8_t)command_code;

    return PB_RESULT_OK;
}

int pb_wire_init_command2(struct pb_command *command,
                          enum pb_commands command_code,
                          void *data,
                          size_t size)
{
    pb_wire_init_command(command, command_code);

    if (size > PB_COMMAND_REQUEST_MAX_SIZE)
        return -PB_RESULT_NO_MEMORY;

    memcpy(command->request, data, size);

    return PB_RESULT_OK;
}

const char *pb_wire_command_string(enum pb_commands cmd)
{
    (void)cmd;
    return "";
}

const char *pb_wire_slc_string(enum pb_slc slc)
{
    (void)slc;
    return "";
}

bool pb_wire_requires_auth(struct pb_command *command)
{
    bool result = true;

    /* Commands that do not require authentication */
    switch (command->command) {
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

bool pb_wire_valid_command(struct pb_command *command)
{
    if (command->magic != PB_WIRE_MAGIC)
        return false;

    bool valid_command = ((command->command > PB_CMD_INVALID) && (command->command < PB_CMD_END));

    return valid_command;
}
