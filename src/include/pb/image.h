/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __IMAGE_H__
#define __IMAGE_H__

#include <stdint.h>
#include <stdbool.h>

#define PB_IMAGE_HEADER_MAGIC 0xd32daeba
#define PB_IMAGE_MAX_COMP 16
#define PB_COMP_HDR_VERSION 1

/* Image header */

struct pb_image_hdr {
    uint32_t header_magic;
    uint32_t header_version;
    uint32_t no_of_components;
    uint32_t key_index;
    uint32_t _reserved[15];
    uint8_t sha256[32];
    uint8_t sign[1024];
    uint32_t sign_length;
} __attribute__ ((packed));


#define PB_IMAGE_COMPTYPE_TEE      0
#define PB_IMAGE_COMPTYPE_VMM      1
#define PB_IMAGE_COMPTYPE_LINUX    2
#define PB_IMAGE_COMPTYPE_DT       3
#define PB_IMAGE_COMPTYPE_RAMDISK  4
#define PB_IMAGE_COMPTYPE_ATF      5

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
    uint8_t padding[16];
};

bool pb_image_verify(struct pb_pbi *pbi);
uint32_t pb_image_load_from_fs(uint32_t part_lba_offset, struct pb_pbi **pbi);
struct pb_component_hdr * pb_image_get_component(struct pb_pbi *pbi, 
                                            uint32_t comp_type);
struct pb_pbi * pb_image(void);

#endif
