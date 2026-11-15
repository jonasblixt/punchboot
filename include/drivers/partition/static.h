#ifndef INCLUDE_DRIVERS_PARTITION_STATIC_H
#define INCLUDE_DRIVERS_PARTITION_STATIC_H

#include <pb/bio.h>
#include <pb/ptbl.h>
#include <pb/utils_def.h>
#include <stdint.h>

#define STATIC_PART_NAME_MAX_SIZE 36

/**
 * \def STATIC_PART_FLAG_IN_PLACE
 * The partition is booted in place: its image is verified where the device
 * is mapped and runs there, nothing is loaded. Only in a table that is set
 * up with static_ptbl_init_mapped().
 */
#define STATIC_PART_FLAG_IN_PLACE BIT(0)

struct static_part_table {
    const unsigned char *uu;
    const char *description;
    lba_t first_lba;
    size_t size;
    uint32_t flags; /*!< STATIC_PART_FLAG_* */
};

int static_ptbl_init(bio_dev_t dev, const struct static_part_table *table);

/**
 * The static table of a device that is memory mapped
 *
 * A partition's mapped address follows from 'base' and its first block, so
 * the table has no addresses. One table is mapped: it isn't replaced while a
 * partition of it that is booted in place exists.
 *
 * @param[in] dev The device
 * @param[in] base Address the device's first byte is mapped at
 * @param[in] table The partitions, ended by an entry without uu
 *
 * @return PB_OK on success
 *        -PB_ERR_PARAM, a partition booted in place ends above the address space
 *        -PB_ERR_STATE, another mapped table has partitions booted in place
 */
int static_ptbl_init_mapped(bio_dev_t dev, uintptr_t base, const struct static_part_table *table);

/**
 * The mapped range of a partition that is booted in place
 *
 * @param[in] dev The partition
 * @param[out] base Address of its first byte
 * @param[out] size Its size in bytes
 *
 * @return PB_OK when 'dev' is booted in place
 *        -PB_ERR_NOT_FOUND when it isn't: it is loaded
 */
int static_ptbl_in_place(bio_dev_t dev, uintptr_t *base, size_t *size);

extern const struct ptbl_ops ptbl_static_ops;
extern const struct ptbl_ops ptbl_static_mapped_ops;

/**
 * A board's partition table (board.ptbl): the partitions '_table', struct
 * static_part_table[], on the device with the UUID '_device', which has no
 * table on it.
 */
#define PTBL_STATIC(_device, _table)                                      \
    {                                                                     \
        .ops = &ptbl_static_ops,                                          \
        .device = (_device),                                              \
        .table = PB_CONFIG(&(_table)[0], const struct static_part_table), \
    }

/**
 * The same for a device that is memory mapped: '_base' is the address of its
 * first byte. Only such a table has partitions that are booted in place.
 */
#define PTBL_STATIC_MAPPED(_device, _base, _table)                        \
    {                                                                     \
        .ops = &ptbl_static_mapped_ops,                                   \
        .device = (_device),                                              \
        .table = PB_CONFIG(&(_table)[0], const struct static_part_table), \
        .base = (_base),                                                  \
    }

#endif
