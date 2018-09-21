/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <gpt.h>
#include <plat.h>
#include <crc.h>
#include <tinyprintf.h>
#include <pb_string.h>

static uint8_t _flag_gpt_ok = false;
static struct gpt_primary_tbl __attribute__((section (".bigbuffer"))) _gpt1;

static void gpt_part_name(struct gpt_part_hdr *part, uint8_t *out, uint8_t len) 
{
    uint8_t null_count = 0;

    for (int i = 0; i < len*2; i++) 
    {
        if (part->name[i]) {
            *out++ = part->name[i]; 
            null_count = 0;
        } else {
            null_count++;
        }

        *out = 0;

        if (null_count > 1)
            break;
    }
}

struct gpt_primary_tbl * gpt_get_tbl(void) 
{
    return &_gpt1;
}

uint32_t gpt_get_part_offset(uint8_t part_no) 
{
    return _gpt1.part[part_no & 0x1f].first_lba;
}

int8_t  gpt_get_part_by_uuid(const uint8_t *uuid) 
{
    if (!_flag_gpt_ok)
        return -1;

    for (unsigned int i = 0; i < _gpt1.hdr.no_of_parts; i++) {
        if (_gpt1.part[i].first_lba == 0)
            return -1;

        if (memcmp(_gpt1.part[i].type_uuid, uuid, 16) == 0)
            return i;
    }
    return -1;
}

static const uint64_t __gpt_header_signature = 0x5452415020494645ULL;
static uint32_t prng_state;

/* Low entropy source for setting up UUID's */
static uint32_t gpt_prng(void)
{
	uint32_t x = prng_state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	prng_state = x;

	return x;
}

/* TODO: Maybe rename struct gpt_primary_tbl to gpt_tbl */
uint32_t gpt_init_tbl(uint64_t first_lba, uint64_t last_lba) 
{
    struct gpt_header *hdr = &_gpt1.hdr;
    
    prng_state = plat_get_us_tick();

    memset((uint8_t *) &_gpt1, 0, sizeof(struct gpt_primary_tbl));

    hdr->signature = __gpt_header_signature;
    hdr->rev = 0x00001000;
    hdr->hdr_sz = sizeof(struct gpt_header) - GPT_HEADER_RSZ;
    hdr->current_lba = first_lba;
    hdr->no_of_parts = 128;
    
    hdr->first_lba = hdr->current_lba + 1 
            + (hdr->no_of_parts*sizeof(struct gpt_part_hdr)) / 512;
    hdr->backup_lba = last_lba;
    hdr->entries_start_lba = _gpt1.hdr.first_lba + 1;
    hdr->part_entry_sz = sizeof(struct gpt_part_hdr);

    for (int i = 0; i < 16; i++) 
        hdr->disk_uuid[i] = gpt_prng();
    
    return PB_OK;
}

uint32_t gpt_add_part(uint8_t part_idx, uint32_t no_of_blocks, 
                      const uint8_t *type_uuid, const char *part_name) 
{

    struct gpt_part_hdr *part = &_gpt1.part[part_idx];
    struct gpt_part_hdr *prev_part = NULL;
    
    if (part_idx == 0) {
        part->first_lba = _gpt1.hdr.first_lba;
    } else {
        prev_part = &_gpt1.part[part_idx-1];
        part->first_lba = prev_part->last_lba+1;
    }

    part->last_lba = part->first_lba + no_of_blocks;
    
    if (strlen(part_name) > (sizeof(part->name)/2))
        return PB_ERR;

    unsigned char *part_name_ptr = part->name;
    for (unsigned int i = 0; i < strlen(part_name); i++) 
    {
        *part_name_ptr++ = part_name[i];
        *part_name_ptr++ = 0;
    }

    for (int i = 0; i < 16; i++) 
        part->uuid[i] = gpt_prng();
    
    memcpy(part->type_uuid, type_uuid, 16);

    return PB_OK;
}

