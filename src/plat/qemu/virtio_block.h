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
#include <pb/storage.h>
#include <plat/qemu/virtio_mmio.h>
#include <plat/qemu/virtio_queue.h>
#include <plat/qemu/virtio.h>


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
    uint8_t _q_data[VIRTIO_QUEUE_SZ_WITH_PADDING(1024)] PB_ALIGN_4k;
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


struct virtio_blk_req
{
    uint32_t type;
    uint32_t reserved;
    uint32_t sector_low;
    uint32_t sector_hi;
};

int virtio_block_read(struct pb_storage_driver *drv,
                            size_t block_offset,
                            void *data,
                            size_t n_blocks);


int virtio_block_write(struct pb_storage_driver *drv,
                            size_t block_offset,
                            void *data,
                            size_t n_blocks);

int virtio_block_init(struct pb_storage_driver *drv);

#endif  // PLAT_TEST_VIRTIO_BLOCK_H_