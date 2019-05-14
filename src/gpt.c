/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <gpt.h>
#include <uuid.h>
#include <plat.h>
#include <crc.h>
#include <string.h>

static __a4k __no_bss uint8_t pmbr[512];

static inline uint32_t efi_crc32(const void *buf, uint32_t sz)
{
	return (crc32(~0L, buf, sz) ^ ~0L);
}

static void gpt_part_name(struct gpt_part_hdr *part, uint8_t *out, uint8_t len)
{
    uint8_t null_count = 0;

    for (int i = 0; i < len*2; i++)
    {
        if (part->name[i])
        {
            *out++ = part->name[i];
            null_count = 0;
        }
        else
        {
            null_count++;
        }

        *out = 0;

        if (null_count > 1)
            break;
    }
}

uint32_t gpt_get_part_first_lba(struct gpt *gpt, uint8_t part_no)
{
    return (gpt->primary.part[part_no & 0x1f].first_lba);
}

uint64_t gpt_get_part_last_lba(struct gpt *gpt, uint8_t part_no)
{
    return (gpt->primary.part[part_no & 0x1f].last_lba);
}

uint32_t gpt_get_part_by_uuid(struct gpt *gpt, const char *uuid,
                              struct gpt_part_hdr **part)
{

    unsigned char guid[16];
    uuid_to_guid((uint8_t *) uuid, guid);

