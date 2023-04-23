/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <pb/pb.h>
#include <pb/mmio.h>
#include <arch/arch.h>
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

#define VIRTIO_CONSOLE_PORT_OPEN (6)

static uint8_t rx_data[VIRTIO_QUEUE_SZ(VIRTIO_SERIAL_QSZ, 64)] __aligned(4096);
static uint8_t tx_data[VIRTIO_QUEUE_SZ(VIRTIO_SERIAL_QSZ, 64)] __aligned(4096);
static uint8_t ctrl_rx_data[VIRTIO_QUEUE_SZ(VIRTIO_SERIAL_QSZ, 64)] __aligned(4096);
static uint8_t ctrl_tx_data[VIRTIO_QUEUE_SZ(VIRTIO_SERIAL_QSZ, 64)] __aligned(4096);
static struct virtq rx;
static struct virtq tx;
static struct virtq ctrl_rx;
static struct virtq ctrl_tx;

static struct virtio_serial_control ctrlm __aligned(64);
static uintptr_t base;

static int virtio_xfer(struct virtq *q, bool read, uint32_t queue_index, uintptr_t buf, size_t length)
{
    size_t bytes_to_transfer = length;
    uintptr_t buf_p = buf;
    uint16_t idx;
    size_t chunk;
    size_t desc_count = 0;

    if (length > (VIRTIO_SERIAL_QSZ * 4096))
        return -PB_ERR_IO;

    //LOG_DBG("%s %u <%p> %zu", read?"R":"W", queue_index, (void *) buf, length);

    /**
     * Note: For some reason qemu/virtio-serial seems to limit the total amount
     * of bytes that can be transfered with a chained descriptor to 4k.
     *
     * Tested various chunk sizes and the total length in the used ring
     * always cap's at 4k.
     *
     * There also seems to be a 4k limit per descriptor as well
     *
     * So instead of chaining descriptors we create as many 4k descriptors
     * as we need without chaining them together.
     */

    while (bytes_to_transfer) {
        idx = (q->avail->idx % q->num);
        chunk = (bytes_to_transfer > 4096)?4096:bytes_to_transfer;
        bytes_to_transfer -= chunk;

        q->desc[desc_count].addr = (uint32_t) buf_p;
        q->desc[desc_count].len = chunk;
        q->desc[desc_count].flags = (read?VIRTQ_DESC_F_WRITE:0) | BIT(7);

        arch_clean_cache_range((uintptr_t) &q->desc[desc_count], sizeof(q->desc[0]));
        q->avail->ring[idx] = desc_count;
        q->avail->idx++;
        desc_count++;
        buf_p += chunk;
    }

    arch_clean_cache_range((uintptr_t) q->avail, sizeof(*q->avail));
    mmio_write_32(base + VIRTIO_MMIO_QUEUE_NOTIFY, queue_index);

    while (q->avail->idx != q->used->idx) {
        arch_invalidate_cache_range((uintptr_t) q->used, sizeof(*q->used));
    }

    return PB_OK;
}

int virtio_serial_write(const void *buf, size_t length)
{
    return virtio_xfer(&tx, false, 5, (uintptr_t) buf, length);
}

int virtio_serial_read(void *buf, size_t length)
{
    return virtio_xfer(&rx, true, 4, (uintptr_t) buf, length);
}

static void queue_init(struct virtq *q, uint8_t *buf, uint32_t queue_id)
{
    q->num = VIRTIO_SERIAL_QSZ;
    q->desc  = (struct virtq_desc *)  buf;
    q->avail = (struct virtq_avail *) (buf + VIRTIO_QUEUE_AVAIL_OFFSET(q->num, 64));
    q->used  = (struct virtq_used *)  (buf + VIRTIO_QUEUE_USED_OFFSET(q->num, 64));

    mmio_write_32(base + VIRTIO_MMIO_QUEUE_SEL, queue_id);
    mmio_write_32(base + VIRTIO_MMIO_QUEUE_NUM, q->num);
    mmio_write_32(base + VIRTIO_MMIO_QUEUE_ALIGN, 64);
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

    mmio_write_32(base + VIRTIO_MMIO_DEVICE_FEATURES_SEL, VIRTIO_F_EVENT_IDX);
    mmio_write_32(base + VIRTIO_MMIO_DEVICE_FEATURES_SEL, 0);
    mmio_write_32(base + VIRTIO_MMIO_GUEST_PAGE_SIZE, 4096);

    struct virtio_serial_config *cfg = \
                     (struct virtio_serial_config *) (base + VIRTIO_MMIO_CONFIG);

    cfg->cols = 0;
    cfg->rows = 0;
    cfg->max_nr_ports = 0;
    cfg->emerg_wr = 0;

    mmio_clrsetbits_32(base + VIRTIO_MMIO_STATUS, 0, VIRTIO_STATUS_FEATURES_OK);

    /* Initiailze queue */
    queue_init(&rx, rx_data, 4);
    queue_init(&tx, tx_data, 5);
    queue_init(&ctrl_rx, ctrl_rx_data, 2);
    queue_init(&ctrl_tx, ctrl_tx_data, 3);

    mmio_clrsetbits_32(base + VIRTIO_MMIO_STATUS, 0, VIRTIO_STATUS_DRIVER_OK);

    ctrlm.id = 1;
    ctrlm.event = VIRTIO_CONSOLE_PORT_OPEN;
    ctrlm.value = 1;

    return virtio_xfer(&ctrl_tx, false, 3, (uintptr_t) &ctrlm,
                       sizeof(struct virtio_serial_control));

}
