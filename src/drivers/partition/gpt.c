/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/partition/gpt.h>
#include <inttypes.h>
#include <pb/crc.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <stdio.h>
#include <string.h>
#include <uuid.h>

#define GPT_HEADER_RSZ 420

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
} __attribute__((packed));

struct gpt_part_hdr {
    uint8_t type_uuid[16];
    uint8_t uuid[16];
    uint64_t first_lba;
    uint64_t last_lba;
    uint8_t attr[8];
    uint8_t name[GPT_PART_NAME_MAX_SIZE * 2];
} __attribute__((packed));

struct gpt_primary_tbl {
    struct gpt_header hdr;
    struct gpt_part_hdr part[128];
} __attribute__((packed));

struct gpt_backup_tbl {
    struct gpt_part_hdr part[128];
    struct gpt_header hdr;
} __attribute__((packed));

#define PB_GPT_ATTR_OK (1 << 7) /*Bit 55*/

static const uint64_t gpt_header_signature = 0x5452415020494645ULL;
static int prng_state;

static struct gpt_primary_tbl primary __aligned(64);
static struct gpt_backup_tbl backup __aligned(64);
static uint8_t gpt_pmbr[512];
static bio_dev_t gpt_dev;
static const struct gpt_table_list *tables;
static size_t tables_length;

static inline uint32_t efi_crc32(const void *buf, uint32_t sz)
{
    return (crc32(~0L, buf, sz) ^ ~0UL);
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
    if (hdr->signature != gpt_header_signature) {
        LOG_ERR("Invalid header signature");
        return PB_ERR;
    }

    uint32_t crc_tmp = hdr->hdr_crc;
    hdr->hdr_crc = 0;

    if (efi_crc32((uint8_t *)hdr, sizeof(struct gpt_header) - GPT_HEADER_RSZ) != crc_tmp) {
        LOG_ERR("Header CRC Error");
        return PB_ERR;
    }

    hdr->hdr_crc = crc_tmp;

    return PB_OK;
}

