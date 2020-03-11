/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <string.h>
#include <pb/protocol.h>
#include <pb/error.h>

int pb_valid_header(struct pb_cmd_header *hdr)
{
    if (hdr->magic != PB_PROTO_MAGIC)
        return -PB_ERR;

    return PB_OK;
}

int pb_init_header(struct pb_cmd_header *hdr)
{
    memset(hdr, 0, sizeof(*hdr));
    hdr->magic = PB_PROTO_MAGIC;

    return PB_OK;
}

const char *pb_command_string(enum pb_command cmd)
{
    switch (cmd)
    {
        case PB_CMD_RESET:
            return "PB_CMD_RESET";
        case PB_CMD_GET_DEVICE_UUID:
            return "PB_CMD_GET_DEVICE_UUID";
        case PB_CMD_GET_DEVICE_PARAMS:
            return "PB_CMD_GET_DEVICE_PARAMS";
        case PB_CMD_FUSE_CONFIG:
            return "PB_CMD_FUSE_CONFIG";
        case PB_CMD_FUSE_CONFIG_LOCK:
            return "PB_CMD_FUSE_CONFIG_LOCK";
        case PB_CMD_REVOKE_ROM_KEY:
            return "PB_CMD_REVOKE_ROM_KEY";
        case PB_CMD_BL_VERSION:
            return "PB_CMD_BL_VERSION";
        case PB_CMD_PART_TBL_GET:
            return "PB_CMD_PART_TBL_GET";
        case PB_CMD_PART_TBL_INSTALL:
            return "PB_CMD_PART_TBL_INSTALL";
        case PB_CMD_PART_WRITE:
            return "PB_CMD_PART_WRITE";
        case PB_CMD_PART_VERIFY:
            return "PB_CMD_PART_VERIFY";
        case PB_CMD_PART_ACTIVATE:
            return "PB_CMD_PART_ACTIVATE";
        case PB_CMD_PART_DESCRIBE:
            return "PB_CMD_PART_DESCRIBE";
        case PB_CMD_AUTHENTICATE:
            return "PB_CMD_AUTHENTICATE";
        case PB_CMD_BUFFER_WRITE:
            return "PB_CMD_BUFFER_WRITE";
        case PB_CMD_BOOT_PART:
            return "PB_CMD_BOOT_PART";
        case PB_CMD_BOOT_RAM:
            return "PB_CMD_BOOT_RAM";
        case PB_CMD_BOARD_COMMAND:
            return "PB_CMD_BOARD_COMMAND";
        case PB_CMD_INVALID:
        default:
            return "PB_CMD_INVALID";
    }
}

const char *pb_slc_string(enum pb_security_life_cycle slc)
{
    switch (slc)
    {
        case PB_SLC_NOT_CONFIGURED:
            return "Not configured";
        case PB_SLC_CONFIGURATION:
            return "Configuration";
        case PB_SLC_CONFIGURATION_LOCKED:
            return "Configuration locked";
        case PB_SLC_EOL:
            return "EOL";
        case PB_SLC_INVALID:
        default:
            return "Invalid";
    }
}

