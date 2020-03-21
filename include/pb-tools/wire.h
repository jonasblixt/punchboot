/**
 * \file wire.h
 *
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This file contains the 'wire' format for the punchboot bootloader. The
 *  various structs and defines are used for sending data between host and
 *  device.
 *
 */

#ifndef INCLUDE_PB_WIRE_H_
#define INCLUDE_PB_WIRE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <pb-tools/error.h>

/**
 * \def PB_WIRE_MAGIC
 * Magic number present in all the command and result headers.
 *
 * \def PB_COMMAND_REQUEST_MAX_SIZE
 * Maximum number of bytes that can be embedded in a command request
 *
 * \def PB_RESULT_RESPONSE_MAX_SIZE
 * Maximum number of bytes that can be embedded in a response result
 *
 */

#define PB_WIRE_MAGIC 0x50424c30   /* PBL0 */
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
    PB_CMD_DEVICE_READ_CAPS,
    PB_CMD_SLC_SET_CONFIGURATION,
    PB_CMD_SLC_SET_CONFIGURATION_LOCK,
    PB_CMD_SLC_SET_EOL,
    PB_CMD_SLC_REVOKE_KEY,
    PB_CMD_SLC_READ,
    PB_CMD_BOOTLOADER_VERSION_READ,
    PB_CMD_PART_TBL_READ,
    PB_CMD_PART_TBL_INSTALL,
    PB_CMD_PART_VERIFY,
    PB_CMD_PART_ACTIVATE,
    PB_CMD_PART_BPAK_READ,
    PB_CMD_AUTHENTICATE,
    PB_CMD_AUTH_SET_OTP_PASSWORD,
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


enum pb_slc
{
    PB_SLC_INVALID,
    PB_SLC_NOT_CONFIGURED,
    PB_SLC_CONFIGURATION,
    PB_SLC_CONFIGURATION_LOCKED,
    PB_SLC_EOL,
};

/**
 * Punchboot command structure (64 bytes)
 *
 * Alignment: 64 bytes
 */
struct pb_command
{
    uint32_t magic;     /*!< PB Protocol magic, set to 'PBL0' and changed
                            for breaking changes in the protocol */
    uint8_t command;    /*!< Command to be executed (from enum pb_command) */
    uint8_t rz[4];      /*!< Reserved */
    uint8_t request[PB_COMMAND_REQUEST_MAX_SIZE]; /*<! Optional request data */
} __attribute__((packed));

/**
 * Punchboot command result (64 bytes)
 *
 * result_code: Command result code (enum pb_results)
 * response:    Optional response data
 *
 */

struct pb_result
{
    uint32_t magic;
    int8_t result_code;
    uint8_t rz[4];
    uint8_t response[PB_RESULT_RESPONSE_MAX_SIZE];
} __attribute__((packed));

/**
 * PB_CMD_DEVICE_READ_CAPS result structure
 *
 * no_of_buffers:     Number of buffers that the device allocates
 * buffer_size:       Size of each buffer in bytes
 * operation_timeout_us: Timeout in us for any operation
 * bpak_stream_support: The device supports streaming concatenated bpak archives
 *
 */

struct pb_result_device_caps
{
    uint8_t stream_no_of_buffers;
    uint32_t stream_buffer_size;
    uint16_t operation_timeout_ms;
    uint16_t part_erase_timeout_ms;
    uint8_t bpak_stream_support;
    uint32_t chunk_transfer_max_bytes;
    uint8_t rz[18];
} __attribute__((packed));

struct pb_result_part_table_read
{
    uint8_t no_of_entries;
    uint8_t rz[31];
} __attribute__((packed));

#define PB_PART_FLAG_BOOTABLE (1 << 0)
#define PB_PART_FLAG_OTP      (1 << 1)
#define PB_PART_FLAG_WRITABLE (1 << 2)

struct pb_result_part_table_entry
{
    uint8_t uuid[16];
    char description[16];
    uint64_t first_block;
    uint64_t last_block;
    uint16_t block_size;
    uint8_t flags;
    uint8_t memory_id;
    uint8_t rz[12];
};


/**
 * Initialize streaming write to partition
 *
 * size:        Total transfer size
 * part_uuid:   Partition to write to
 *
 */

struct pb_command_stream_initialize
{
    uint8_t part_uuid[16];
    uint8_t rz[16];
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

struct pb_command_activate_part
{
    uint8_t uuid[16];
    uint8_t rz[16];
};

struct pb_command_read_bpak
{
    uint8_t uuid[16];
    uint8_t rz[16];
};

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
 * SLC read
 *
 */

struct pb_result_slc
{
    uint8_t slc;
    uint8_t rz[31];
} __attribute__((packed));

struct pb_result_slc_key_status
{
    uint32_t active[16];
    uint32_t revoked[16];
} __attribute__((packed));

struct pb_command_revoke_key
{
    uint32_t key_id;
    uint8_t rz[28];
} __attribute__((packed));

struct pb_command_boot_part
{
    uint8_t uuid[16];
    uint8_t verbose;
    uint8_t rz[15];
} __attribute__((packed));

struct pb_command_ram_boot
{
    uint8_t verbose;
    uint8_t rz[31];
} __attribute__((packed));

struct pb_command_board
{
    uint8_t command;
    uint32_t request_size;
    uint32_t response_buffer_size;
    uint8_t rz[23];
} __attribute__((packed));

struct pb_result_board
{
    uint32_t size;
    uint8_t rz[28];
} __attribute__((packed));


struct pb_result_board_status
{
    uint32_t size;
    uint8_t rz[28];
} __attribute__((packed));

/**
 * Initializes and resets a command structure. The magic value is populated and
 *  the command code is set.
 *
 * @param[out] command Pointer to a command structure
 * @param[in] command_code The actual command code
 *
 * @return PB_RESULT_OK on success or a negative number on errors
 */
int pb_wire_init_command(struct pb_command *command,
                                enum pb_commands command_code);

/**
 * Initializes and resets a command structure. The magic value is populated,
 *  the command code is set and the request array is populated with data.
 *
 * @param[out] command Pointer to a command structure
 * @param[in] command_code The actual command code
 * @param[in] data Pointer to data that will be stored in the request array
 * @param[in] size Number of bytes of data
 *
 * @return PB_RESULT_OK on success or a negative number on errors
 */
int pb_wire_init_command2(struct pb_command *command,
                              enum pb_commands command_code,
                              void *data,
                              size_t size);

int pb_wire_init_result(struct pb_result *result,
                                enum pb_results result_code);

int pb_wire_init_result2(struct pb_result *result,
                             enum pb_results result_code,
                             void *data,
                             size_t size);

bool pb_wire_valid_command(struct pb_command *command);
bool pb_wire_valid_result(struct pb_result *result);
bool pb_wire_requires_auth(struct pb_command *command);

const char *pb_wire_command_string(enum pb_commands cmd);
const char *pb_wire_slc_string(enum pb_slc slc);

#endif  // INCLUDE_PB_WIRE_H_
