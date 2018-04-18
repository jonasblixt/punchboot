#ifndef __RECOVERY_H__
#define __RECOVERY_H__

#include <sys/types.h>

#define PB_CMD_TRANSFER_DATA 0xAA
#define PB_CMD_FLASH_BOOT 0xB0
#define PB_CMD_RESET      0xB1
#define PB_CMD_WRITE_PART 0xB2


struct pb_usb_command_hdr {
    uint32_t cmd;
    uint32_t payload_sz;
    uint32_t payload_crc;
    uint32_t header_crc;
} __attribute__ ((packed));


struct pb_chunk_hdr {
    uint16_t chunk_no;
    uint16_t chunk_sz;
} __attribute__ ((packed));

struct pb_write_part_hdr {
    uint32_t part_no;
    uint32_t lba_offset;
    uint32_t no_of_blocks;
} __attribute__ ((packed));



#endif
