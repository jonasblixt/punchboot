/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <pb/arch.h>
#include <pb/gpt.h>
#include <pb/plat.h>
#include <pb/crc.h>
#include <uuid/uuid.h>
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
} __attribute__((packed));

struct gpt_part_hdr
{
    uint8_t type_uuid[16];
    uint8_t uuid[16];
    uint64_t first_lba;
    uint64_t last_lba;
    uint8_t attr[8];
    uint8_t name[GPT_PART_NAME_MAX_SIZE*2];
} __attribute__((packed));


struct gpt_primary_tbl
{
    struct gpt_header hdr;
    struct gpt_part_hdr part[128];
} __attribute__((packed));

struct gpt_backup_tbl
{
    struct gpt_part_hdr part[128];
    struct gpt_header hdr;
} __attribute__((packed));

#define PB_GPT_ATTR_OK       (1 << 7) /*Bit 55*/

static const uint64_t __gpt_header_signature = 0x5452415020494645ULL;
static int prng_state;

struct gpt_private
{
    struct gpt_primary_tbl primary;
    struct gpt_backup_tbl backup;
    uint8_t pmbr[512];
};

#define PB_GPT_PRIV(_drv) ((struct gpt_private *) drv->map_private)

static inline uint32_t efi_crc32(const void *buf, uint32_t sz)
{
    return (crc32(~0L, buf, sz) ^ ~0L);
}

/* Low entropy source for setting up UUID's */
static int gpt_prng(void)
{
    int x = prng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    prng_state = x;

    return x;
}

static int gpt_has_valid_header(struct gpt_header *hdr)
{
    if (hdr->signature != __gpt_header_signature)
    {
        LOG_ERR("Invalid header signature");
        return PB_ERR;
    }

    uint32_t crc_tmp = hdr->hdr_crc;
    hdr->hdr_crc = 0;

    if (efi_crc32((uint8_t*) hdr, sizeof(struct gpt_header) -
                                   GPT_HEADER_RSZ) != crc_tmp)
    {
        LOG_ERR("Header CRC Error");
        return PB_ERR;
    }

    hdr->hdr_crc = crc_tmp;

    return PB_OK;
}

static int gpt_has_valid_part_array(struct gpt_header *hdr,
                                    struct gpt_part_hdr *part)
{
    uint32_t crc_tmp = hdr->part_array_crc;

    if (efi_crc32((uint8_t *) part, sizeof(struct gpt_part_hdr) *
                        hdr->no_of_parts) != crc_tmp)
    {
        LOG_ERR("Partition array CRC error");
        return PB_ERR;
    }


    hdr->part_array_crc = crc_tmp;

    return PB_OK;
}

static int gpt_is_valid(struct gpt_header *hdr, struct gpt_part_hdr *part)
{
    if (gpt_has_valid_header(hdr) != PB_OK)
        return PB_ERR;

    if (gpt_has_valid_part_array(hdr, part) != PB_OK)
        return PB_ERR;

    return PB_OK;
}

static int gpt_init_tbl(struct pb_storage_driver *drv)
{
    struct gpt_private *priv = PB_GPT_PRIV(drv);
    struct gpt_header *hdr = &priv->primary.hdr;

    prng_state = arch_get_us_tick();

    LOG_INFO("Initializing table at lba 1, last lba: %zu",
                    pb_storage_last_block(drv));

    memset((uint8_t *) &priv->primary, 0, sizeof(priv->primary));
    memset((uint8_t *) &priv->backup, 0, sizeof(priv->backup));

    hdr->signature = __gpt_header_signature;
    hdr->rev = 0x00010000;
    hdr->hdr_sz = sizeof(struct gpt_header) - GPT_HEADER_RSZ;
    hdr->current_lba = 1;
    hdr->no_of_parts = 128;

    hdr->first_lba = (hdr->current_lba + 1
            + (hdr->no_of_parts*sizeof(struct gpt_part_hdr)) / 512);
    hdr->backup_lba = pb_storage_last_block(drv);
    hdr->last_lba = (pb_storage_last_block(drv) - 1 -
                (hdr->no_of_parts*sizeof(struct gpt_part_hdr)) / 512);
    hdr->entries_start_lba = (priv->primary.hdr.current_lba + 1);
    hdr->part_entry_sz = sizeof(struct gpt_part_hdr);

    for (uint32_t i = 0; i < membersof(hdr->disk_uuid); i++)
        hdr->disk_uuid[i] = gpt_prng();


    return PB_OK;
}

