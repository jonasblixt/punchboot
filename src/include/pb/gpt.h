/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __GPT_H__
#define __GPT_H__

#include <stdint.h>
#include <stdbool.h>

#define GPT_HEADER_RSZ 420

#define GPT_ATTR_NOT_BOOTABLE ((uint64_t) (0x8000000000000000))

struct gpt_header {
    uint64_t signature;
    uint32_t rev;
    uint32_t hdr_sz;
    uint32_t hdr_crc;
    uint32_t __reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_lba;
    uint64_t last_lba;
    uint8_t disk_uuid[16];
    uint64_t entries_start_lba;
    uint32_t no_of_parts;
    uint32_t part_entry_sz;
    uint32_t part_array_crc;
    uint8_t __reserved2[GPT_HEADER_RSZ];

} __attribute__ ((packed));

struct gpt_part_hdr {
    uint8_t type_uuid[16];
    uint8_t uuid[16];
    uint64_t first_lba;
    uint64_t last_lba;
    uint8_t attr[8];
    uint8_t name[72];
} __attribute__ ((packed));


struct gpt_primary_tbl {
    struct gpt_header hdr;
    struct gpt_part_hdr part[128];
} __attribute__ ((packed));

struct gpt_backup_tbl {
    struct gpt_part_hdr part[128];
    struct gpt_header hdr;
} __attribute__ ((packed));

struct gpt
{
    struct gpt_primary_tbl primary;
    struct gpt_backup_tbl backup;
};

uint32_t gpt_init(struct gpt *gpt);
uint32_t gpt_get_part_first_lba(struct gpt *gpt, uint8_t part_no);
uint64_t gpt_get_part_last_lba(struct gpt *gpt, uint8_t part_no);
uint32_t gpt_get_part_by_uuid(struct gpt *gpt, const char *uuid, struct gpt_part_hdr **part);
uint32_t gpt_write_tbl(struct gpt *gpt);
uint32_t gpt_init_tbl(struct gpt *gpt, uint64_t first_lba, uint64_t last_lba);
uint32_t gpt_add_part(struct gpt *gpt, uint8_t part_idx, uint32_t no_of_blocks, 
                                        const char *type_uuid, 
                                        const char *part_name);

bool gpt_part_is_bootable(struct gpt_part_hdr *part);
uint32_t gpt_part_set_bootable(struct gpt_part_hdr *part, bool bootable);

#endif