static int gpt_has_valid_part_array(struct gpt_header *hdr, struct gpt_part_hdr *part)
{
    uint32_t crc_tmp = hdr->part_array_crc;

    if (efi_crc32((uint8_t *)part, sizeof(struct gpt_part_hdr) * hdr->no_of_parts) != crc_tmp) {
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

static int gpt_init_tbl(bio_dev_t dev)
{
    struct gpt_header *hdr = &primary.hdr;
    unsigned int last_lba = bio_get_last_block(dev);
    prng_state = plat_get_us_tick();

    LOG_INFO("Initializing table at lba 1, last lba: %u", last_lba);

    memset((uint8_t *)&primary, 0, sizeof(primary));
    memset((uint8_t *)&backup, 0, sizeof(backup));

    hdr->signature = gpt_header_signature;
    hdr->rev = 0x00010000;
    hdr->hdr_sz = sizeof(struct gpt_header) - GPT_HEADER_RSZ;
    hdr->current_lba = 1;
    hdr->no_of_parts = 128;

    hdr->first_lba =
        (hdr->current_lba + 1 + (hdr->no_of_parts * sizeof(struct gpt_part_hdr)) / 512);
    hdr->backup_lba = last_lba;
    hdr->last_lba = (last_lba - (hdr->no_of_parts * sizeof(struct gpt_part_hdr)) / 512);
    hdr->entries_start_lba = primary.hdr.current_lba + 1;
    hdr->part_entry_sz = sizeof(struct gpt_part_hdr);

    for (uint32_t i = 0; i < membersof(hdr->disk_uuid); i++)
        hdr->disk_uuid[i] = gpt_prng();

    return PB_OK;
}

static int
gpt_add_part(int part_idx, unsigned int no_of_blocks, const uuid_t type_uuid, const char *part_name)
{
    struct gpt_part_hdr *part = &primary.part[part_idx];
    struct gpt_part_hdr *prev_part = NULL;

    memset(part, 0, sizeof(struct gpt_part_hdr));

    if (part_idx == 0) {
        part->first_lba = primary.hdr.first_lba;
    } else {
        prev_part = &primary.part[part_idx - 1];
        part->first_lba = prev_part->last_lba + 1;
    }

    part->last_lba = part->first_lba + no_of_blocks - 1;

    if (part->last_lba >= primary.hdr.last_lba)
        return -PB_ERR_MEM;

    if (strlen(part_name) > GPT_PART_NAME_MAX_SIZE)
        return -PB_ERR;

    unsigned char *part_name_ptr = part->name;
    for (unsigned int i = 0; i < strlen(part_name); i++) {
        *part_name_ptr++ = part_name[i];
        *part_name_ptr++ = 0;
    }

    part->attr[7] = 0x80; /* Not bootable */
    part->attr[6] = PB_GPT_ATTR_OK;

    memcpy(part->type_uuid, type_uuid, 16);
    memcpy(part->uuid, type_uuid, 16);
    memcpy(&backup.part[part_idx], part, sizeof(struct gpt_part_hdr));

    return PB_OK;
}

static int gpt_write_tbl(bio_dev_t dev)
{
    uint32_t crc_tmp = 0;
    uint32_t err = 0;

    /* Calculate CRC32 for header and part table */
    primary.hdr.hdr_crc = 0;
    primary.hdr.part_array_crc =
        efi_crc32((uint8_t *)primary.part, (sizeof(struct gpt_part_hdr) * primary.hdr.no_of_parts));

    crc_tmp = efi_crc32((uint8_t *)&primary.hdr, (sizeof(struct gpt_header) - GPT_HEADER_RSZ));

    primary.hdr.hdr_crc = crc_tmp;

    /* Write protective MBR */
    memset(gpt_pmbr, 0, sizeof(gpt_pmbr));

    gpt_pmbr[448] = 2;
    gpt_pmbr[449] = 0;
    gpt_pmbr[450] = 0xEE; // type , 0xEE = GPT
    gpt_pmbr[451] = 0xFF;
    gpt_pmbr[452] = 0xFF;
    gpt_pmbr[453] = 0xFF;
    gpt_pmbr[454] = 0x01;

    uint32_t llba = (uint32_t)bio_get_last_block(dev);
    memcpy(&gpt_pmbr[458], &llba, sizeof(uint32_t));
    gpt_pmbr[510] = 0x55;
    gpt_pmbr[511] = 0xAA;

    err = bio_write(dev, 0, sizeof(gpt_pmbr), gpt_pmbr);

    if (err != PB_OK) {
        LOG_ERR("Writing protective MBR failed");
        return err;
    }

    /* Write primary GPT Table */
    LOG_DBG("writing primary gpt tbl to lba %llu", primary.hdr.current_lba);

    err = bio_write(dev, primary.hdr.current_lba, sizeof(primary), &primary);

    if (err != PB_OK) {
        LOG_ERR("error writing primary gpt table (%i)", err);
        return err;
    }

    /* Configure backup GPT table */
    memcpy(&backup.hdr, &primary.hdr, sizeof(struct gpt_header));
    memcpy(backup.part, primary.part, (sizeof(struct gpt_part_hdr) * primary.hdr.no_of_parts));

    uint64_t last_lba = bio_get_last_block(dev);

    struct gpt_header *hdr = &backup.hdr;

    hdr->backup_lba = primary.hdr.current_lba;
    hdr->current_lba = last_lba;
    hdr->entries_start_lba =
        (last_lba - ((hdr->no_of_parts * sizeof(struct gpt_part_hdr)) / bio_block_size(dev)));

    hdr->hdr_crc = 0;
    hdr->part_array_crc =
        efi_crc32((uint8_t *)backup.part, sizeof(struct gpt_part_hdr) * backup.hdr.no_of_parts);

    crc_tmp = efi_crc32((uint8_t *)hdr, sizeof(struct gpt_header) - GPT_HEADER_RSZ);
    backup.hdr.hdr_crc = crc_tmp;

    /* Write backup GPT table */
    LOG_INFO("Writing backup GPT tbl to LBA %llu", backup.hdr.entries_start_lba);

    return bio_write(dev, backup.hdr.entries_start_lba, sizeof(backup), &backup);
}

static int gpt_install_partition_table(bio_dev_t dev, int variant)
{
    int rc;
    int part_idx = 0;
    uuid_t part_guid;
    const struct gpt_table_list *tbl;

    if (variant < 0 || (size_t)variant >= tables_length)
        return -PB_ERR_PARAM;

    tbl = &tables[variant];

    rc = gpt_init_tbl(dev);

    if (rc != PB_OK)
        return rc;

    LOG_INFO("Installing partition table: %s", tbl->name);

    for (size_t n = 0; n < tbl->table_length; n++) {
        const struct gpt_part_table *ent = &tbl->table[n];

        LOG_DBG("Add: %s", ent->description);
        uuid_to_guid(ent->uu, part_guid);

        rc = gpt_add_part(part_idx++, ent->size / bio_block_size(dev), part_guid, ent->description);

        if (rc != PB_OK)
            return rc;
    }

    return gpt_write_tbl(dev);
}

int gpt_ptbl_init(bio_dev_t dev, const struct gpt_table_list *default_tables, size_t length)
{
    int rc;
    uuid_t uu;
    char name[37];

    LOG_INFO("Init");

    gpt_dev = dev;
    tables = default_tables;
    tables_length = length;

    rc = bio_set_install_partition_cb(dev, gpt_install_partition_table);

    if (rc != PB_OK)
        return rc;

    /* Read primary and backup GPT headers and parition tables */
    rc = bio_read(dev, 1, sizeof(primary), &primary);

    if (rc != PB_OK)
        return rc;

    size_t backup_lba = bio_get_last_block(dev) - ((128 * sizeof(struct gpt_part_hdr)) / 512);

    rc = bio_read(dev, backup_lba, sizeof(backup), &backup);

    if (rc != PB_OK)
        return rc;

    if (gpt_is_valid(&primary.hdr, primary.part) != PB_OK) {
        LOG_ERR("Primary GPT table is corrupt or missing,"
                " trying to recover backup");

        if (gpt_has_valid_header(&backup.hdr) != PB_OK) {
            LOG_ERR("Invalid backup GPT header, unable to recover");
            return -PB_ERR;
        }

        if (gpt_has_valid_part_array(&backup.hdr, backup.part) != PB_OK) {
            LOG_ERR("Invalid backup GPT part array, unable to recover");
            return -PB_ERR;
        }

        LOG_ERR("Recovering from backup GPT tables");

        memcpy(&primary.hdr, &backup.hdr, sizeof(struct gpt_header));
        memcpy(primary.part, backup.part, (sizeof(struct gpt_part_hdr) * backup.hdr.no_of_parts));

        primary.hdr.backup_lba = bio_get_last_block(dev);
        primary.hdr.current_lba = 1;
        primary.hdr.entries_start_lba = primary.hdr.current_lba + 1;

        LOG_DBG("no_of_parts = %u", primary.hdr.no_of_parts);

        if (gpt_write_tbl(dev) != PB_OK) {
            LOG_ERR("Could not update primary GPT table, unable to recover");
            return -PB_ERR;
        }
    }

    if (gpt_is_valid(&backup.hdr, backup.part) != PB_OK) {
        LOG_ERR("Backup GPT table is corrupt or missing,"
                " trying to recover from primary");

        if (gpt_has_valid_header(&primary.hdr) != PB_OK) {
            LOG_ERR("Invalid primary GPT header, unable to recover");
            return -PB_ERR;
        }

        if (gpt_has_valid_part_array(&primary.hdr, primary.part) != PB_OK) {
            LOG_ERR("Invalid primary GPT part array, unable to recover");
            return -PB_ERR;
        }

        LOG_ERR("Recovering from primary GPT tables");

        memcpy(&backup.hdr, &primary.hdr, sizeof(struct gpt_header));
        memcpy(backup.part, primary.part, (sizeof(struct gpt_part_hdr) * primary.hdr.no_of_parts));

        backup.hdr.backup_lba = 1;
        backup.hdr.current_lba = bio_get_last_block(dev);
        backup.hdr.entries_start_lba =
            (backup.hdr.current_lba -
             ((backup.hdr.no_of_parts * sizeof(struct gpt_part_hdr)) / 512));

        if (gpt_write_tbl(dev) != PB_OK) {
            LOG_ERR("Could not update GPT table, unable to recover");
            return -PB_ERR;
        }
    }

    /* Note/TODO: We check that both primary and backup tables are OK,
     * but we don't compare them. It's possible that we could have two
     * tables with correct checksum's but they disagree with each other
     * regarding partition layout.
     *
     * We here asume that if both checksum's are OK the primary table contains
     * the thruth
     *
     * It's likely not worth the extra boot up time to compare the tables.
     */

    for (struct gpt_part_hdr *p = primary.part; p->first_lba; p++) {
        int n = 0;
        uuid_to_guid(p->uuid, uu);

        for (int i = 0; i < 36; i++) {
            name[i] = p->name[n];
            n += 2;
        }

        name[36] = 0;
        bio_dev_t new_dev =
            bio_allocate_parent(dev, p->first_lba, p->last_lba, bio_block_size(dev), uu, name);
        if (new_dev < 0) {
            LOG_ERR("bio alloc failed (%i)", new_dev);
            return new_dev;
        }
    }

    return PB_OK;
}