static int gpt_add_part(struct pb_storage_driver *drv,
                        int part_idx, unsigned int no_of_blocks,
                        const uuid_t type_uuid, const char *part_name)
{
    struct gpt_private *priv = PB_GPT_PRIV(drv);
    struct gpt_part_hdr *part = &priv->primary.part[part_idx];
    struct gpt_part_hdr *prev_part = NULL;

    memset(part, 0, sizeof(struct gpt_part_hdr));

    if (part_idx == 0)
    {
        part->first_lba = priv->primary.hdr.first_lba;
    }
    else
    {
        prev_part = &priv->primary.part[part_idx-1];
        part->first_lba = prev_part->last_lba+1;
    }

    part->last_lba = part->first_lba + no_of_blocks - 1;

    if (strlen(part_name) > GPT_PART_NAME_MAX_SIZE)
        return -PB_ERR;

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
    memcpy(&priv->backup.part[part_idx], part, sizeof(struct gpt_part_hdr));

    return PB_OK;
}



static int gpt_write_tbl(struct pb_storage_driver *drv)
{
    struct gpt_private *priv = PB_GPT_PRIV(drv);
    uint32_t crc_tmp = 0;
    uint32_t err = 0;

    /* Calculate CRC32 for header and part table */
    priv->primary.hdr.hdr_crc = 0;
    priv->primary.hdr.part_array_crc = \
        efi_crc32((uint8_t *) priv->primary.part,
           (sizeof(struct gpt_part_hdr) * priv->primary.hdr.no_of_parts));

    crc_tmp  = efi_crc32((uint8_t*) &priv->primary.hdr,
                    (sizeof(struct gpt_header) - GPT_HEADER_RSZ));

    priv->primary.hdr.hdr_crc = crc_tmp;


    /* Write protective MBR */
    memset(priv->pmbr, 0, sizeof(priv->pmbr));

    priv->pmbr[448] = 2;
    priv->pmbr[449] = 0;
    priv->pmbr[450] = 0xEE;  // type , 0xEE = GPT
    priv->pmbr[451] = 0xFF;
    priv->pmbr[452] = 0xFF;
    priv->pmbr[453] = 0xFF;
    priv->pmbr[454] = 0x01;

    uint32_t llba = (uint32_t) pb_storage_blocks(drv);
    memcpy(&priv->pmbr[458], &llba, sizeof(uint32_t));
    priv->pmbr[510] = 0x55;
    priv->pmbr[511] = 0xAA;

    LOG_INFO("writing protective MBR");
    err = drv->write(drv, 0, priv->pmbr, 1);

    if (err != PB_OK)
    {
        LOG_ERR("error writing protective MBR");
        return PB_ERR;
    }
    /* Write primary GPT Table */

    LOG_INFO("writing primary gpt tbl to lba %llu",
                           priv->primary.hdr.current_lba);
    err = drv->write(drv, priv->primary.hdr.current_lba,
                           &priv->primary,
                           (sizeof(priv->primary) / 512));

    if (err != PB_OK)
    {
        LOG_ERR("error writing primary gpt table");
        return PB_ERR;
    }

    /* Configure backup GPT table */
    memcpy(&priv->backup.hdr, &priv->primary.hdr, sizeof(struct gpt_header));
    memcpy(priv->backup.part, priv->primary.part, (sizeof(struct gpt_part_hdr)
                                    * priv->primary.hdr.no_of_parts));

    uint64_t last_lba = pb_storage_last_block(drv);

    struct gpt_header *hdr = (&priv->backup.hdr);

    hdr->backup_lba = priv->primary.hdr.current_lba;
    hdr->current_lba = last_lba;
    hdr->entries_start_lba = (last_lba -
                ((hdr->no_of_parts*sizeof(struct gpt_part_hdr)) / 512));

    hdr->hdr_crc = 0;
    hdr->part_array_crc = efi_crc32((uint8_t *) priv->backup.part,
                sizeof(struct gpt_part_hdr) * priv->backup.hdr.no_of_parts);

    crc_tmp  = efi_crc32((uint8_t*) hdr, sizeof(struct gpt_header)
                            - GPT_HEADER_RSZ);
    priv->backup.hdr.hdr_crc = crc_tmp;

    /* Write backup GPT table */
    LOG_INFO("Writing backup GPT tbl to LBA %llu",
                        priv->backup.hdr.entries_start_lba);

    err = drv->write(drv, priv->backup.hdr.entries_start_lba,
                            &priv->backup,
                            (sizeof(priv->backup) / 512));


    return err;
}

