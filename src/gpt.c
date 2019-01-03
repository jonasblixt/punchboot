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
#include <string.h>
#include <uuid.h>

static uint8_t _flag_gpt_ok = false;
__a4k __no_bss static struct gpt_primary_tbl _gpt1;
__a4k __no_bss static struct gpt_backup_tbl _gpt2;

static inline uint32_t efi_crc32(const void *buf, uint32_t sz)
{
	return (crc32(~0L, buf, sz) ^ ~0L);
}

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

uint32_t gpt_get_part_first_lba(uint8_t part_no) 
{
    return _gpt1.part[part_no & 0x1f].first_lba;
}

uint64_t gpt_get_part_last_lba(uint8_t part_no)
{
    return _gpt1.part[part_no & 0x1f].last_lba;
}

uint32_t gpt_get_part_by_uuid(const uint8_t *uuid, uint32_t *lba_offset) 
{
    if (!_flag_gpt_ok)
        return PB_ERR;

    for (unsigned int i = 0; i < _gpt1.hdr.no_of_parts; i++) {
        if (_gpt1.part[i].first_lba == 0)
            return PB_ERR;

        if (memcmp(_gpt1.part[i].uuid, uuid, 16) == 0)
        {
            *lba_offset = _gpt1.part[i].first_lba;
            return PB_OK;
        }
    }
    return PB_ERR;
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

uint32_t gpt_init_tbl(uint64_t first_lba, uint64_t last_lba) 
{
    struct gpt_header *hdr = &_gpt1.hdr;
    
    prng_state = plat_get_us_tick();

    memset((uint8_t *) &_gpt1, 0, sizeof(struct gpt_primary_tbl));

    hdr->signature = __gpt_header_signature;
    hdr->rev = 0x00010000;
    hdr->hdr_sz = sizeof(struct gpt_header) - GPT_HEADER_RSZ;
    hdr->current_lba = first_lba;
    hdr->no_of_parts = 128;
    
    hdr->first_lba = hdr->current_lba + 1 
            + (hdr->no_of_parts*sizeof(struct gpt_part_hdr)) / 512;
    hdr->backup_lba = last_lba;
    hdr->last_lba = last_lba - 1 - 
                (hdr->no_of_parts*sizeof(struct gpt_part_hdr)) / 512;
    hdr->entries_start_lba = _gpt1.hdr.current_lba + 1;
    hdr->part_entry_sz = sizeof(struct gpt_part_hdr);
    

    for (int i = 0; i < 16; i++) 
        hdr->disk_uuid[i] = gpt_prng();
 
    /* Configure backup GPT table */
    memcpy(&_gpt2.hdr, hdr, sizeof(struct gpt_header));
    
    hdr = &_gpt2.hdr;
    hdr->backup_lba = _gpt1.hdr.current_lba;
    hdr->current_lba = last_lba;
    hdr->entries_start_lba = last_lba - 
                (hdr->no_of_parts*sizeof(struct gpt_part_hdr)) / 512;

    return PB_OK;
}

uint32_t gpt_add_part(uint8_t part_idx, uint32_t no_of_blocks, 
                      const uint8_t *type_uuid, const char *part_name) 
{

    struct gpt_part_hdr *part = &_gpt1.part[part_idx];
    struct gpt_part_hdr *prev_part = NULL;
    
    memset(part, 0, sizeof(struct gpt_part_hdr));

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

    //for (int i = 0; i < 16; i++) 
    //    part->uuid[i] = gpt_prng();
    
    memcpy(part->type_uuid, type_uuid, 16);
    memcpy(part->uuid, type_uuid, 16);
    memcpy(&_gpt2.part[part_idx], part, sizeof(struct gpt_part_hdr));

    return PB_OK;
}

uint32_t gpt_write_tbl(void) 
{
    uint32_t crc_tmp = 0;
    uint32_t err = 0;

    /* Calculate CRC32 for header and part table */
    _gpt1.hdr.hdr_crc = 0;
    _gpt1.hdr.part_array_crc = efi_crc32((uint8_t *) _gpt1.part, 
                sizeof(struct gpt_part_hdr) * _gpt1.hdr.no_of_parts);

    crc_tmp  = efi_crc32((uint8_t*) &_gpt1.hdr, sizeof(struct gpt_header)
                            - GPT_HEADER_RSZ);
    _gpt1.hdr.hdr_crc = crc_tmp;

    /* Write primary GPT Table */

    LOG_INFO("Writing primary GPT tbl to LBA %llu", _gpt1.hdr.current_lba);
    err = plat_write_block(_gpt1.hdr.current_lba,(uint8_t*) &_gpt1, 
                    sizeof(struct gpt_primary_tbl) / 512);

    if (err != PB_OK) 
    {
        LOG_ERR ("Error writing primary GPT table");
        return PB_ERR;
    }


    _gpt2.hdr.hdr_crc = 0;
    _gpt2.hdr.part_array_crc = efi_crc32((uint8_t *) _gpt2.part, 
                sizeof(struct gpt_part_hdr) * _gpt2.hdr.no_of_parts);

    crc_tmp  = efi_crc32((uint8_t*) &_gpt2.hdr, sizeof(struct gpt_header)
                            - GPT_HEADER_RSZ);
    _gpt2.hdr.hdr_crc = crc_tmp;

    /* Write backup GPT table */

    LOG_INFO("Writing backup GPT tbl to LBA %llu", _gpt2.hdr.entries_start_lba);
    err = plat_write_block(_gpt2.hdr.entries_start_lba, (uint8_t *) &_gpt2,
            sizeof(struct gpt_backup_tbl) / 512);

    if (err != PB_OK) 
    {
        LOG_ERR ("Error writing backup GPT table");
        return PB_ERR;
    }

    return PB_OK;
}


static uint32_t gpt_has_valid_header(struct gpt_header *hdr)
{

    if (hdr->signature != __gpt_header_signature) 
    {
        LOG_ERR ("Invalid header signature");
        return PB_ERR;
    }

    uint32_t crc_tmp = hdr->hdr_crc;
    hdr->hdr_crc = 0;

    if (efi_crc32((uint8_t*) hdr, sizeof(struct gpt_header) - 
                                   GPT_HEADER_RSZ) != crc_tmp) 
    {
        LOG_ERR ("Header CRC Error");
        return PB_ERR;
    }

    return PB_OK;
}

static uint32_t gpt_has_valid_part_array(struct gpt_header *hdr, 
                                    struct gpt_part_hdr *part)
{
    uint32_t crc_tmp = 0;
    uint8_t tmp_string[64];

    for (uint32_t i = 0; i < hdr->no_of_parts; i++) 
    {

        if (part[i].first_lba == 0)
            break;

        gpt_part_name(&part[i], tmp_string, sizeof(tmp_string));

        LOG_INFO2 (" %lu - [%16s] lba 0x%8.8lX%8.8lX - 0x%8.8lX%8.8lX\n\r",
                        i,
                        tmp_string,
                        (uint32_t) (part[i].first_lba >> 32) & 0xFFFFFFFF,
                        (uint32_t) part[i].first_lba & 0xFFFFFFFF,
                        (uint32_t) (part[i].last_lba >> 32) & 0xFFFFFFFF,
                        (uint32_t) part[i].last_lba & 0xFFFFFFFF);
    
     
        uuid_to_string(part[i].uuid, tmp_string);
        LOG_INFO(tmp_string);
    }

    crc_tmp = hdr->part_array_crc;

    if (efi_crc32((uint8_t *) part, sizeof(struct gpt_part_hdr) *
                        hdr->no_of_parts) != crc_tmp) 
    {
        LOG_ERR ("Partition array CRC error");
        _flag_gpt_ok = false;
        return PB_ERR;
    }

    return PB_OK;
}

static uint32_t gpt_is_valid(struct gpt_header *hdr, struct gpt_part_hdr *part)
{

    LOG_INFO("Init... ");


    if (gpt_has_valid_header(hdr) != PB_OK)
        return PB_ERR;

    if (gpt_has_valid_part_array(hdr, part) != PB_OK)
        return PB_ERR;

    return PB_OK;
}

uint32_t gpt_init(void) 
{
    _flag_gpt_ok = false;

    plat_read_block(1,(uint8_t*) &_gpt1, 
                    sizeof(struct gpt_primary_tbl) / 512);

    if (gpt_is_valid(&_gpt1.hdr, _gpt1.part) != PB_OK)
    {
        LOG_ERR("Primary GPT table is corrupt or missing, trying to recover backup");
        plat_read_block(plat_get_lastlba(), (uint8_t *) &_gpt2.hdr, 
                                sizeof(struct gpt_header) / 512);

        if (gpt_has_valid_header(&_gpt2.hdr) != PB_OK)
        {
            LOG_ERR("Invalid backup GPT header, unable to recover");
            return PB_ERR;
        }
        
        plat_read_block(_gpt2.hdr.entries_start_lba, (uint8_t *) _gpt2.part,
                    (_gpt2.hdr.no_of_parts * sizeof(struct gpt_part_hdr)) / 512);

        if (gpt_has_valid_part_array(&_gpt2.hdr, _gpt2.part) != PB_OK)
        {
            LOG_ERR("Invalid backup GPT part array, unable to recover");
            return PB_ERR;
        }

        LOG_ERR ("Recovering from backup GPT tables");

        memcpy(&_gpt1.hdr, &_gpt2.hdr, sizeof(struct gpt_header));
        memcpy(_gpt1.part, _gpt2.part, sizeof(struct gpt_part_hdr)
                    * _gpt2.hdr.no_of_parts);
        
        _gpt1.hdr.backup_lba = plat_get_lastlba();
        _gpt1.hdr.current_lba = 1;
        _gpt1.hdr.entries_start_lba = _gpt1.hdr.current_lba + 1;

        if (gpt_write_tbl() != PB_OK)
        {
            LOG_ERR("Could not update primary GPT table, unable to recover");
            return PB_ERR;
        }
    }

    _flag_gpt_ok = true;

    return PB_OK;
}
