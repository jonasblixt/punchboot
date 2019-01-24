/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __RECOVERY_H__
#define __RECOVERY_H__

#include <stdint.h>

#define PB_RECOVERY_PROTOCOL_VERSION 1

enum {
    PB_CMD_RESET,
    PB_CMD_FLASH_BOOTLOADER,
    PB_CMD_PREP_BULK_BUFFER,
    PB_CMD_GET_VERSION,
    PB_CMD_GET_GPT_TBL,
    PB_CMD_WRITE_PART,
    PB_CMD_BOOT_PART,
    PB_CMD_GET_CONFIG_TBL,
    PB_CMD_GET_CONFIG_VAL,
    PB_CMD_SET_CONFIG_VAL,
    PB_CMD_READ_UUID,
    PB_CMD_WRITE_DFLT_GPT,
    PB_CMD_BOOT_RAM,
    PB_CMD_SETUP,
    PB_CMD_SETUP_LOCK,
};

extern const char *recovery_cmd_name[];

struct pb_cmd_header
{
    uint32_t cmd;
    uint32_t size;
    uint32_t arg0;
    uint32_t arg1;
    uint32_t arg2;
    uint32_t arg3;
    uint8_t rz[40];
} __attribute__ ((packed));

struct pb_cmd_prep_buffer 
{
    uint32_t buffer_id;
    uint32_t no_of_blocks;
    uint8_t rz[56];
} __attribute__ ((packed));

struct pb_cmd_write_part 
{
    uint32_t no_of_blocks;
    uint32_t lba_offset;
    uint32_t part_no;
    uint32_t buffer_id;
    uint8_t rz[48];
} __attribute__ ((packed));

struct pb_device_setup
{
    uint8_t uuid[16];
    uint8_t device_variant;
    uint8_t device_revision;
    uint8_t dry_run;
    uint8_t rz[45];
} __attribute__ ((packed));

void recovery_initialize(void);

#endif