    for (unsigned int i = 0; i < gpt->primary.hdr.no_of_parts; i++) {
        if (gpt->primary.part[i].first_lba == 0)
            return PB_ERR;

        if (memcmp((void *) gpt->primary.part[i].uuid, guid, 16) == 0)
        {
            (*part) = &gpt->primary.part[i];
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

uint32_t gpt_init_tbl(struct gpt *gpt, uint64_t first_lba, uint64_t last_lba)
{
    struct gpt_header *hdr = &gpt->primary.hdr;

    prng_state = plat_get_us_tick();

    memset((uint8_t *) &gpt->primary, 0, sizeof(struct gpt_primary_tbl));
    memset((uint8_t *) &gpt->backup, 0, sizeof(struct gpt_primary_tbl));

    hdr->signature = __gpt_header_signature;
    hdr->rev = 0x00010000;
    hdr->hdr_sz = sizeof(struct gpt_header) - GPT_HEADER_RSZ;
    hdr->current_lba = first_lba;
    hdr->no_of_parts = 128;

    hdr->first_lba = (hdr->current_lba + 1
            + (hdr->no_of_parts*sizeof(struct gpt_part_hdr)) / 512);
    hdr->backup_lba = last_lba;
    hdr->last_lba = (last_lba - 1 -
                (hdr->no_of_parts*sizeof(struct gpt_part_hdr)) / 512);
    hdr->entries_start_lba = (gpt->primary.hdr.current_lba + 1);
    hdr->part_entry_sz = sizeof(struct gpt_part_hdr);

    for (int i = 0; i < 16; i++)
        hdr->disk_uuid[i] = gpt_prng();


    return PB_OK;
}

uint32_t gpt_add_part(struct gpt *gpt, uint8_t part_idx, uint32_t no_of_blocks,
                      const char *type_uuid, const char *part_name)
{

    struct gpt_part_hdr *part = &gpt->primary.part[part_idx];
    struct gpt_part_hdr *prev_part = NULL;

    memset(part, 0, sizeof(struct gpt_part_hdr));

    if (part_idx == 0) {
        part->first_lba = gpt->primary.hdr.first_lba;
    } else {
        prev_part = &gpt->primary.part[part_idx-1];
        part->first_lba = prev_part->last_lba+1;
    }

    part->last_lba = part->first_lba + no_of_blocks;

    if (strlen(part_name) > GPT_PART_NAME_MAX_SIZE)
        return PB_ERR;

    unsigned char *part_name_ptr = part->name;
    for (unsigned int i = 0; i < strlen(part_name); i++)
    {
        *part_name_ptr++ = part_name[i];
        *part_name_ptr++ = 0;
    }

    part->attr[7] = 0x80; /* Not bootable */
    part->attr[6] = PB_GPT_ATTR_OK;

    memcpy(part->type_uuid, type_uuid, 16);
    memcpy(part->uuid, type_uuid, 16);
    memcpy(&gpt->backup.part[part_idx], part, sizeof(struct gpt_part_hdr));

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

        LOG_INFO2 (" %u - [%s] lba 0x%016llx" \
                    " - 0x%016llx\n\r",
                        i,
                        tmp_string,
                        part[i].first_lba,
                        part[i].last_lba);
    }

    crc_tmp = hdr->part_array_crc;


    LOG_INFO("Stored CRC: %x, calc: %x",
        crc_tmp,
        efi_crc32((uint8_t *) part, sizeof(struct gpt_part_hdr) *
                        hdr->no_of_parts));
    if (efi_crc32((uint8_t *) part, sizeof(struct gpt_part_hdr) *
                        hdr->no_of_parts) != crc_tmp)
    {
        LOG_ERR ("Partition array CRC error");
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

uint32_t gpt_write_tbl(struct gpt *gpt)
{
    uint32_t crc_tmp = 0;
    uint32_t err = 0;

    /* Calculate CRC32 for header and part table */
    gpt->primary.hdr.hdr_crc = 0;
    gpt->primary.hdr.part_array_crc = efi_crc32((uint8_t *) gpt->primary.part,
                (sizeof(struct gpt_part_hdr) * gpt->primary.hdr.no_of_parts));

    crc_tmp  = efi_crc32((uint8_t*) &gpt->primary.hdr,
                    (sizeof(struct gpt_header) - GPT_HEADER_RSZ));

    gpt->primary.hdr.hdr_crc = crc_tmp;

    /* Write protective MBR */

    memset(pmbr, 0, sizeof(pmbr));

    pmbr[448] = 2;
    pmbr[449] = 0;
    pmbr[450] = 0xEE; // type , 0xEE = GPT
    pmbr[451] = 0xFF;
    pmbr[452] = 0xFF;
    pmbr[453] = 0xFF;
    pmbr[454] = 0x01;

    uint32_t llba = (uint32_t) plat_get_lastlba();
    memcpy(&pmbr[458], &llba, sizeof(uint32_t));
    pmbr[510] = 0x55;
    pmbr[511] = 0xAA;


    LOG_INFO("writing protective MBR");
    err = plat_write_block(0,(uintptr_t) pmbr, 1);

    if (err != PB_OK)
    {
        LOG_ERR ("error writing protective MBR");
        return PB_ERR;
    }

    /* Write primary GPT Table */

    LOG_INFO("writing primary gpt tbl to lba %llu",
                            gpt->primary.hdr.current_lba);
    err = plat_write_block(gpt->primary.hdr.current_lba,
                           (uintptr_t) &gpt->primary,
                           (sizeof(struct gpt_primary_tbl) / 512));

    if (err != PB_OK)
    {
        LOG_ERR ("error writing primary gpt table");
        return PB_ERR;
    }

    /* Configure backup GPT table */
    memcpy(&gpt->backup.hdr, &gpt->primary.hdr, sizeof(struct gpt_header));
    memcpy(gpt->backup.part, gpt->primary.part, (sizeof(struct gpt_part_hdr)
                                    * gpt->primary.hdr.no_of_parts));

    uint64_t last_lba = plat_get_lastlba();

    struct gpt_header *hdr = (&gpt->backup.hdr);

    hdr->backup_lba = gpt->primary.hdr.current_lba;
    hdr->current_lba = last_lba;
    hdr->entries_start_lba = (last_lba -
                ((hdr->no_of_parts*sizeof(struct gpt_part_hdr)) / 512));

    hdr->hdr_crc = 0;
    hdr->part_array_crc = efi_crc32((uint8_t *) gpt->backup.part,
                sizeof(struct gpt_part_hdr) * gpt->backup.hdr.no_of_parts);

    crc_tmp  = efi_crc32((uint8_t*) hdr, sizeof(struct gpt_header)
                            - GPT_HEADER_RSZ);
    gpt->backup.hdr.hdr_crc = crc_tmp;

    LOG_DBG("no_of_parts %u",gpt->backup.hdr.no_of_parts);
    /* Write backup GPT table */

    LOG_INFO("Writing backup GPT tbl to LBA %llu",
                        gpt->backup.hdr.entries_start_lba);

    err = plat_write_block(gpt->backup.hdr.entries_start_lba,
                            (uintptr_t) &gpt->backup,
                            (sizeof(struct gpt_backup_tbl) / 512));

    if (err != PB_OK)
    {
        LOG_ERR ("Error writing backup GPT table");
        return PB_ERR;
    }

    return PB_OK;
}



uint32_t gpt_init(struct gpt *gpt)
{

    LOG_DBG("GPT ptr %p", gpt);
    plat_read_block(1,(uintptr_t) &gpt->primary,
                    sizeof(struct gpt_primary_tbl) / 512);

    if (gpt_is_valid(&gpt->primary.hdr, gpt->primary.part) != PB_OK)
    {
        LOG_ERR("Primary GPT table is corrupt or missing, trying to recover backup");

        plat_read_block(plat_get_lastlba(),
                        (uintptr_t) &gpt->backup.hdr,
                        sizeof(struct gpt_header) / 512);

        if (gpt_has_valid_header(&gpt->backup.hdr) != PB_OK)
        {
            LOG_ERR("Invalid backup GPT header, unable to recover");
            return PB_ERR;
        }

        plat_read_block(gpt->backup.hdr.entries_start_lba,
            (uintptr_t) gpt->backup.part,
            (gpt->backup.hdr.no_of_parts * sizeof(struct gpt_part_hdr)) / 512);

        if (gpt_has_valid_part_array(&gpt->backup.hdr, gpt->backup.part) != PB_OK)
        {
            LOG_ERR("Invalid backup GPT part array, unable to recover");
            return PB_ERR;
        }

        LOG_ERR ("Recovering from backup GPT tables");

        memcpy(&gpt->primary.hdr, &gpt->backup.hdr, sizeof(struct gpt_header));
        memcpy(gpt->primary.part, gpt->backup.part, (sizeof(struct gpt_part_hdr)
                    * gpt->backup.hdr.no_of_parts));

        gpt->primary.hdr.backup_lba = plat_get_lastlba();
        gpt->primary.hdr.current_lba = 1;
        gpt->primary.hdr.entries_start_lba = (gpt->primary.hdr.current_lba + 1);

        LOG_DBG("no_of_parts = %u", gpt->primary.hdr.no_of_parts);

        if (gpt_write_tbl(gpt) != PB_OK)
        {
            LOG_ERR("Could not update primary GPT table, unable to recover");
            return PB_ERR;
        }
    }
    else
    {
        /* TODO: Primary GPT OK, check backup GPT */
    }

    return PB_OK;
}

uint32_t gpt_pb_attr_setbits(struct gpt_part_hdr *part, uint8_t attr)
{
    if (part == NULL)
        return PB_ERR;

    part->attr[6] |= attr;

    return PB_OK;
}

uint32_t gpt_pb_attr_clrbits(struct gpt_part_hdr *part, uint8_t attr)
{
    if (part == NULL)
        return PB_ERR;

    part->attr[6] &= (~attr);

    return PB_OK;
}

bool gpt_pb_attr_ok(struct gpt_part_hdr *part)
{
    if (part == NULL)
        return false;

    return ((part->attr[6] & PB_GPT_ATTR_OK) == PB_GPT_ATTR_OK);
}

uint8_t gpt_pb_attr_counter(struct gpt_part_hdr *part)
{
    if (part == NULL)
        return 0;
    return (part->attr[6] & 0x0f);
}

uint32_t gpt_part_set_bootable(struct gpt_part_hdr *part, bool bootable)
{
    if(part == NULL)
        return PB_ERR;

    if (bootable)
        part->attr[7] &= ~0x80;
    else
        part->attr[7] |= 0x80;

    return PB_OK;
}

bool gpt_part_is_bootable(struct gpt_part_hdr *part)
{
    if(part == NULL)
        return false;

    return ((part->attr[7] & 0x80) != 0x80);
}