int gpt_init(struct pb_storage_driver *drv)
{
    struct gpt_private *priv = PB_GPT_PRIV(drv);

    LOG_DBG("GPT MAP init");

    /* Read primary and backup GPT headers and parition tables */
    drv->read(drv, 1, &priv->primary, (sizeof(priv->primary) / 512));

    size_t backup_lba = pb_storage_last_block(drv) - \
            ((128*sizeof(struct gpt_part_hdr)) / 512);

    drv->read(drv, backup_lba, &priv->backup, (sizeof(priv->backup) / 512));

    if (gpt_is_valid(&priv->primary.hdr, priv->primary.part) != PB_OK)
    {
        LOG_ERR("Primary GPT table is corrupt or missing," \
                            " trying to recover backup");

        if (gpt_has_valid_header(&priv->backup.hdr) != PB_OK)
        {
            LOG_ERR("Invalid backup GPT header, unable to recover");
            return PB_ERR;
        }

        if (gpt_has_valid_part_array(&priv->backup.hdr, priv->backup.part) != PB_OK)
        {
            LOG_ERR("Invalid backup GPT part array, unable to recover");
            return PB_ERR;
        }

        LOG_ERR("Recovering from backup GPT tables");

        memcpy(&priv->primary.hdr, &priv->backup.hdr, sizeof(struct gpt_header));
        memcpy(priv->primary.part, priv->backup.part, (sizeof(struct gpt_part_hdr)
                    * priv->backup.hdr.no_of_parts));

        priv->primary.hdr.backup_lba = pb_storage_last_block(drv);
        priv->primary.hdr.current_lba = 1;
        priv->primary.hdr.entries_start_lba = (priv->primary.hdr.current_lba + 1);

        LOG_DBG("no_of_parts = %u", priv->primary.hdr.no_of_parts);

        if (gpt_write_tbl(drv) != PB_OK)
        {
            LOG_ERR("Could not update primary GPT table, unable to recover");
            return PB_ERR;
        }
    }


    if (gpt_is_valid(&priv->backup.hdr, priv->backup.part) != PB_OK)
    {
        LOG_ERR("Backup GPT table is corrupt or missing," \
                " trying to recover from primary");

        if (gpt_has_valid_header(&priv->primary.hdr) != PB_OK)
        {
            LOG_ERR("Invalid primary GPT header, unable to recover");
            return PB_ERR;
        }

        if (gpt_has_valid_part_array(&priv->primary.hdr,
                                     priv->primary.part) != PB_OK)
        {
            LOG_ERR("Invalid primary GPT part array, unable to recover");
            return PB_ERR;
        }

        LOG_ERR("Recovering from primary GPT tables");

        memcpy(&priv->backup.hdr, &priv->primary.hdr, sizeof(struct gpt_header));
        memcpy(priv->backup.part, priv->primary.part, (sizeof(struct gpt_part_hdr)
                    * priv->primary.hdr.no_of_parts));

        priv->backup.hdr.backup_lba = 1;
        priv->backup.hdr.current_lba = pb_storage_blocks(drv);
        priv->backup.hdr.entries_start_lba = (priv->backup.hdr.current_lba -
            ((priv->backup.hdr.no_of_parts*sizeof(struct gpt_part_hdr)) / 512));

        if (gpt_write_tbl(drv) != PB_OK)
        {
            LOG_ERR("Could not update GPT table, unable to recover");
            return PB_ERR;
        }
    }

    uint8_t uuid[16];
    char name[37];
    char uuid_str[37];

#if (LOGLEVEL > 1)

    for (struct gpt_part_hdr *p = priv->primary.part; p->first_lba; p++)
    {
        uuid_to_guid(p->uuid, uuid);
        uuid_unparse(uuid, uuid_str);

        int n = 0;
        for (int i = 0; i < 36; i++)
        {
            name[i] = p->name[n];
            n += 2;
        }

        name[36] = 0;

        LOG_DBG("%s: %s, %llu -> %llu", uuid_str, name,
                                      p->first_lba, p->last_lba);
    }

#endif

    /* Translate to internal map format */

    struct pb_storage_map *entries = (struct pb_storage_map *) drv->map_data;

    for (struct gpt_part_hdr *p = priv->primary.part; p->first_lba; p++)
    {
        struct pb_storage_map *part = NULL;
        uuid_to_guid(p->uuid, uuid);
        uuid_unparse(uuid, uuid_str);

        pb_storage_foreach_part(drv->map_default, dp)
        {
            if (!dp->valid_entry)
                break;

            if (memcmp(dp->uuid_str, uuid_str, 36) == 0)
            {
                part = dp;
                break;
            }
        }

        if (!part)
            continue;

        int n = 0;
        for (int i = 0; i < 36; i++)
        {
            name[i] = p->name[n];
            n += 2;
        }

        name[36] = 0;

        LOG_DBG("Copy %s", name);

        struct pb_storage_map *entry = &entries[drv->map_entries++];

        entry->valid_entry = true;
        memcpy(entry->uuid, uuid, 16);
        memcpy(entry->description, name, sizeof(entry->description));
        entry->first_block = p->first_lba;
        entry->last_block = p->last_lba;
        entry->no_of_blocks = p->last_lba - p->first_lba + 1;
        entry->flags = part->flags;
    }

    memset(&entries[drv->map_entries], 0, sizeof(*entries));

    LOG_DBG("Loaded %i entries", drv->map_entries);

    return PB_OK;
}

