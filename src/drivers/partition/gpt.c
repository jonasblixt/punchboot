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
#include <stdbool.h>
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

#define PB_GPT_ATTR_OK           (1 << 7) /*Bit 55*/
#define PB_GPT_ATTR_NOT_BOOTABLE (1 << 7) /* Bit 63, attr[7] */
#define GPT_HEADER_REVISION_1_0  0x00010000

/* The protective MBR: one partition of type 0xEE over the disk (UEFI 2.x 5.2.3) */
#define MBR_PART0                446 /* The first partition entry */
#define MBR_PART_CHS_START       (MBR_PART0 + 1)
#define MBR_PART_TYPE            (MBR_PART0 + 4)
#define MBR_PART_CHS_END         (MBR_PART0 + 5)
#define MBR_PART_LBA_START       (MBR_PART0 + 8)
#define MBR_PART_LBA_SIZE        (MBR_PART0 + 12)
#define MBR_TYPE_GPT_PROTECTIVE  0xEE
#define MBR_SIGNATURE            510 /* 0x55, 0xAA */

static const uint64_t gpt_header_signature = 0x5452415020494645ULL;
static uint32_t prng_state;

static struct gpt_primary_tbl primary __aligned(64);
static struct gpt_backup_tbl backup __aligned(64);
static uint8_t gpt_pmbr[512];
static bio_dev_t gpt_dev;
/* The first block of the backup: its array's, which its header gives. The
 * header is in the device's last block */
static lba_t backup_first_lba;
static const struct gpt_table_list *tables;
static size_t tables_length;

static inline uint32_t efi_crc32(const void *buf, uint32_t sz)
{
    return (crc32(~0L, buf, sz) ^ ~0UL);
}

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

    /* The partition array must fit in the in-memory tables */
    if (hdr->no_of_parts > ARRAY_SIZE(primary.part) ||
        hdr->part_entry_sz != sizeof(struct gpt_part_hdr)) {
        LOG_ERR("Unsupported partition array, %u entries of %u bytes",
                hdr->no_of_parts,
                hdr->part_entry_sz);
        return PB_ERR;
    }

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

/* The blocks that the header's partition array takes on the device */
static uint32_t gpt_array_blocks(bio_dev_t dev, const struct gpt_header *hdr)
{
    const uint32_t block_sz = (uint32_t)bio_block_size(dev);

    return (hdr->no_of_parts * (uint32_t)sizeof(struct gpt_part_hdr) + block_sz - 1) / block_sz;
}

/* An entry of the table on the medium is input (bio.md 4): a partition is
 * made of it only when its blocks are in the device, which also means that
 * they fit lba_t, the first isn't after the last, and none is one of the
 * table's own: the protective MBR, the primary header and its array at the
 * start of the device, the backup's array and its header at the end. */
static bool gpt_entry_is_valid(bio_dev_t dev, const struct gpt_part_hdr *part)
{
    const uint64_t last_lba = (uint64_t)bio_get_last_block(dev);
    const uint64_t primary_end = 1 + gpt_array_blocks(dev, &primary.hdr);

    if (part->first_lba > part->last_lba || part->last_lba > last_lba)
        return false;

    return part->first_lba > primary_end && part->last_lba < backup_first_lba;
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
    hdr->rev = GPT_HEADER_REVISION_1_0;
    hdr->hdr_sz = sizeof(struct gpt_header) - GPT_HEADER_RSZ;
    hdr->current_lba = 1;
    hdr->no_of_parts = 128;

    hdr->first_lba =
        (hdr->current_lba + 1 + (hdr->no_of_parts * sizeof(struct gpt_part_hdr)) / 512);
    hdr->backup_lba = last_lba;
    hdr->last_lba = (last_lba - (hdr->no_of_parts * sizeof(struct gpt_part_hdr)) / 512);
    hdr->entries_start_lba = primary.hdr.current_lba + 1;
    hdr->part_entry_sz = sizeof(struct gpt_part_hdr);

    for (uint32_t i = 0; i < ARRAY_SIZE(hdr->disk_uuid); i++)
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

    part->attr[7] = PB_GPT_ATTR_NOT_BOOTABLE;
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

    /* CHS start 0/0/2, CHS end 0xFFFFFF (beyond CHS), from LBA 1 */
    gpt_pmbr[MBR_PART_CHS_START + 1] = 2;
    gpt_pmbr[MBR_PART_CHS_START + 2] = 0;
    gpt_pmbr[MBR_PART_TYPE] = MBR_TYPE_GPT_PROTECTIVE;
    gpt_pmbr[MBR_PART_CHS_END] = 0xFF;
    gpt_pmbr[MBR_PART_CHS_END + 1] = 0xFF;
    gpt_pmbr[MBR_PART_CHS_END + 2] = 0xFF;
    gpt_pmbr[MBR_PART_LBA_START] = 0x01;

    uint32_t llba = (uint32_t)bio_get_last_block(dev);
    memcpy(&gpt_pmbr[MBR_PART_LBA_SIZE], &llba, sizeof(uint32_t));
    gpt_pmbr[MBR_SIGNATURE] = 0x55;
    gpt_pmbr[MBR_SIGNATURE + 1] = 0xAA;

    err = bio_write(dev, 0, sizeof(gpt_pmbr), gpt_pmbr);

    if (err != PB_OK) {
        LOG_ERR("Writing protective MBR failed");
        return err;
    }

    /* Write primary GPT Table. Its place is LBA 1, where it is read: never
     * the LBA a header from the medium gives for itself (bio.md 4) */
    LOG_DBG("writing primary gpt tbl to lba 1");

    const size_t array_sz = (size_t)gpt_array_blocks(dev, &primary.hdr) * bio_block_size(dev);

    err = bio_write(dev, 1, sizeof(struct gpt_header) + array_sz, &primary);

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
    hdr->entries_start_lba = last_lba - gpt_array_blocks(dev, hdr);

    hdr->hdr_crc = 0;
    hdr->part_array_crc =
        efi_crc32((uint8_t *)backup.part, sizeof(struct gpt_part_hdr) * backup.hdr.no_of_parts);

    crc_tmp = efi_crc32((uint8_t *)hdr, sizeof(struct gpt_header) - GPT_HEADER_RSZ);
    backup.hdr.hdr_crc = crc_tmp;

    /* Write backup GPT table: the array's blocks, as many as the entries
     * take, and then the header that makes them valid */
    LOG_INFO("Writing backup GPT tbl to LBA %" PRIu64, backup.hdr.entries_start_lba);

    err = bio_write(dev, (lba_t)backup.hdr.entries_start_lba, array_sz, backup.part);

    if (err != PB_OK) {
        LOG_ERR("error writing backup gpt array (%i)", err);
        return err;
    }

    backup_first_lba = (lba_t)backup.hdr.entries_start_lba;

    return bio_write(dev, (lba_t)last_lba, sizeof(struct gpt_header), &backup.hdr);
}

