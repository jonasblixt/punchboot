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

#include <pb/pb.h>
#include <stdint.h>
#include <stdbool.h>

#define GPT_HEADER_RSZ 420


struct gpt_header 
{
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

#define PB_GPT_ATTR_OK       (1 << 7) /*Bit 55*/
#define PB_GPT_ATTR_ROLLBACK (1 << 6) /*Bit 54*/
#define PB_GPT_ATTR_RFU1     (1 << 5) /*Bit 53*/
#define PB_GPT_ATTR_RFU2     (1 << 4) /*Bit 52*/
#define PB_GPT_ATTR_COUNTER3 (1 << 3) /*Bit 51*/
#define PB_GPT_ATTR_COUNTER2 (1 << 2) /*Bit 50*/
#define PB_GPT_ATTR_COUNTER1 (1 << 1) /*Bit 49*/
#define PB_GPT_ATTR_COUNTER0 (1 << 0) /*Bit 48*/

struct gpt_part_hdr 
{
    uint8_t type_uuid[16];
    uint8_t uuid[16];
    uint64_t first_lba;
    uint64_t last_lba;
    uint8_t attr[8];
    uint8_t name[GPT_PART_NAME_MAX_SIZE*2];
} __attribute__ ((packed));


struct gpt_primary_tbl 
{
    struct gpt_header hdr;
    struct gpt_part_hdr part[128];
} __attribute__ ((packed));

struct gpt_backup_tbl 
{
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
uint32_t gpt_pb_attr_setbits(struct gpt_part_hdr *part, uint8_t attr);
uint32_t gpt_pb_attr_clrbits(struct gpt_part_hdr *part, uint8_t attr);
bool gpt_pb_attr_ok(struct gpt_part_hdr *part);
uint8_t gpt_pb_attr_counter(struct gpt_part_hdr *part);
#endif