uint32_t gpt_write_tbl(void) 
{
    uint32_t crc_tmp = 0;
    uint32_t err = 0;
    uint32_t part_tbl_blocks = 0;

    /* Calculate CRC32 for header and part table */
    _gpt1.hdr.hdr_crc = 0;
    _gpt1.hdr.part_array_crc = crc32(0, (uint8_t *) &_gpt1.part, 
                sizeof(struct gpt_part_hdr) * _gpt1.hdr.no_of_parts);


    crc_tmp  = crc32(0, (uint8_t*) &_gpt1.hdr, sizeof(struct gpt_header)
                            - GPT_HEADER_RSZ);
    _gpt1.hdr.hdr_crc = crc_tmp;

    /* TODO: Maybe do the GPT table writes the same way for clarity */
    /* Write primary GPT Table */
    err = plat_emmc_write_block(_gpt1.hdr.current_lba,(uint8_t*) &_gpt1, 
                    sizeof(struct gpt_primary_tbl) / 512);

    if (err != PB_OK) 
    {
        LOG_ERR ("Error writing primary GPT table");
        return PB_ERR;
    }

    /* Write backup GPT table */

    err = plat_emmc_write_block(_gpt1.hdr.backup_lba, (uint8_t *) &_gpt1.hdr, 1);

    if (err != PB_OK) 
    {
        LOG_ERR ("Error writing backup GPT table");
        return PB_ERR;
    }

    part_tbl_blocks = (_gpt1.hdr.no_of_parts * _gpt1.hdr.part_entry_sz) / 512;

    err = plat_emmc_write_block(_gpt1.hdr.backup_lba-part_tbl_blocks, 
                        (uint8_t *) &_gpt1.part, part_tbl_blocks);

    if (err != PB_OK) 
    {
        LOG_ERR ("Error writing backup GPT part table");
        return PB_ERR;
    }

    return PB_OK;
}


uint32_t gpt_init(void) 
{
    plat_emmc_read_block(1,(uint8_t*) &_gpt1, 
                    sizeof(struct gpt_primary_tbl) / 512);

    uint8_t tmp_string[64];

    LOG_INFO("Init...");

    for (uint32_t i = 0; i < _gpt1.hdr.no_of_parts; i++) 
    {

        if (_gpt1.part[i].first_lba == 0)
            break;

        gpt_part_name(&_gpt1.part[i], tmp_string, sizeof(tmp_string));

        LOG_INFO2 (" %lu - [%16s] lba 0x%8.8lX%8.8lX - 0x%8.8lX%8.8lX\n\r",
                        i,
                        tmp_string,
                        (uint32_t) (_gpt1.part[i].first_lba >> 32) & 0xFFFFFFFF,
                        (uint32_t) _gpt1.part[i].first_lba & 0xFFFFFFFF,
                        (uint32_t) (_gpt1.part[i].last_lba >> 32) & 0xFFFFFFFF,
                        (uint32_t) _gpt1.part[i].last_lba & 0xFFFFFFFF);
    
        
    }

    /* TODO: Implement logic to recover faulty primare GPT table */
    if (_gpt1.hdr.signature != __gpt_header_signature) 
    {
        LOG_ERR ("Invalid header signature");
        _flag_gpt_ok = false;
        return PB_ERR;
    }

    uint32_t crc_tmp = _gpt1.hdr.hdr_crc;
    _gpt1.hdr.hdr_crc = 0;

    if (crc32(0, (uint8_t*) &_gpt1.hdr, sizeof(struct gpt_header) - 
                                                GPT_HEADER_RSZ) != crc_tmp) 
    {
        LOG_ERR ("Header CRC Error");
        _flag_gpt_ok = false;
        return PB_ERR;
    }

    crc_tmp = _gpt1.hdr.part_array_crc;

    if (crc32(0, (uint8_t *) &_gpt1.part, sizeof(struct gpt_part_hdr) *
                        _gpt1.hdr.no_of_parts) != crc_tmp) 
    {
        LOG_ERR ("Partition array CRC error");
        _flag_gpt_ok = false;
        return PB_ERR;
    }

    LOG_INFO ("GPT crc = 0x%8.8lX", crc_tmp);

    _flag_gpt_ok = true;
    return PB_OK;

}