int gpt_install_map(struct pb_storage_driver *drv,
                            struct pb_storage_map *map)
{
    int rc;
    int part_count = 0;
    struct pb_storage_map *map_p = map;
    uuid_t part_uuid;
    uuid_t part_guid;

    rc = gpt_init_tbl(drv);

    if (rc != PB_OK)
        return rc;

    while (map_p->valid_entry)
    {
        if (map_p->flags & PB_STORAGE_MAP_FLAG_STATIC_MAP)
        {
            map_p++;
            continue;
        }

        LOG_DBG("Add: %s", map_p->description);
        uuid_parse(map_p->uuid_str, part_uuid);
        uuid_to_guid(part_uuid, part_guid);
        rc = gpt_add_part(drv, part_count++, map_p->no_of_blocks,
                                part_guid, map_p->description);

        if (rc != PB_OK)
            break;

        map_p++;
    }

    return gpt_write_tbl(drv);
}

int gpt_resize_map(struct pb_storage_driver *drv, struct pb_storage_map *map,
                    size_t blocks)
{
    uint8_t uuid[16];
    char uuid_str[37];
    ssize_t new_offset = 0;
    size_t old_size = 0;
    bool found_part = false;
    struct gpt_private *priv = PB_GPT_PRIV(drv);

    uuid_unparse(map->uuid, uuid_str);
    LOG_DBG("Resizing partition %s to %zu blocks", uuid_str, blocks);

    for (struct gpt_part_hdr *p = priv->primary.part; p->first_lba; p++)
    {
        uuid_to_guid(p->uuid, uuid);

        if (memcmp(uuid, map->uuid, 16) == 0) {
            LOG_DBG("Found partition:");
            found_part = true;
            old_size = p->last_lba - p->first_lba + 1;
            new_offset = blocks - old_size;
            p->last_lba += new_offset;
            LOG_DBG("  new first lba = %llu, last lba = %llu", p->first_lba,
                                                               p->last_lba);
        } else if (found_part) {
            uuid_unparse(uuid, uuid_str);
            LOG_DBG("Adjusting offset for %s with %zd blocks", uuid_str,
                                                               new_offset);
            LOG_DBG("  old first lba = %llu, last lba = %llu", p->first_lba,
                                                               p->last_lba);
            p->first_lba += new_offset;
            p->last_lba += new_offset;

            LOG_DBG("  new first lba = %llu, last lba = %llu", p->first_lba,
                                                               p->last_lba);
        }
    }

    if (!found_part) {
        return -PB_ERR;
    }

    return gpt_write_tbl(drv);
}
