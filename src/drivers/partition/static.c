#include <drivers/partition/static.h>
#include <pb/bio.h>
#include <pb/pb.h>
#include <stdbool.h>
#include <uuid.h>

/* The table of the device that is mapped, for static_ptbl_in_place() */
static struct {
    bool valid;
    uintptr_t base;
    size_t block_sz;
    const struct static_part_table *table;
} mapped;

/* The board's data is checked before a partition exists */
static int
ptbl_check(bio_dev_t dev, const struct static_part_table *table, bool is_mapped, uintptr_t base)
{
    ssize_t dev_block_sz = bio_block_size(dev);
    size_t block_sz;
    lba_t next_start_lba = 0;

    if (table == NULL)
        return -PB_ERR_PARAM;

    if (dev_block_sz < 0)
        return (int)dev_block_sz;

    block_sz = (size_t)dev_block_sz;

    for (const struct static_part_table *entry = table; entry->uu != NULL; entry++) {
        if (entry->first_lba) {
            next_start_lba = entry->first_lba;
        }

        if (entry->size == 0) {
            return -PB_ERR_PARAM;
        }

        if (entry->size % block_sz != 0) {
            return -PB_ERR_ALIGN;
        }

        lba_t last_lba = next_start_lba + (entry->size / block_sz) - 1;

        if (entry->flags & STATIC_PART_FLAG_IN_PLACE) {
            /* Its address needs the device's, and its last byte has one */
            uint64_t end = ((uint64_t)last_lba + 1) * block_sz;

            if (!is_mapped || end - 1 > UINTPTR_MAX - base) {
                LOG_ERR("'%s' can't be booted in place", entry->description);
                return -PB_ERR_PARAM;
            }
        }

        next_start_lba = last_lba + 1;
    }

    return PB_OK;
}

static int ptbl_allocate(bio_dev_t dev, const struct static_part_table *table)
{
    size_t block_sz = bio_block_size(dev);
    lba_t next_start_lba = 0;

    for (const struct static_part_table *entry = table; entry->uu != NULL; entry++) {
        if (entry->first_lba) {
            next_start_lba = entry->first_lba;
        }

        lba_t last_lba = next_start_lba + (entry->size / block_sz) - 1;
        bio_dev_t new_dev = bio_allocate_parent(
            dev, next_start_lba, last_lba, block_sz, entry->uu, entry->description);
        if (new_dev < 0) {
            LOG_ERR("bio alloc failed (%i)", new_dev);
            return new_dev;
        }

        next_start_lba = last_lba + 1;
    }

    return PB_OK;
}

int static_ptbl_init(bio_dev_t dev, const struct static_part_table *table)
{
    int rc = ptbl_check(dev, table, false, 0);

    if (rc != PB_OK)
        return rc;

    return ptbl_allocate(dev, table);
}

int static_ptbl_init_mapped(bio_dev_t dev, uintptr_t base, const struct static_part_table *table)
{
    int rc;

    /* A partition that is booted in place stays so: with another table in
     * its table's place it would be loaded, to its address in the flash */
    if (mapped.valid) {
        for (const struct static_part_table *entry = mapped.table; entry->uu != NULL; entry++) {
            if ((entry->flags & STATIC_PART_FLAG_IN_PLACE) && bio_get_part_by_uu(entry->uu) >= 0)
                return -PB_ERR_STATE;
        }
    }

    rc = ptbl_check(dev, table, true, base);

    if (rc != PB_OK)
        return rc;

    /* Before the partitions exist: one that is there is in place, also when
     * a later one can't be allocated */
    mapped.base = base;
    mapped.block_sz = bio_block_size(dev);
    mapped.table = table;
    mapped.valid = true;

    return ptbl_allocate(dev, table);
}

int static_ptbl_in_place(bio_dev_t dev, uintptr_t *base, size_t *size)
{
    const unsigned char *uu = bio_get_uu(dev);
    lba_t next_start_lba = 0;

    if (!mapped.valid || uu == NULL)
        return -PB_ERR_NOT_FOUND;

    /* The range is the table's, never what 'dev' says of itself: another
     * device's table comes from its storage, and an address can't */
    for (const struct static_part_table *entry = mapped.table; entry->uu != NULL; entry++) {
        if (entry->first_lba)
            next_start_lba = entry->first_lba;

        if ((entry->flags & STATIC_PART_FLAG_IN_PLACE) && uuid_compare(entry->uu, uu) == 0) {
            *base = mapped.base + (uintptr_t)next_start_lba * mapped.block_sz;
            *size = entry->size;
            return PB_OK;
        }

        next_start_lba += entry->size / mapped.block_sz;
    }

    return -PB_ERR_NOT_FOUND;
}

static int ptbl_static_init(const struct ptbl *ptbl, bio_dev_t dev)
{
    return static_ptbl_init(dev, ptbl->table);
}

static int ptbl_static_mapped_init(const struct ptbl *ptbl, bio_dev_t dev)
{
    return static_ptbl_init_mapped(dev, ptbl->base, ptbl->table);
}

const struct ptbl_ops ptbl_static_ops = {
    .init = ptbl_static_init,
};

const struct ptbl_ops ptbl_static_mapped_ops = {
    .init = ptbl_static_mapped_init,
};
