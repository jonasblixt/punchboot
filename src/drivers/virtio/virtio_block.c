/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <inttypes.h>
#include <pb/pb.h>
#include <pb/mmio.h>
#include <arch/arch.h>
#include <drivers/virtio/virtio_block.h>
#include "virtio_mmio.h"
#include "virtio_queue.h"

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

#define VIRTIO_BLK_QUEUE_SZ 256

static uint8_t status __aligned(64);
static uint8_t queue_data[VIRTIO_QUEUE_SZ(VIRTIO_BLK_QUEUE_SZ, 4096)] __aligned(4096);
static struct virtq queue;
static struct virtio_blk_req request __aligned(64);
static uintptr_t base;

static int virtio_xfer(bio_dev_t dev,
                       bool read,
                       int lba,
                       size_t length,
                       uintptr_t buf)
{
    uint16_t idx = (queue.avail->idx % queue.num);
    status = VIRTIO_BLK_S_UNSUPP;

    request.type = read?VIRTIO_BLK_T_IN:VIRTIO_BLK_T_OUT;
    request.reserved = 0;
    request.sector_low = lba;
    request.sector_hi = 0;

    arch_clean_cache_range((uintptr_t) &request, sizeof(request));

    queue.desc[0].addr = (uint32_t)((uintptr_t) &request);
    queue.desc[0].len = sizeof(struct virtio_blk_req);
    queue.desc[0].flags = VIRTQ_DESC_F_NEXT;
    queue.desc[0].next = 1;

    queue.desc[1].addr = (uint32_t) buf;
    queue.desc[1].len = length;
    queue.desc[1].flags = VIRTQ_DESC_F_NEXT | (read?VIRTQ_DESC_F_WRITE:0);
    queue.desc[1].next = 2;

    queue.desc[2].addr = (uint32_t)((uintptr_t) &status);
    queue.desc[2].len = 1;
    queue.desc[2].flags = VIRTQ_DESC_F_WRITE;
    queue.desc[2].next = 0;

    arch_clean_cache_range((uintptr_t) &queue.desc[0], sizeof(queue.desc[0]) * 3);
    queue.avail->ring[idx] = 0;
    queue.avail->idx++;

    arch_clean_cache_range((uintptr_t) queue.avail, sizeof(*queue.avail));

    mmio_write_32(base + VIRTIO_MMIO_QUEUE_NOTIFY, 0);

    while (queue.avail->idx != queue.used->idx) {
        arch_invalidate_cache_range((uintptr_t) queue.used, sizeof(*queue.used));
    }

    arch_invalidate_cache_range((uintptr_t) &status, sizeof(status));

    if (status != VIRTIO_BLK_S_OK) {
        LOG_ERR("I/O ERROR idx=%u status=0x%x", idx, status);
        return -PB_ERR_IO;
    }

    return PB_OK;
}

static int virtio_bio_read(bio_dev_t dev, int lba, size_t length, uintptr_t buf)
{
    return virtio_xfer(dev, true, lba, length, buf);
}

static int virtio_bio_write(bio_dev_t dev, int lba, size_t length, uintptr_t buf)
{
    return virtio_xfer(dev, false, lba, length, buf);
}

bio_dev_t virtio_block_init(uintptr_t base_, const uuid_t uu)
{
    int rc;
    base = base_;
    uint32_t device_id = mmio_read_32(base + VIRTIO_MMIO_DEVICE_ID);

    if (device_id != 2) {
        LOG_ERR("Device id of %"PRIxPTR" is %u, expected 2", base, device_id);
        return -PB_ERR_IO;
    }

    struct virtio_blk_config *cfg = (struct virtio_blk_config *) \
                                            (base + VIRTIO_MMIO_CONFIG);

    LOG_INFO("Detected virtio disk @%"PRIxPTR", capacity = %llu blocks, bs = %u",
                base, cfg->capacity, cfg->blk_size);

    mmio_write_32(base + VIRTIO_MMIO_STATUS, 0); // TODO: Do we need this?
    mmio_write_32(base + VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE |
                                             VIRTIO_STATUS_DRIVER);

    mmio_write_32(base + VIRTIO_MMIO_GUEST_PAGE_SIZE, 4096);

    LOG_DBG("Maximum queue length = %u", mmio_read_32(base + VIRTIO_MMIO_QUEUE_NUM_MAX));
    /* Initiailze queue */
    queue.num = VIRTIO_BLK_QUEUE_SZ;
    queue.desc  = (struct virtq_desc *)  queue_data;
    queue.avail = (struct virtq_avail *) (queue_data + VIRTIO_QUEUE_AVAIL_OFFSET(queue.num, 4096));
    queue.used  = (struct virtq_used *)  (queue_data + VIRTIO_QUEUE_USED_OFFSET(queue.num, 4096));

    LOG_DBG("Q: avail=%lu, used=%lu", (uintptr_t) (queue.avail) - (uintptr_t) queue_data,
                                    (uintptr_t) (queue.used) - (uintptr_t) queue_data);
    mmio_write_32(base + VIRTIO_MMIO_QUEUE_SEL, 0);
    mmio_write_32(base + VIRTIO_MMIO_QUEUE_NUM, queue.num);
    mmio_write_32(base + VIRTIO_MMIO_QUEUE_ALIGN, 4096);
    mmio_write_32(base + VIRTIO_MMIO_QUEUE_PFN, (uint32_t) ((uintptr_t)queue_data >> 12));

    mmio_clrsetbits_32(base + VIRTIO_MMIO_STATUS, 0, VIRTIO_STATUS_DRIVER_OK);

    bio_dev_t dev = bio_allocate(0,
                                 cfg->capacity - 1,
                                 cfg->blk_size,
                                 uu,
                                 "Virtio disk");

    if (dev < 0)
        return dev;

    rc = bio_set_ios(dev, virtio_bio_read, virtio_bio_write);

    if (rc < 0)
        return rc;

    rc = bio_set_flags(dev, BIO_FLAG_VISIBLE | BIO_FLAG_WRITABLE);
    if (rc < 0)
        return rc;

    return dev;
}
