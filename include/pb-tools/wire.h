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
 * Generally a struct pb_command is sent from the host and the device is
 * expected to respond with a struct pb_result.
 *
 * Additional read/writes are optional but must always be terminated by
 * sending a struct pb_result
 *
 *
 * The #pb_commands enum encodes all of the available commands.
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
#define PB_COMMAND_REQUEST_MAX_SIZE 503
#define PB_RESULT_RESPONSE_MAX_SIZE 503

/*! \public
 *
 * The punchboot protocol support two different methods of authentication.
 * The device may choose to implement more then one.
 */
enum pb_auth_method
{
    PB_AUTH_INVALID,     /*!< Invalid, guard */
    PB_AUTH_ASYM_TOKEN,  /*!< Use a signature based authentication token */
    PB_AUTH_PASSWORD,    /*!< Use a password based authentication */
};

/*! \public
 * Punchboot commands
 */
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
    PB_CMD_PART_ERASE,
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
    uint32_t magic;     /*!< PB wire format magic, set to 'PBL0' and changed
                            for breaking changes in the protocol */
    uint8_t command;    /*!< Command to be executed, from enum pb_commands */
    uint8_t rz[4];      /*!< Reserved */
    uint8_t request[PB_COMMAND_REQUEST_MAX_SIZE]; /*<! Optional request data */
} __attribute__((packed));

/**
 * Punchboot command result (64 bytes)
 *
 * Alignment: 64 Bytes
 */
struct pb_result
{
    uint32_t magic;     /*!< PB wire format magic */
    int8_t result_code; /*!< Command result code */
    uint8_t rz[4];      /*!< Reserved */
    uint8_t response[PB_RESULT_RESPONSE_MAX_SIZE]; /*!< Response data */
} __attribute__((packed));

/**
 * Device capabilities
 */
struct pb_result_device_caps
{
    uint8_t stream_no_of_buffers;       /*!< Number of stream buffers */
    uint32_t stream_buffer_size;        /*!< Size of stream buffers in bytes */
    uint16_t operation_timeout_ms;      /*!< Generic operation timeout */
    uint16_t part_erase_timeout_ms;     /*!< Maximum erase time for a partition */
    uint8_t bpak_stream_support;        /*!< Set to 1 if the device supports
                                        receiving concatenated bpak archives */

    uint32_t chunk_transfer_max_bytes;  /*!< Maximum number of bytes in one
                                                                    transfer */
    uint8_t rz[18];                     /*!< Reserved */
} __attribute__((packed));

/**
 * Read partition table response
 */
struct pb_result_part_table_read
{
    uint8_t no_of_entries;  /*!< Number of partitions in the following data */
    uint8_t rz[31];         /*!< Reserved */
} __attribute__((packed));

/**
 * \def PB_WIRE_PART_FLAG_BOOTABLE
 * The partition is bootable
 *
 * \def PB_WIRE_PART_FLAG_OTP
 * The partition can only be written once
 *
 * \def PB_WIRE_PART_FLAG_WRITABLE
 * The partition is writeable
 *
 * \def PB_WIRE_PART_FLAG_ERASE_BEFORE_WRITE
 * The partition must be erased before any write operation
 */

#define PB_WIRE_PART_FLAG_BOOTABLE (1 << 0)
#define PB_WIRE_PART_FLAG_OTP      (1 << 1)
#define PB_WIRE_PART_FLAG_WRITABLE (1 << 2)
#define PB_WIRE_PART_FLAG_ERASE_BEFORE_WRITE (1 << 3)

struct pb_result_part_table_entry
{
    uint8_t uuid[16];     /*!< Partition UUID */
    char description[16]; /*!< Textual description of partition */
    uint64_t first_block; /*!< Partition start block */
    uint64_t last_block;  /*!< Last(inclusive) block of partition */
    uint16_t block_size;  /*!< Block size */
    uint8_t flags;        /*!< Flags */
    uint8_t rz[13];       /*!< Reserved */
};


/**
 * Initialize streaming write to partition
 *
 */
struct pb_command_stream_initialize
{
    uint8_t part_uuid[16]; /*!< Partition UUID */
    uint8_t rz[16];        /*!< Reserved */
} __attribute__((packed));

/**
 * Prepare buffer to receive data
 *
 * This command must always be followd by a data transfer corresponding the
 *  size field.
 */
struct pb_command_stream_prepare_buffer
{
    uint32_t size;  /*!< Bytes to transfer into buffer */
    uint8_t id;     /*!< Buffer ID */
    uint8_t rz[27]; /*!< Reserved */
} __attribute__((packed));

/**
 * Write data from an internal buffer to a partition
 */
struct pb_command_stream_write_buffer
{
    uint32_t size;     /*!< Bytes to transfer from buffer to partition */
    uint64_t offset;   /*!< Offset in bytes into the partition */
    uint8_t buffer_id; /*!< Source buffer id */
    uint8_t rz[19];    /*!< Reserved */
} __attribute__((packed));

/**
 * Authentication command
 *
 * The authentication command must be followed by a write opertion by the host
 * that contains the authentication data.
 */
struct pb_command_authenticate
{
    uint8_t method;  /*!< Method to use for authenticaton see enum pb_auth_method */
    uint16_t size;   /*!< Authentication data size in bytes */
    uint8_t rz[29];  /*!< Reserved */
} __attribute__((packed));

/**
 * Verify partition
 *
 * The device computes a sha256 hash and compares with the input hash
 */