/* Reads the backup: its header from the device's last block, and its array
 * from where the header says. 'valid' is whether both are. The return value
 * is the reads': a copy that can't be read isn't a bad one. */
static int gpt_read_backup(bio_dev_t dev, bool *valid)
{
    int rc;
    const lba_t hdr_lba = (lba_t)bio_get_last_block(dev);
    const lba_t array_lba = hdr_lba - ((128 * sizeof(struct gpt_part_hdr)) / 512);

    *valid = false;

    /* The header, and the array of 128 entries before it, where every tool
     * puts it: one read */
    rc = bio_read(dev, array_lba, sizeof(backup), &backup);

    if (rc != PB_OK)
        return rc;

    if (gpt_has_valid_header(&backup.hdr) != PB_OK)
        return PB_OK;

    /* The array's place is input: after the primary's header, before its
     * own, all of it */
    const uint64_t first_lba = backup.hdr.entries_start_lba;
    const uint32_t blocks = gpt_array_blocks(dev, &backup.hdr);

    if (first_lba < 2 || first_lba > hdr_lba || blocks > hdr_lba - first_lba) {
        LOG_ERR("Backup array at %" PRIu64 ", %u blocks", first_lba, blocks);
        return PB_OK;
    }

    /* A table of fewer entries has its array closer to the header */
    if (first_lba != array_lba) {
        rc = bio_read(dev, (lba_t)first_lba, (size_t)blocks * bio_block_size(dev), backup.part);

        if (rc != PB_OK)
            return rc;
    }

    if (gpt_has_valid_part_array(&backup.hdr, backup.part) != PB_OK)
        return PB_OK;

    backup_first_lba = (lba_t)first_lba;
    *valid = true;

    return PB_OK;
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
    bool backup_valid;

    LOG_INFO("Init");

    /* The board's data (board.md 3): a GPT has at least one layout that
     * command mode can install. Checked before the device is touched */
    if (default_tables == NULL || length == 0) {
        LOG_ERR("No layout");
        return -PB_ERR_PARAM;
    }

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

    rc = gpt_read_backup(dev, &backup_valid);

    if (rc != PB_OK)
        return rc;

    if (gpt_is_valid(&primary.hdr, primary.part) != PB_OK) {
        LOG_ERR("Primary GPT table is corrupt or missing,"
                " trying to recover backup");

        if (!backup_valid) {
            LOG_ERR("Invalid backup GPT, unable to recover");
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

    if (!backup_valid) {
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

    for (struct gpt_part_hdr *p = primary.part; p < &primary.part[primary.hdr.no_of_parts]; p++) {
        int n = 0;

        /* An empty entry is skipped, and the ones after it are made (bio.md 4) */
        if (p->first_lba == 0)
            continue;

        uuid_to_guid(p->uuid, uu);

        for (int i = 0; i < 36; i++) {
            name[i] = p->name[n];
            n += 2;
        }

        name[36] = 0;

        /* An entry that fails is skipped and logged, and the others are
         * made (bio.md 4) */
        if (!gpt_entry_is_valid(dev, p)) {
            LOG_ERR("Entry skipped, %s: %" PRIu64 " - %" PRIu64, name, p->first_lba, p->last_lba);
            continue;
        }

        bio_dev_t new_dev = bio_allocate_parent(
            dev, (lba_t)p->first_lba, (lba_t)p->last_lba, bio_block_size(dev), uu, name);

        /* The core refuses what a device can't be: an entry whose UUID is
         * taken (bio.md 2.6). The first one keeps the name */
        if (new_dev == -PB_ERR_PARAM) {
            LOG_ERR("Entry refused, %s", name);
            continue;
        }

        if (new_dev < 0) {
            LOG_ERR("bio alloc failed (%i)", new_dev);
            return new_dev;
        }
    }

    return PB_OK;
}

static int ptbl_gpt_init(const struct ptbl *ptbl, bio_dev_t dev)
{
    return gpt_ptbl_init(dev, ptbl->table, ptbl->length);
}

const struct ptbl_ops ptbl_gpt_ops = {
    .init = ptbl_gpt_init,
};
