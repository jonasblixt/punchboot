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

#define VIRTIO_SERIAL_QSZ 256

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

static uint8_t rx_data[VIRTIO_QUEUE_SZ(VIRTIO_SERIAL_QSZ, 4096)] __aligned(4096);
static uint8_t tx_data[VIRTIO_QUEUE_SZ(VIRTIO_SERIAL_QSZ, 4096)] __aligned(4096);
static uint8_t ctrl_rx_data[VIRTIO_QUEUE_SZ(VIRTIO_SERIAL_QSZ, 4096)] __aligned(4096);
static uint8_t ctrl_tx_data[VIRTIO_QUEUE_SZ(VIRTIO_SERIAL_QSZ, 4096)] __aligned(4096);
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
    size_t chunk;
    size_t desc_count = 0;

    //LOG_DBG("%s %u <%p> %zu", read?"R":"W", queue_index, (void *) buf, length);

    /**
     * Note: For some reason qemu/virtio-serial seems to limit the total amount
     * of bytes that can be transfered with a chained descriptor to 4k.
     *
     * Tested various chunk sizes and the total length in the used ring
     * always cap's at 4k.
     */

    while (bytes_to_transfer) {
        chunk = (bytes_to_transfer > 4096)?4096:bytes_to_transfer;

        bytes_to_transfer -= chunk;

        q->desc[desc_count].addr = (uint32_t) buf_p;
        q->desc[desc_count].len = chunk;
        q->desc[desc_count].flags = BIT(7);

        if (bytes_to_transfer) {
            q->desc[desc_count].flags = VIRTQ_DESC_F_NEXT;
            q->desc[desc_count].next = (desc_count + 1) % q->num;
        } else {
            q->desc[desc_count].next = 0;
        }

        if (read) {
            q->desc[desc_count].flags |= VIRTQ_DESC_F_WRITE;
        }
/*
        uint16_t f = q->desc[desc_count].flags;
        LOG_DBG("desc %i, sz=%zu, p=%p nxt=%i (%s%s) %s",
                    desc_count,
                    chunk,
                    (void *) buf_p,
                    q->desc[desc_count].next,
                    (f & VIRTQ_DESC_F_NEXT)?"N":"",
                    (f & VIRTQ_DESC_F_WRITE)?"W":"R",
                    (bytes_to_transfer == 0)?"LAST":"");
*/
        arch_clean_cache_range((uintptr_t) &q->desc[desc_count], sizeof(q->desc[0]));
        desc_count++;
        memset(&q->desc[desc_count], 0, sizeof(q->desc[0]));
        buf_p += chunk;
    }

    q->avail->flags = VIRTQ_AVAIL_F_NO_INTERRUPT;
    q->avail->ring[idx] = 0;
    q->avail->idx++;
    LOG_DBG("avail->ring[%u] = 0, q->avail->idx=%u", idx, q->avail->idx);

    //LOG_DBG("Submitting %u descriptors", last_idx - first_idx + 1);
    arch_clean_cache_range((uintptr_t) q->avail, sizeof(*q->avail));

    //printf("q[%u]->avail: idx=%u, flgs=0x%x\n\r", queue_index, q->avail->idx, q->avail->flags);

    // TODO: memory barrier?

    mmio_write_32(base + VIRTIO_MMIO_QUEUE_NOTIFY, queue_index);

    while ((q->avail->idx != q->used->idx) ) {
        arch_invalidate_cache_range((uintptr_t) q->used, sizeof(*q->used));
    }

    printf("q[%u]->used: idx=%u, flgs=0x%x\n\r", queue_index, q->used->idx, q->used->flags);
/*    for (int n = 0; n < q->used->idx + 1; n++)
        printf("ring[%i] = <%u, %u>\n\r", n, q->used->ring[n].id,
                                            q->used->ring[n].len);
*/
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
    q->avail = (struct virtq_avail *) (buf + VIRTIO_QUEUE_AVAIL_OFFSET(q->num, 4096));
    q->used  = (struct virtq_used *)  (buf + VIRTIO_QUEUE_USED_OFFSET(q->num, 4096));

    mmio_write_32(base + VIRTIO_MMIO_QUEUE_SEL, queue_id);
    LOG_DBG("Queue %u sz=%u", queue_id, mmio_read_32(base + VIRTIO_MMIO_QUEUE_NUM_MAX));
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
#ifdef __NOPE
    if (features & VIRTIO_CONSOLE_F_EMERG_WRITE) {
        mmio_write_32(base + VIRTIO_MMIO_DEVICE_FEATURES_SEL, VIRTIO_CONSOLE_F_EMERG_WRITE);
        mmio_clrsetbits_32(base + VIRTIO_MMIO_STATUS, 0, VIRTIO_STATUS_FEATURES_OK);
    }
#endif
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
/*
    virtio_xfer(&ctrl_rx, true, 2, (uintptr_t) &ctrlm, sizeof(ctrlm));
    LOG_DBG("Ctrlm %i %i", ctrlm.id, ctrlm.value);
*/
    ctrlm.id = 1;
    ctrlm.event = VIRTIO_CONSOLE_PORT_OPEN;
    ctrlm.value = 1;

    return virtio_xfer(&ctrl_tx, false, 3, (uintptr_t) &ctrlm,
                       sizeof(struct virtio_serial_control));

}
