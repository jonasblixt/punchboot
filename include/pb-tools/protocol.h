/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_PROTOCOL_H_
#define INCLUDE_PB_PROTOCOL_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <pb-tools/error.h>

#define PB_PROTO_MAGIC 0x50424c30   /* PBL0 */
#define PB_COMMAND_REQUEST_MAX_SIZE 55
#define PB_RESULT_RESPONSE_MAX_SIZE 55

enum pb_auth_method
{
    PB_AUTH_INVALID,
    PB_AUTH_ASYM_TOKEN,
    PB_AUTH_PASSWORD,
};

enum pb_commands
{
    PB_CMD_INVALID,
    PB_CMD_DEVICE_RESET,
    PB_CMD_DEVICE_IDENTIFIER_READ,
    PB_CMD_DEVICE_CONFIG,
    PB_CMD_DEVICE_CONFIG_LOCK,
    PB_CMD_DEVICE_EOL,
    PB_CMD_DEVICE_REVOKE_KEY,
    PB_CMD_DEVICE_SLC_READ,
    PB_CMD_DEVICE_READ_CAPS,
    PB_CMD_VERSION_READ,
    PB_CMD_PART_TBL_READ,
    PB_CMD_PART_TBL_INSTALL,
    PB_CMD_PART_VERIFY,
    PB_CMD_PART_ACTIVATE,
    PB_CMD_PART_BPAK_READ,
    PB_CMD_AUTHENTICATE,
    PB_CMD_SET_PASSWORD,
    PB_CMD_STREAM_INITIALIZE,
    PB_CMD_STREAM_PREPARE_BUFFER,
    PB_CMD_STREAM_WRITE_BUFFER,
    PB_CMD_STREAM_FINALIZE,
    PB_CMD_BOOT_PART,
    PB_CMD_BOOT_RAM,
    PB_CMD_BOARD_COMMAND,
    PB_CMD_BOARD_STATUS_READ,
    PB_CMD_END,                     /* Sentinel, must be the last entry */
};

enum pb_results
{
    PB_RESULT_OK,
    PB_RESULT_ERROR,
    PB_RESULT_AUTHENTICATION_FAILED,
    PB_RESULT_NOT_AUTHENTICATED,
    PB_RESULT_NOT_SUPPORTED,
    PB_RESULT_INVALID_ARGUMENT,
    PB_RESULT_INVALID_COMMAND,
    PB_RESULT_PART_VERIFY_FAILED,
    PB_RESULT_PART_NOT_BOOTABLE,
};


/**
 * Punchboot command structure (64 bytes)
 *
 * Alignment: 64 bytes
 *
 * command:  Command to be executed (enum pb_commands)
 * size:     Size of data written after the command (if any)
 * args:     4, 32-bit, optional arguments
 * request:  Optional request data
 *
 */

struct pb_command
{
    uint32_t magic;     /* PB Protocol magic, set to 'PBL0' and changed
                            for breaking changes in the protocol */
    uint8_t command;
    uint32_t size;
    uint8_t request[PB_COMMAND_REQUEST_MAX_SIZE];
} __attribute__((packed));

/**
 * Punchboot command result (64 bytes)
 *
 * result_code: Command result code (enum pb_results)
 * size:        Return data size
 * response:    Optional response data
 *
 */

struct pb_result
{
    uint32_t magic;
    uint8_t result_code;
    uint32_t size;
    uint8_t response[PB_RESULT_RESPONSE_MAX_SIZE];
} __attribute__((packed));

/**
 * PB_CMD_DEVICE_READ_CAPS result structure
 *
 * no_of_buffers:     Number of buffers that the device allocates
 * buffer_size:       Size of each buffer in bytes
 * buffer_alignment:  Data alignment required by the device
 * operation_timeout_us: Timeout in us for any operation
 *
 */

struct pb_result_device_caps
{
    uint8_t stream_no_of_buffers;
    uint32_t stream_buffer_alignment;
    uint32_t stream_buffer_size;
    uint32_t operation_timeout_us;
    uint8_t rz[19];
} __attribute__((packed));

struct pb_result_part_table_read
{
    uint8_t no_of_entries;
    uint8_t rz[31];
} __attribute__((packed));

/**
 * Initialize streaming write to partition
 *
 * size:        Total transfer size
 * part_uuid:   Partition to write to
 *
 */

struct pb_command_stream_initialize
{
    uint64_t size;
    uint8_t part_uuid[16];
    uint8_t rz[8];
} __attribute__((packed));

/**
 * Prepare buffer to receive data
 *
 * Prepares buffer with id 'id' to receive 'size' number of bytes
 *
 * This command must always be followd by a data transfer corresponding the
 *  size field.
 */

struct pb_command_stream_prepare_buffer
{
    uint32_t size;
    uint8_t id;
    uint8_t rz[27];
} __attribute__((packed));

/**
 * Write data from an internal buffer to a partition
 *
 * size:        Size in bytes
 * offset:      Offset in bytes
 * buffer_id:   Buffer to read data from
 *
 */

struct pb_command_stream_write_buffer
{
    uint32_t size;
    uint64_t offset;
    uint8_t buffer_id;
    uint8_t rz[19];
} __attribute__((packed));

/**
 * Authentication command
 *
 *
 */

struct pb_command_authenticate
{
    uint8_t method;
    uint16_t size;
    uint8_t rz[29];
} __attribute__((packed));

/**
 * Verify partition
 *
 * uuid:        Partition UUID
 * sha256:      Expected SHA256
 *
 */

struct pb_command_verify_part
{
    uint8_t uuid[16];
    uint8_t sha256[32];
    uint8_t rz[7];
} __attribute__((packed));

/**
 * Read device UUID
 *
 *
 */

struct pb_result_device_identifier
{
    uint8_t device_uuid[16];
    char board_id[16];
    uint8_t rz[23];
} __attribute__((packed));



/**
 * Initializes and resets a command structure. The magic value is populated and
 *  the command code is set.
 *
 * Returns PB_OK on success.
 *
 */

int pb_protocol_init_command(struct pb_command *command,
                                enum pb_commands command_code);

int pb_protocol_init_command2(struct pb_command *command,
                              enum pb_commands command_code,
                              void *data,
                              size_t size);

int pb_protocol_init_result(struct pb_result *result,
                                enum pb_results result_code);

int pb_protocol_init_result2(struct pb_result *result,
                             enum pb_results result_code,
                             void *data,
                             size_t size);

bool pb_protocol_valid_command(struct pb_command *command);
bool pb_protocol_valid_result(struct pb_result *result);
bool pb_protocol_requires_auth(struct pb_command *command);
const char *pb_protocol_command_string(enum pb_commands cmd);

#endif  // INCLUDE_PB_PROTOCOL_H_
