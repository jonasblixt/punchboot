#ifndef __RECOVERY_H__
#define __RECOVERY_H__

#include <sys/types.h>


#define PB_USB_REQUEST_TYPE 0x20
#define PB_PREP_BUFFER 0x21
#define PB_PROG_BOOTLOADER 0x22
#define PB_GET_VERSION 0x23
#define PB_DO_RESET   0x024
#define PB_PROG_PART 0x0025

struct pb_cmd {
    uint32_t cmd;
    uint32_t data[60];
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
