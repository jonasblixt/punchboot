#ifndef __RECOVERY_PROTOCOL_H__
#define __RECOVERY_PROTOCOL_H__

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
    PB_CMD_WRITE_UUID,
    PB_CMD_READ_UUID,
    PB_CMD_WRITE_DFLT_GPT,
    PB_CMD_WRITE_DFLT_FUSE,
};

extern const char *recovery_cmd_name[];

struct pb_cmd_header
{
    uint32_t cmd;
    uint32_t size;
} __attribute__ ((packed));

struct pb_cmd_prep_buffer 
{
    uint32_t buffer_id;
    uint32_t no_of_blocks;
} __attribute__ ((packed));

struct pb_cmd_write_part 
{
    uint32_t no_of_blocks;
    uint32_t lba_offset;
    uint32_t part_no;
    uint32_t buffer_id;
} __attribute__ ((packed));

#endif
