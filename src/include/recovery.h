#ifndef __RECOVERY_H__
#define __RECOVERY_H__

#include "pb_types.h"

#define PB_CMD_RESET            0x00000001
#define PB_CMD_FLASH_BOOTLOADER 0x00000002
#define PB_CMD_PREP_BULK_BUFFER 0x00000003
#define PB_CMD_GET_VERSION      0x00000004
#define PB_CMD_GET_GPT_TBL      0x00000005
#define PB_CMD_WRITE_PART       0x00000006
#define PB_CMD_BOOT_PART        0x00000007



struct pb_usb_cmd {
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

void recovery(void);

#endif
