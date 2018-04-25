/**
 * Punch BOOT bootloader cli
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __GPT_H__
#define __GPT_H__

#include "pb_types.h"

struct gpt_header {
    u8 signature[8];
    u32 rev;
    u32 hdr_sz;
    u32 hdr_crc;
    u32 __reserved;
    u32 current_lba;
    u32 current_lba_hi;
    u32 backup_lba;
    u32 backup_lba_hi;
    u32 first_lba;
    u32 first_lba_hi;
    u32 last_lba;
    u32 last_lba_hi;
    u8 disk_uuid[16];
    u32 entries_start_lba;
    u32 entries_start_lba_hi;
    u32 no_of_parts;
    u32 part_entry_sz;
    u32 part_array_crc;
    u8 __reserved2[420];

} __attribute__ ((packed));

struct gpt_part_hdr {
    u8 type_uuid[16];
    u8 uuid[16];
    u32 first_lba;
    u32 first_lba_hi;
    u32 last_lba;
    u32 last_lba_hi;
    u8 attr[8];
    u8 name[72];
} __attribute__ ((packed));


struct gpt_primary_tbl {
    struct gpt_header hdr;
    struct gpt_part_hdr part[128];
} __attribute__ ((packed));


#endif
