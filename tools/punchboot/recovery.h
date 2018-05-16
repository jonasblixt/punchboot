/**
 * Punch BOOT bootloader cli
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __RECOVERY_H__
#define __RECOVERY_H__

#include "pb_types.h"

#define PB_CMD_DO_RESET         0x00000001
#define PB_CMD_FLASH_BOOTLOADER 0x00000002
#define PB_CMD_PREP_BULK_BUFFER 0x00000003
#define PB_CMD_GET_VERSION      0x00000004
#define PB_CMD_GET_GPT_TBL      0x00000005
#define PB_CMD_WRITE_PART       0x00000006
#define PB_CMD_BOOT_PART        0x00000007
#define PB_CMD_GET_CONFIG_TBL   0x00000008
#define PB_CMD_GET_CONFIG_VAL   0x00000009

#define PB_CMD_WRITE_UUID       0x00000100
#define PB_CMD_READ_UUID        0x00000101
#define PB_CMD_WRITE_DFLT_GPT   0x00000102
#define PB_CMD_WRITE_DFLT_FUSE  0x00000103

struct pb_config_item {
     s8 index;
     char description[16];
 #define PB_CONFIG_ITEM_RW 1
 #define PB_CONFIG_ITEM_RO 2
 #define PB_CONFIG_ITEM_OTP 3
     u8 access;
     u32 default_value;
} __attribute__ ((packed));

struct pb_cmd {
    u32 cmd;
    u8 data[60];
} __attribute__ ((packed));



struct pb_cmd_prep_buffer {
    u32 cmd;
    u32 no_of_blocks;
    u32 buffer_id;
    u8 _reserved[52];
} __attribute__ ((packed));


struct pb_cmd_write_part {
    u32 cmd;
    u32 no_of_blocks;
    u32 lba_offset;
    u32 part_no;
    u32 buffer_id;
    u8 _reserved[44];
} __attribute__ ((packed));




#endif
