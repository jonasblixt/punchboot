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

#include <pb.h>

#define PB_CMD_RESET            0x00000001
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


struct pb_usb_cmd {
    uint32_t cmd;
    uint8_t data[60];
} __attribute__ ((packed));


struct pb_cmd_prep_buffer {
     uint32_t cmd;
     uint32_t no_of_blocks;
     uint32_t buffer_id;
     uint8_t _reserved[52];
} __attribute__ ((packed));


struct pb_cmd_write_part {
    uint32_t cmd;
    uint32_t no_of_blocks;
    uint32_t lba_offset;
    uint32_t part_no;
    uint32_t buffer_id;
    uint8_t _reserved[44];
} __attribute__ ((packed));

void recovery(void);

#endif