struct pb_command_verify_part
{
    uint8_t uuid[16];   /*!< UUID of partition to verify */
    uint8_t sha256[32]; /*!< Expected sha256 hash */
    uint32_t size;      /*!< Size in bytes */
    uint8_t bpak;       /*!< Parse bpak header */
    uint8_t rz[2];      /*!< Reserved */
} __attribute__((packed));

/**
 * Activate bootable partition command
 *
 */
struct pb_command_activate_part
{
    uint8_t uuid[16];   /*!< UUID of partition to activate */
    uint8_t rz[16];     /*!< Reserved */
};

/**
 * Read BPAK header command
 *
 * If the last 4kByte of the partition contains a valid bpak header the
 * header is sent to the host using this command.
 */
struct pb_command_read_bpak
{
    uint8_t uuid[16];   /*!< Partition UUID */
    uint8_t rz[16];     /*!< Reserved */
};

/**
 * Erase partition command
 */
struct pb_command_erase_part
{
    uint8_t uuid[16];   /*!< UUID of partition to erase */
    uint8_t rz[16];     /*!< Reserved */
};

/**
 * Read device identifier
 */
struct pb_result_device_identifier
{
    uint8_t device_uuid[16];    /*!< Device UUID */
    char board_id[16];          /*!< Board id string */
    uint8_t rz[23];             /*!< Reserved */
} __attribute__((packed));

/**
 * Security Life Cycle (SLC) result
 */
struct pb_result_slc
{
    uint8_t slc;        /*!< SLC state */
    uint8_t rz[31];     /*!< Reserved */
} __attribute__((packed));

/**
 * Active/Revoked keys result
 */
struct pb_result_slc_key_status
{
    uint32_t active[16];  /*!< ID's of keys that are active */
    uint32_t revoked[16]; /*!< ID's of keys that are revoked */
} __attribute__((packed));

/**
 * Revoke key command
 */
struct pb_command_revoke_key
{
    uint32_t key_id; /*!< ID of key to revoke */
    uint8_t rz[28];  /*!< Reserved */
} __attribute__((packed));

/**
 * Boot from partition command
 */
struct pb_command_boot_part
{
    uint8_t uuid[16];   /*!< UUID of bootable partition */
    uint8_t verbose;    /*!< Verbose boot output */
    uint8_t rz[15];     /*!< Reserved */
} __attribute__((packed));

/**
 * Ram boot command
 *
 * A bpak image is transfered to ram and executed
 */
struct pb_command_ram_boot
{
    uint8_t verbose;    /*!< Verbose boot output */
    uint8_t rz[31];     /*!< Reserved */
} __attribute__((packed));

/**
 * Board specific command
 *
 * This optional command, when called will run a function in the board module
 * that can be unique for a board.
 */
struct pb_command_board
{
    uint8_t command;        /*!< Board command to run */
    uint32_t request_size;  /*!< Request size in bytes */
    uint32_t response_buffer_size;  /*!< Maximum amount of data that can be
                                        included in the response */
    uint8_t rz[23];         /*!< Reserved */
} __attribute__((packed));

/**
 * The result structure for the board command
 */
struct pb_result_board
{
    uint32_t size;  /*!< Bytes to read after the result structure */
    uint8_t rz[28]; /*!< Reserved */
} __attribute__((packed));

/**
 * Board status result
 */
struct pb_result_board_status
{
    uint32_t size;  /*!< Bytes to read after the result structure */
    uint8_t rz[28]; /*!< Reserved */
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

/**
 * Initializes and resets a result structure. The magic value is populated
 * and the result code is set.
 *
 * @param[out] result pointer to a pb_result structure
 * @param[in] result_code The result code
 *
 * @return PB_RESULT_OK on success or a negative number on errors
 *
 */
int pb_wire_init_result(struct pb_result *result,
                                enum pb_results result_code);

/**
 * Initializes and resets a result structure. The magic value is populated
 * and the result code is set.
 *
 * @param[out] result pointer to a pb_result structure
 * @param[in] result_code The result code
 * @param[in] data Pointer to data that will be stored in the response array
 * @param[in] size size in bytes of the data
 *
 * @return PB_RESULT_OK on success or a negative number on errors
 *
 */
int pb_wire_init_result2(struct pb_result *result,
                             enum pb_results result_code,
                             void *data,
                             size_t size);
/**
 * Checks that a command is valid and contains the correct magic number
 *
 * @param[in] command The command to check
 *
 * @return True on if the command is valid or false if not
 */
bool pb_wire_valid_command(struct pb_command *command);

/**
 * Checks that a result is valid and contains the correct magic number
 *
 * @param[in] result The result to check
 *
 * @return True on if the result is valid or false if not
 */
bool pb_wire_valid_result(struct pb_result *result);

/**
 * Checks if a command requires the user to authenticate before issuing the
 * command.
 *
 * @param[in] command The command to check
 *
 * @return True if the command requires an authenticated session
 */
bool pb_wire_requires_auth(struct pb_command *command);

/**
 * Translates a punchboot command to a textual representation
 *
 * @param[in] cmd The command
 *
 * @return a string or ""
 */
const char *pb_wire_command_string(enum pb_commands cmd);

/**
 * Translates a punchboot security life cycle value to a string
 *
 * @param[in] slc SLC value
 *
 * @return a string or ""
 */
const char *pb_wire_slc_string(enum pb_slc slc);

#endif  // INCLUDE_PB_WIRE_H_
