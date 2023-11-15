#ifndef INCLUDE_DRIVERS_PARTITION_STATIC_H
#define INCLUDE_DRIVERS_PARTITION_STATIC_H

#include <stdint.h>
#include <pb/bio.h>

#define STATIC_PART_NAME_MAX_SIZE 36

struct static_part_table {
    const unsigned char * uu;
    const char * description;
    lba_t first_lba;
    size_t size;
};

int static_ptbl_init(bio_dev_t dev, const struct static_part_table *table);

#endif
