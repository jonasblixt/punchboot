#include <drivers/partition/static.h>
#include <pb/bio.h>
#include <pb/pb.h>

int static_ptbl_init(bio_dev_t dev, const struct static_part_table *table)
{
    size_t block_sz = bio_block_size(dev);
    const struct static_part_table *entry = table;

    if (table == NULL)
        return -PB_ERR_PARAM;

    for (; entry->uu != NULL; entry++) {
        lba_t last_lba = entry->first_lba + (entry->size / block_sz) - 1;
        bio_dev_t new_dev = bio_allocate_parent(
            dev, entry->first_lba, last_lba, block_sz, entry->uu, entry->description);
        if (new_dev < 0) {
            LOG_ERR("bio alloc failed (%i)", new_dev);
            return new_dev;
        }
    }

    return PB_OK;
}
