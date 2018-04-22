#ifndef __PB_IMAGE_H__
#define __PB_IMAGE_H__

#include "pb_types.h"

#define PB_IMAGE_HEADER_MAGIC 0xd32daeba

/* Image header */

struct pb_image_hdr {
    u32 header_magic;
    u32 header_version;
    u32 no_of_components;
    u32 _reserved[16];
    u8 sha256[32];
    u8 sign[1024];
    u32 sign_length;
} __attribute__ ((packed));


#define PB_IMAGE_COMPTYPE_TEE      0
#define PB_IMAGE_COMPTYPE_VMM      1
#define PB_IMAGE_COMPTYPE_LINUX    2
#define PB_IMAGE_COMPTYPE_DT       3
#define PB_IMAGE_COMPTYPE_RAMDISK  4


/* Component header */
struct pb_component_hdr {
    u32 comp_header_version;
    u32 component_type;
    u32 load_addr_low;
    u32 load_addr_high;
    u32 component_size;
    u32 component_offset;
    u32 _reserved[16];
} __attribute__ ((packed));



struct pb_pbi {
    struct pb_image_hdr hdr;
    struct pb_component_hdr comp[16];
    u8 padding[511];
};

#endif
