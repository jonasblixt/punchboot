/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <pb/mmio.h>
#include <drivers/virtio/virtio.h>

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

static volatile uint8_t status __aligned(64);
static uint8_t queue_data[VIRTIO_QUEUE_SZ_WITH_PADDING(64)] __aligned(64);
static struct virtq queue;
static struct virtio_blk_req request __aligned(64);

static int virtio_xfer(bio_dev_t dev,
                       uint32_t xfer_type,
                       int lba,
                       size_t length,
                       uintptr_t buf)
{
    uint16_t idx = (queue.avail->idx % queue.num);
    uint16_t idx_start = idx;
    status = VIRTIO_BLK_S_UNSUPP;

    r.type = xfer_type;
    r.reserved = 0;
    r.sector_low = lba;
    r.sector_hi = 0;

    arch_clean_cache_range((uintptr_t) &r, sizeof(r));

    queue.desc[idx].addr = (uint32_t)((uintptr_t) &r);
    queue.desc[idx].len = sizeof(struct virtio_blk_req);
    queue.desc[idx].flags = VIRTQ_DESC_F_NEXT;
    queue.desc[idx].next = ((idx + 1) % queue.num);
    arch_clean_cache_range((uintptr_t) &queue.desc[idx], sizeof(queue.desc[idx]));
    idx = ((idx + 1) % queue.num);

    queue.desc[idx].addr = (uint32_t) buf;
    queue.desc[idx].len = (512 * n_blocks);
    queue.desc[idx].flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;
    queue.desc[idx].next = ((idx + 1) % queue.num);
    arch_clean_cache_range((uintptr_t) &queue.desc[idx], sizeof(queue.desc[idx]));
    idx = ((idx + 1) % queue.num);

    queue.desc[idx].addr = (uint32_t)((uintptr_t) &status);
    queue.desc[idx].len = 1;
    queue.desc[idx].flags = VIRTQ_DESC_F_WRITE;
    queue.desc[idx].next = 0;
    arch_clean_cache_range((uintptr_t) &queue.desc[idx], sizeof(queue.desc[idx]));
    idx = ((idx + 1) % queue.num);

    queue.avail->ring[idx_start] = idx_start;
    queue.avail->idx++;

    arch_clean_cache_range((uintptr_t) queue.avail, sizeof(*queue.avail));

    virtio_mmio_notify_queue(virtio_dev, &queue);

    while ((queue.avail->idx) != (queue.used->idx)) {
        arch_invalidate_cache_range((uintptr_t) queue.avail, sizeof(*queue.avail));
    }

    arch_invalidate_cache_range((uintptr_t) &status, sizeof(status));
    if (status == VIRTIO_BLK_S_OK)
        return PB_OK;

    return -PB_ERR_IO;
}

static int virtio_bio_read(bio_dev_t dev, int lba, size_t length, uintptr_t buf)
{
    return virtio_xfer(dev, VIRTIO_BLK_T_IN, lba, length, buf);
}

static int virtio_bio_write(bio_dev_t dev, int lba, size_t length, uintptr_t buf)
{
    return virtio_xfer(dev, VIRTIO_BLK_T_OUT, lba, length, buf);
}

bio_dev_t virtio_block_init(uintptr_t base, uuid_t uu)
{
    virtio_dev = virtio_mmio_alloc(base)

    if (virtio_dev < 0) {
        LOG_ERR("Init failed (%i)", virtio_dev);
        return virtio_dev;
    }

    struct virtio_blk_config *cfg = (struct virtio_blk_config *) \
                                            (base + VIRTIO_MMIO_CONFIG);

    LOG_INFO("Detected virtio disk @%"PRIxPTR", capacity = %llu",
                base, cfg->capacity);

    // TODO: Most of this should come from qemu?
    cfg->blk_size = 512;
    cfg->writeback = 0;
    cfg->capacity = 32768;
    cfg->geometry.cylinders = 0;
    cfg->geometry.heads = 0;
    cfg->geometry.sectors = 0;
    cfg->topology.physical_block_exp = 1;
    cfg->topology.alignment_offset = 0;
    cfg->topology.min_io_size = 512;
    cfg->topology.opt_io_size = 512;

    arch_clean_cache_range((uintptr_t) cfg, sizeof(*cfg));

    // TODO: Add define for queue size
    virtio_init_queue(queue_data, 64, &queue);
    virtio_mmio_init_queue(virtio_dev, &queue, 0, 64);

    virtio_mmio_driver_ok(&d->dev);

    bio_dev_t dev = bio_allocate(0,
                       32768 - 1,
                       512,
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
