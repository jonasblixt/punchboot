/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef INCLUDE_PB_IMAGE_H_
#define INCLUDE_PB_IMAGE_H_

#include <stdint.h>
#include <stdbool.h>

#define PB_IMAGE_HEADER_MAGIC 0xd32daeba
#define PB_IMAGE_MAX_COMP 16
#define PB_COMP_HDR_VERSION 1
#define PB_IMAGE_SIGN_MAX_SIZE 1024

/* Image header, 512b aligned */
struct pb_image_hdr
{
    uint32_t header_magic;
    uint32_t header_version;
    uint32_t no_of_components;
    uint32_t key_index;
    uint32_t hash_kind;
    uint32_t sign_kind;
    uint32_t _reserved[122];
} __attribute__((packed));

enum
{
    PB_IMAGE_COMPTYPE_TEE = 0,
    PB_IMAGE_COMPTYPE_VMM = 1,
    PB_IMAGE_COMPTYPE_LINUX = 2,
    PB_IMAGE_COMPTYPE_DT = 3,
    PB_IMAGE_COMPTYPE_RAMDISK = 4,
    PB_IMAGE_COMPTYPE_ATF = 5,
    PB_IMAGE_COMPTYPE_KERNEL = 6,
};

/* Component header 128b aligned */
struct pb_component_hdr
{
    uint32_t comp_header_version;
    uint32_t component_type;
    uint64_t load_addr;
    uint32_t component_size;
    uint32_t component_offset;
    uint32_t _reserved[26];
} __attribute__((packed));

struct pb_pbi
{
    struct pb_image_hdr hdr;
    uint8_t sign[1024];
    struct pb_component_hdr comp[PB_IMAGE_MAX_COMP];
};

uint32_t pb_image_verify(struct pb_pbi *pbi, const char *inhash);

uint32_t pb_image_load_from_fs(uint32_t part_lba_offset,
                               struct pb_pbi *pbi,
                               const char *hash);

struct pb_component_hdr * pb_image_get_component(struct pb_pbi *pbi,
                                            uint32_t comp_type);
uint32_t pb_image_check_header(struct pb_pbi *pbi);

#endif  // INCLUDE_PB_IMAGE_H_
