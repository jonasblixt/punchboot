#ifndef INCLUDE_PB_PARTITION_H_
#define INCLUDE_PB_PARTITION_H_

#include <stdint.h>
#include <stdbool.h>

#define PB_PART_FLAG_BOOTABLE       (1 << 0)
#define PB_PART_FLAG_WRITE          (1 << 1)
#define PB_PART_FLAG_OTP            (1 << 2)

/**
 * Punchboot partition table entry (64 byte)
 *
 * first_block:     Start of partition
 * last_block:      Last block of partition
 * uuid:            Partition UUID
 * flags:           Partition flags
 * description:     Textual description of partition
 *
 */

struct pb_partition_table_entry
{
    uint8_t uuid[16];
    uint64_t first_block;
    uint64_t last_block;
    uint32_t flags;
    uint8_t rz[28];
};

#endif  // INCLUDE_PB_PARTITION_H_
