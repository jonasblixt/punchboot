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
#include <pb/error.h>

#define PB_PROTO_MAGIC 0x50424c30   /* PBL0 */

enum pb_auth_method
{
    PB_AUTH_INVALID,
    PB_AUTH_ASYM_TOKEN,
    PB_AUTH_PASSWORD,
};

enum pb_command
{
    PB_CMD_INVALID,
    PB_CMD_RESET,
    PB_CMD_GET_DEVICE_UUID,
    PB_CMD_GET_DEVICE_PARAMS,
    PB_CMD_FUSE_CONFIG,
    PB_CMD_FUSE_CONFIG_LOCK,
    PB_CMD_REVOKE_ROM_KEY,
    PB_CMD_BL_VERSION,
    PB_CMD_PART_TBL_GET,
    PB_CMD_PART_TBL_INSTALL,
    PB_CMD_PART_WRITE,
    PB_CMD_PART_VERIFY,
    PB_CMD_PART_ACTIVATE,
    PB_CMD_PART_DESCRIBE,
    PB_CMD_AUTHENTICATE,
    PB_CMD_BUFFER_WRITE,
    PB_CMD_BOOT_PART,
    PB_CMD_BOOT_RAM,
    PB_CMD_BOARD_COMMAND,
};

enum pb_security_life_cycle
{
    PB_SLC_INVALID,
    PB_SLC_NOT_CONFIGURED,
    PB_SLC_CONFIGURATION,
    PB_SLC_CONFIGURATION_LOCKED,
    PB_SLC_EOL,
};

/**
 * Command header, 64 byte
 *
 */

struct pb_cmd_header
{
    uint32_t magic;
    uint32_t cmd;
    uint64_t size;
    uint32_t arg0;
    uint32_t arg1;
    uint32_t arg2;
    uint32_t arg3;
    uint8_t pad[32];        /* Pad to 64 byte */
} __attribute__((packed));

/*
 * Prepare transfer buffer, 64 byte
 *
 */
struct pb_cmd_prep_buffer
{
    uint64_t no_of_blocks;
    uint8_t buffer_id;
    uint8_t pad[55];        /* Pad to 64 bytes */
} __attribute__((packed));

/* Write command structure, 64 byte
 *
 *
 */

struct pb_cmd_write_part
{
    uint64_t no_of_blocks;
    uint64_t offset;
    uint8_t part_no;
    uint8_t buffer_id;
    uint8_t pad[46];        /* Pad to 64 bytes */
} __attribute__((packed));

struct pb_part_table_entry
{
    uint64_t first_block;
    uint64_t last_block;
    uint8_t uuid[16];
    uint32_t block_alignment;
    uint8_t pad[26];        /* Pad to 64 bytes */
} __attribute__((packed));

int pb_init_header(struct pb_cmd_header *hdr);
int pb_valid_header(struct pb_cmd_header *hdr);

const char *pb_slc_string(enum pb_security_life_cycle slc);
const char *pb_command_string(enum pb_command cmd);

#endif  // INCLUDE_PB_PROTOCOL_H_
