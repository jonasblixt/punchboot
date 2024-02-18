#include <drivers/partition/static.h>
#include <pb/bio.h>
#include <pb/pb.h>

int static_ptbl_init(bio_dev_t dev, const struct static_part_table *table)
{
    size_t block_sz = bio_block_size(dev);
    lba_t next_start_lba = 0;
    const struct static_part_table *entry = table;

    if (table == NULL)
        return -PB_ERR_PARAM;

    for (; entry->uu != NULL; entry++) {
        if (entry->first_lba) {
            next_start_lba = entry->first_lba;
        }

        if (entry->size % block_sz != 0) {
            return -PB_ERR_ALIGN;
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
