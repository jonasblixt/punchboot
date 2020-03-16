#ifndef INCLUDE_PB_PBTBL_H_
#define INCLUDE_PB_PBTBL_H_


#define PB_PART_FLAG_BOOTABLE       (1 << 0)
#define PB_PART_FLAG_EMMC_BOOT_PART (1 << 1)

/**
 * Punchboot partition table entry (64 byte)
 *
 * first_block:     Start of partition
 * last_block:      Last block of partition
 * uuid:            Partition UUID
 * block_size:      Partition block size in bytes
 * flags:           Partition flags
 * description:     Textual description of partition
 *
 */

struct pb_part_table_entry
{
    uint8_t uuid[16];
    uint64_t first_block;
    uint64_t last_block;
    uint32_t block_size;
    uint32_t flags;
    uint8_t description[24];
} __attribute__((packed));

#endif  // INCLUDE_PB_PBTBL_H_
