/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <pb.h>
#include <string.h>
#include <io.h>
#include <plat/test/virtio_block.h>
#include <plat/test/virtio_queue.h>

static volatile __a16b uint8_t status;

uint32_t virtio_block_init(struct virtio_block_device *d)
{
    if (virtio_mmio_init(&d->dev) != PB_OK)
        return PB_ERR;

    LOG_DBG("VIRTIO %p", &d->dev);
    d->config = (struct virtio_blk_config *) (d->dev.base + 0x100);


    d->config->blk_size = 512;
    d->config->writeback = 0;
    d->config->capacity = 32768;
    d->config->geometry.cylinders = 0;
    d->config->geometry.heads = 0;
    d->config->geometry.sectors = 0;
    d->config->topology.physical_block_exp = 1;
    d->config->topology.alignment_offset = 0;
    d->config->topology.min_io_size = 512;
    d->config->topology.opt_io_size = 512;

    virtio_init_queue(d->_q_data, 1024, &d->q);
    virtio_mmio_init_queue(&d->dev, &d->q, 0, 1024);

    virtio_mmio_driver_ok(&d->dev);

    return PB_OK;
}

uint32_t virtio_block_write(struct virtio_block_device *d,
                            uint32_t lba,
                            uintptr_t buf,
                            uint32_t no_of_blocks)
{
    struct virtio_blk_req r;
    struct virtq *q = &d->q;
    uint16_t idx = (q->avail->idx % q->num);

    LOG_DBG("lba = 0x%x, buf: %p, no_of_blocks: %u", lba, (void *)
                                                    buf, no_of_blocks);
    status = VIRTIO_BLK_S_UNSUPP;

    r.type = VIRTIO_BLK_T_OUT;
    r.sector_low = lba;
    r.sector_hi = 0;

    uint16_t idx_start = idx;

    q->desc[idx].addr = (uint32_t)((uintptr_t) &r);
    q->desc[idx].len = sizeof(struct virtio_blk_req);
    q->desc[idx].flags = VIRTQ_DESC_F_NEXT;
    q->desc[idx].next = ((idx + 1) % q->num);
    idx = ((idx + 1) % q->num);

    q->desc[idx].addr = (uint32_t)((uintptr_t) (buf));
    q->desc[idx].len = (512*no_of_blocks);
    q->desc[idx].flags = VIRTQ_DESC_F_NEXT;
    q->desc[idx].next = ((idx + 1) % q->num);
    idx = ((idx + 1) % q->num);

    q->desc[idx].addr = (uint32_t)((uintptr_t) &status);
    q->desc[idx].len = 1;
    q->desc[idx].flags = VIRTQ_DESC_F_WRITE;
    q->desc[idx].next = 0;
    idx = ((idx + 1) % q->num);

    q->avail->ring[idx_start] = idx_start;
    q->avail->idx++;

    virtio_mmio_notify_queue(&d->dev, &d->q);

    LOG_DBG("q->avail->idx = %u, q->used->idx = %u",
                q->avail->idx, q->used->idx);
    while ( ((q->avail->idx) != (q->used->idx)) )
        __asm__ volatile("nop");

    LOG_DBG("q->avail->idx = %u, q->used->idx = %u",
                    q->avail->idx, q->used->idx);

    if (status == VIRTIO_BLK_S_OK)
        return PB_OK;

    LOG_ERR("Failed, lba=%u, no_of_blocks=%u", lba, no_of_blocks);
    return PB_ERR;
}


uint32_t virtio_block_read(struct virtio_block_device *d,
                            uint32_t lba,
                            uintptr_t buf,
                            uint32_t no_of_blocks)
{
    struct virtq *q = &d->q;
    struct virtio_blk_req r;
    uint16_t idx = (q->avail->idx % q->num);
    uint16_t idx_start = idx;
    status = VIRTIO_BLK_S_UNSUPP;
    LOG_DBG("lba = 0x%x, buf: %p, no_of_blocks: %u", lba, (void *)
                                                    buf, no_of_blocks);
    memset(&r, 0, sizeof(struct virtio_blk_req));
    r.type = VIRTIO_BLK_T_IN;
    r.reserved = 0;
    r.sector_low = lba;
    r.sector_hi = 0;


    q->desc[idx].addr = (uint32_t)((uintptr_t) &r);
    q->desc[idx].len = sizeof(struct virtio_blk_req);
    q->desc[idx].flags = VIRTQ_DESC_F_NEXT;
    q->desc[idx].next = ((idx + 1) % q->num);
    idx = ((idx + 1) % q->num);

    q->desc[idx].addr = (uint32_t)((uintptr_t) buf);
    q->desc[idx].len = (512*no_of_blocks);
    q->desc[idx].flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;
    q->desc[idx].next = ((idx + 1) % q->num);
    idx = ((idx + 1) % q->num);

    q->desc[idx].addr = (uint32_t)((uintptr_t) &status);
    q->desc[idx].len = 1;
    q->desc[idx].flags = VIRTQ_DESC_F_WRITE;
    q->desc[idx].next = 0;
    idx = ((idx + 1) % q->num);

    q->avail->ring[idx_start] = idx_start;
    q->avail->idx++;

    virtio_mmio_notify_queue(&d->dev, &d->q);

    LOG_DBG("status = %u", d->dev.status);
    LOG_DBG("q->avail->idx = %u, q->used->idx = %u",
                q->avail->idx, q->used->idx);
    while ( (q->avail->idx) != (q->used->idx) )
        __asm__ volatile ("nop");
    LOG_DBG("status = %u", d->dev.status);
    LOG_DBG("q->avail->idx = %u, q->used->idx = %u",
                    q->avail->idx, q->used->idx);

    if (status == VIRTIO_BLK_S_OK)
        return PB_OK;

    LOG_ERR("Failed, lba=%u, no_of_blocks=%u", lba, no_of_blocks);
    return PB_ERR;
}


