/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __PB_IMAGE_H__
#define __PB_IMAGE_H__

#include <pb.h>

#define PB_IMAGE_HEADER_MAGIC 0xd32daeba

/* Image header */

struct pb_image_hdr {
    uint32_t header_magic;
    uint32_t header_version;
    uint32_t no_of_components;
    uint32_t _reserved[16];
    uint8_t sha256[32];
    uint8_t sign[1024];
    uint32_t sign_length;
} __attribute__ ((packed));


#define PB_IMAGE_COMPTYPE_TEE      0
#define PB_IMAGE_COMPTYPE_VMM      1
#define PB_IMAGE_COMPTYPE_LINUX    2
#define PB_IMAGE_COMPTYPE_DT       3
#define PB_IMAGE_COMPTYPE_RAMDISK  4


/* Component header */
struct pb_component_hdr {
    uint32_t comp_header_version;
    uint32_t component_type;
    uint32_t load_addr_low;
    uint32_t load_addr_high;
    uint32_t component_size;
    uint32_t component_offset;
    uint32_t _reserved[16];
} __attribute__ ((packed));



struct pb_pbi {
    struct pb_image_hdr hdr;
    struct pb_component_hdr comp[16];
    uint8_t padding[511];
};

#endif
