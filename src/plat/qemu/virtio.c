/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include <stdio.h>
#include <string.h>
#include <pb/board.h>
#include <pb/io.h>
#include "virtio.h"
#include "virtio_mmio.h"
#include "virtio_queue.h"

uint32_t virtio_mmio_init(struct virtio_device *d)
{
    if ((pb_read32(d->base + VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976))
    {
        LOG_ERR("Magic error %x", d->base);
        return PB_ERR;
    }

    if ((pb_read32(d->base + VIRTIO_MMIO_DEVICE_ID) != d->device_id))
        return PB_ERR;

    if ((pb_read32(d->base + VIRTIO_MMIO_VENDOR_ID) != d->vendor_id))
        return PB_ERR;

    LOG_INFO("Found VIRTIO @ 0x%08x, type: 0x%02x, vendor: 0x%08x, version: %u",
                d->base,
                pb_read32(d->base + VIRTIO_MMIO_DEVICE_ID),
                pb_read32(d->base + VIRTIO_MMIO_VENDOR_ID),
                pb_read32(d->base + VIRTIO_MMIO_VERSION));


    d->status = 0;
    pb_write32(d->status , (d->base + VIRTIO_MMIO_STATUS));
    d->status |= (VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);
    pb_write32(d->status , (d->base + VIRTIO_MMIO_STATUS));

    pb_write32(4096, d->base + VIRTIO_MMIO_GUEST_PAGE_SIZE);

    return PB_OK;
}


uint32_t virtio_mmio_get_features(struct virtio_device *d)
{
    return pb_read32(d->base + VIRTIO_MMIO_DEVICE_FEATURES);
}

static uint32_t virtio_mmio_get_status(struct virtio_device *d)
{
    return pb_read32(d->base + VIRTIO_MMIO_STATUS);
}

uint32_t virtio_mmio_select_features(struct virtio_device *d, uint32_t feat)
{
    uint32_t cap = virtio_mmio_get_features(d);

    if ((cap & feat) != feat)
        return PB_ERR;

    pb_write32(feat, d->base + VIRTIO_MMIO_DEVICE_FEATURES_SEL);

    d->status |= VIRTIO_STATUS_FEATURES_OK;
    pb_write32(d->status , d->base + VIRTIO_MMIO_STATUS);

    if ((virtio_mmio_get_status(d) & VIRTIO_STATUS_FAILED) ==
                VIRTIO_STATUS_FAILED)
        return PB_ERR;

    return PB_OK;
}

uint32_t virtio_mmio_init_queue(struct virtio_device *d,
                                struct virtq *q,
                                uint32_t queue_index,
                                uint32_t queue_sz)
{
    q->queue_index = queue_index;
    pb_write32(queue_index, d->base + VIRTIO_MMIO_QUEUE_SEL);

    uint32_t q_size = pb_read32(d->base + VIRTIO_MMIO_QUEUE_NUM_MAX);

    if (q_size < queue_sz)
        return PB_ERR;

    pb_write32(queue_sz, d->base + VIRTIO_MMIO_QUEUE_NUM);
    pb_write32(4096, d->base + VIRTIO_MMIO_QUEUE_ALIGN);
    pb_write32((((uint32_t) q->desc) >> 12),
                    d->base + VIRTIO_MMIO_QUEUE_PFN);

    LOG_INFO("Virtio Queue %u init: %08x, %08x, %08x",
            q->queue_index, (uint32_t) q->desc,
            (uint32_t) q->avail, (uint32_t) q->used);

    return PB_OK;
}

uint32_t virtio_mmio_driver_ok(struct virtio_device *d)
{
    d->status |= VIRTIO_STATUS_DRIVER_OK;
    pb_write32(d->status , d->base + VIRTIO_MMIO_STATUS);

    return PB_OK;
}

uint32_t virtio_mmio_write_one(struct virtio_device *d,
                           struct virtq *q,
                           uint8_t *buf,
                           uint32_t len)
{
    uint16_t idx = (q->avail->idx % q->num);
    uint32_t bytes_to_transfer = len;
    uint32_t pos = 0;
    uint32_t descriptor_count = 0;
    uint32_t chunk;

    while (bytes_to_transfer)
    {
        if (bytes_to_transfer > 4096)
            chunk = 4096;
        else
            chunk = bytes_to_transfer;

        bytes_to_transfer -= chunk;

        q->desc[idx].addr = (uint32_t)(uintptr_t) &buf[pos];
        q->desc[idx].len = chunk;

        //  idx = ((idx + 1) % (q->num));

        if (bytes_to_transfer == 0)
        {
            q->desc[idx].flags = 0;
            q->desc[idx].next = 0;
        } else {
            q->desc[idx].flags = VIRTQ_DESC_F_NEXT;
            q->desc[idx].next = (idx+1) % q->num;
        }

        arch_clean_cache_range((uintptr_t) &q->desc[idx], sizeof(q->desc[0]));

        idx = ((idx+1)%q->num);
        pos += chunk;
        descriptor_count++;
    }

    q->avail->ring[idx] = idx;
    q->avail->idx += descriptor_count;

    arch_clean_cache_range((uintptr_t) q->avail, sizeof(*q->avail));

    //  LOG_DBG("1: %u %u",q->avail->idx,q->used->idx);
    pb_write32(q->queue_index, d->base + VIRTIO_MMIO_QUEUE_NOTIFY);

    while ( (q->avail->idx != q->used->idx) )
    {
        __asm__ volatile ("nop");
        arch_invalidate_cache_range((uintptr_t) q->avail, sizeof(*q->avail));
        //  LOG_DBG("2: %u %u",q->avail->idx,q->used->idx);
    }

    //  LOG_DBG("3: %u %u",q->avail->idx,q->used->idx);
    return len;
}

uint32_t virtio_mmio_read_one(struct virtio_device *d,
                           struct virtq *q,
                           uint8_t *buf,
                           uint32_t len)
{
    uint16_t idx = (q->avail->idx % q->num);
    uint16_t idx_old = idx;
    uint32_t bytes_to_transfer = len;
    uint32_t pos = 0;
    uint32_t descriptor_count = 0;
    uint32_t chunk;

    while (bytes_to_transfer)
    {
        if (bytes_to_transfer > 4096)
            chunk = 4096;
        else
            chunk = bytes_to_transfer;

        bytes_to_transfer -= chunk;

        q->desc[idx].addr = ((uint32_t) &buf[pos]);
        q->desc[idx].len = chunk;

        q->desc[idx].flags = VIRTQ_DESC_F_WRITE;
        q->desc[idx].next = 0;

        arch_clean_cache_range((uintptr_t) &q->desc[idx], sizeof(q->desc[0]));

        idx = ((idx + 1) % q->num);
        pos += chunk;
        descriptor_count++;

        q->avail->ring[idx] = idx;
        q->avail->idx += 1;

        arch_clean_cache_range((uintptr_t) q->avail, sizeof(*q->avail));
        //  LOG_INFO("%u %u",q->avail->idx,q->used->idx);
        pb_write32(q->queue_index, d->base + VIRTIO_MMIO_QUEUE_NOTIFY);

        while ( (q->avail->idx != q->used->idx) )
        {
            arch_invalidate_cache_range((uintptr_t) q->avail, sizeof(*q->avail));
        }
        //  LOG_INFO("%u %u",q->avail->idx,q->used->idx);
    }

    arch_invalidate_cache_range((uintptr_t) &q->used->ring[idx_old],
                                sizeof(q->used->ring[idx_old]));

    return (q->used->ring[idx_old].len);
}


uint32_t virtio_mmio_notify_queue(struct virtio_device *d,
                                  struct virtq *q)
{
    if ((d == NULL) || (q == NULL))
    {
        LOG_ERR("q or d is NULL");
        return PB_ERR;
    }

    LOG_DBG("Queue %u, dev: %p, q: %p", q->queue_index, d, q);

    arch_clean_cache_range((uintptr_t) q, sizeof(*q));

    pb_write32((q->queue_index), (d->base + VIRTIO_MMIO_QUEUE_NOTIFY));

    return PB_OK;
}
