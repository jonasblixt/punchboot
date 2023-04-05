/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include <stdio.h>
#include <inttypes.h>
#include <pb/pb.h>
#include <pb/mmio.h>
#include <drivers/virtio/virtio_serial.h>
#include "virtio_mmio.h"
#include "virtio_queue.h"

#define VIRTIO_SERIAL_QSZ 1024

#define VIRTIO_CONSOLE_F_SIZE (1 << 0)
#define VIRTIO_CONSOLE_F_MULTIPORT (1 << 1)
#define VIRTIO_CONSOLE_F_EMERG_WRITE (1 << 2)

struct virtio_serial_config
{
    uint16_t cols;
    uint16_t rows;
    uint32_t max_nr_ports;
    uint32_t emerg_wr;
};

struct virtio_serial_control
{
    uint32_t id; /* Port number */
    uint16_t event; /* The kind of control event */
    uint16_t value; /* Extra information for the event */
};

static uint8_t rx_data[VIRTIO_QUEUE_SZ(VIRTIO_SERIAL_QSZ)] __aligned(4096);
static uint8_t tx_data[VIRTIO_QUEUE_SZ(VIRTIO_SERIAL_QSZ)] __aligned(4096);
static uint8_t ctrl_rx_data[VIRTIO_QUEUE_SZ(VIRTIO_SERIAL_QSZ)] __aligned(4096);
static uint8_t ctrl_tx_data[VIRTIO_QUEUE_SZ(VIRTIO_SERIAL_QSZ)] __aligned(4096);
static struct virtq rx;
static struct virtq tx;
static struct virtq ctrl_rx;
static struct virtq ctrl_tx;

static struct virtio_serial_control ctrlm __aligned(64);
static uintptr_t base;

static int virtio_xfer(struct virtq *q, bool read, uint32_t queue_index, uintptr_t buf, size_t length)
{
    uint16_t idx = (q->avail->idx % q->num);
    size_t bytes_to_transfer = length;
    uintptr_t buf_p = buf;
    size_t descriptor_count = 0;
    size_t chunk;

    while (bytes_to_transfer) {
        chunk = (bytes_to_transfer > 4096)?4096:bytes_to_transfer;

        bytes_to_transfer -= chunk;

        q->desc[idx].addr = (uint32_t) buf_p;
        q->desc[idx].len = chunk;

        if (bytes_to_transfer)
            q->desc[idx].flags = VIRTQ_DESC_F_NEXT;
        else
            q->desc[idx].flags = 0;

        if (read) {
            q->desc[idx].flags |= VIRTQ_DESC_F_WRITE;
        }

        if (bytes_to_transfer)
            q->desc[idx].next = (idx+1) % q->num;
        else
            q->desc[idx].next = 0;

        arch_clean_cache_range((uintptr_t) &q->desc[idx], sizeof(q->desc[0]));

        idx = ((idx+1)%q->num);
        buf_p += chunk;
        descriptor_count++;
    }

    q->avail->ring[idx] = idx;
    q->avail->idx += descriptor_count;

    arch_clean_cache_range((uintptr_t) q->avail, sizeof(*q->avail));

    mmio_write_32(base + VIRTIO_MMIO_QUEUE_NOTIFY, queue_index);

    while (q->avail->idx != q->used->idx) {
        arch_invalidate_cache_range((uintptr_t) q->avail, sizeof(*q->avail));
    }

    return PB_OK;
}

int virtio_serial_write(uintptr_t buf, size_t length)
{
    return virtio_xfer(&tx, false, 5, buf, length);
}

int virtio_serial_read(uintptr_t buf, size_t length)
{
    return virtio_xfer(&rx, true, 4, buf, length);
}

static void queue_init(struct virtq *q, uint8_t *buf, uint32_t queue_id)
{
    q->num = VIRTIO_SERIAL_QSZ;
    q->desc  = (struct virtq_desc *)  buf;
    q->avail = (struct virtq_avail *) (buf + VIRTIO_QUEUE_AVAIL_OFFSET(q->num));
    q->used  = (struct virtq_used *)  (buf + VIRTIO_QUEUE_USED_OFFSET(q->num, 4096));

    mmio_write_32(base + VIRTIO_MMIO_QUEUE_SEL, queue_id);
    mmio_write_32(base + VIRTIO_MMIO_QUEUE_NUM, q->num);
    mmio_write_32(base + VIRTIO_MMIO_QUEUE_ALIGN, 4096);
    mmio_write_32(base + VIRTIO_MMIO_QUEUE_PFN, (uint32_t) ((uintptr_t) q->desc >> 12));
}

int virtio_serial_init(uintptr_t base_)
{
    uint32_t device_id;
    uint32_t features;
    base = base_;

    device_id = mmio_read_32(base + VIRTIO_MMIO_DEVICE_ID);

    if (device_id != 3) {
        LOG_ERR("Device id of %"PRIxPTR" is %u, expected 3", base, device_id);
        return -PB_ERR_IO;
    }

    features = mmio_read_32(base + VIRTIO_MMIO_DEVICE_FEATURES);

    LOG_INFO("Detected virtio serial @%"PRIxPTR", features = 0x%x",
                base, features);

    mmio_write_32(base + VIRTIO_MMIO_STATUS, 0);
    mmio_clrsetbits_32(base + VIRTIO_MMIO_STATUS, 0,
                        VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    // TODO: Why do we need this feature?
    if (features & VIRTIO_CONSOLE_F_EMERG_WRITE) {
        mmio_write_32(base + VIRTIO_MMIO_DEVICE_FEATURES_SEL, VIRTIO_CONSOLE_F_EMERG_WRITE);
        mmio_clrsetbits_32(base + VIRTIO_MMIO_STATUS, 0, VIRTIO_STATUS_FEATURES_OK);
    }

    mmio_write_32(base + VIRTIO_MMIO_GUEST_PAGE_SIZE, 4096);

    struct virtio_serial_config *cfg = \
                     (struct virtio_serial_config *) (base + VIRTIO_MMIO_CONFIG);

    cfg->cols = 0;
    cfg->rows = 0;
    cfg->max_nr_ports = 1;
    cfg->emerg_wr = 1;

    /* Initiailze queue */
    queue_init(&rx, rx_data, 4);
    queue_init(&tx, tx_data, 5);
    queue_init(&ctrl_rx, ctrl_rx_data, 2);
    queue_init(&ctrl_tx, ctrl_tx_data, 3);

    mmio_clrsetbits_32(base + VIRTIO_MMIO_STATUS, 0, VIRTIO_STATUS_DRIVER_OK);

    ctrlm.id = 1;
    ctrlm.event = 6;
    ctrlm.value = 1;

    return virtio_xfer(&ctrl_tx, false, 3, (uintptr_t) &ctrlm,
                       sizeof(struct virtio_serial_control));
}
