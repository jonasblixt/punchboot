/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_TEST_VIRTIO_BLOCK_H_
#define PLAT_TEST_VIRTIO_BLOCK_H_

#include <stdint.h>
#include <plat/test/virtio_mmio.h>
#include <plat/test/virtio_queue.h>
#include <plat/test/virtio.h>


struct virtio_blk_config {
    uint64_t capacity;
    uint32_t size_max;
    uint32_t seg_max;
    struct virtio_blk_geometry {
        uint16_t cylinders;
        uint8_t heads;
        uint8_t sectors;
    } geometry;
    uint32_t blk_size;
    struct virtio_blk_topology {
        uint8_t physical_block_exp;
        uint8_t alignment_offset;
        uint16_t min_io_size;
        uint32_t opt_io_size;
    } topology;
    uint8_t writeback;
};

struct virtio_block_device
{
    uint8_t _q_data[VIRTIO_QUEUE_SZ_WITH_PADDING(1024)];
    struct virtio_device dev;
    struct virtio_blk_config *config;
    struct virtq q;
};


#define VIRTIO_BLK_T_IN           0
#define VIRTIO_BLK_T_OUT          1
#define VIRTIO_BLK_T_FLUSH        4

#define VIRTIO_BLK_S_OK        0
#define VIRTIO_BLK_S_IOERR     1
#define VIRTIO_BLK_S_UNSUPP    2


struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint32_t sector_low;
    uint32_t sector_hi;
};

uint32_t virtio_block_init(struct virtio_block_device *d);


uint32_t virtio_block_write(struct virtio_block_device *d,
                            uint32_t lba,
                            uintptr_t buf,
                            uint32_t no_of_blocks);

uint32_t virtio_block_read(struct virtio_block_device *d,
                            uint32_t lba,
                            uintptr_t buf,
                            uint32_t no_of_blocks);
#endif  // PLAT_TEST_VIRTIO_BLOCK_H_
