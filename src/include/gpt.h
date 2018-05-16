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

#include "pb_types.h"

#define GPT_HEADER_RSZ 420

struct gpt_header {
    u64 signature;
    u32 rev;
    u32 hdr_sz;
    u32 hdr_crc;
    u32 __reserved;
    u64 current_lba;
    u64 backup_lba;
    u64 first_lba;
    u64 last_lba;
    u8 disk_uuid[16];
    u64 entries_start_lba;
    u32 no_of_parts;
    u32 part_entry_sz;
    u32 part_array_crc;
    u8 __reserved2[GPT_HEADER_RSZ];

} __attribute__ ((packed));

struct gpt_part_hdr {
    u8 type_uuid[16];
    u8 uuid[16];
    u64 first_lba;
    u64 last_lba;
    u8 attr[8];
    u8 name[72];
} __attribute__ ((packed));


struct gpt_primary_tbl {
    struct gpt_header hdr;
    struct gpt_part_hdr part[128];
} __attribute__ ((packed));

u32 gpt_init(void);
u32 gpt_get_part_offset(u8 part_no);
s8  gpt_get_part_by_uuid(const u8 *uuid);

struct gpt_primary_tbl* gpt_get_tbl(void);
u32 gpt_write_tbl(void);
u32 gpt_init_tbl(u64 first_lba, u64 last_lba);
u32 gpt_add_part(u8 part_idx, u32 no_of_blocks, const u8 *type_uuid, 
                                                    const char *part_name);


#endif
