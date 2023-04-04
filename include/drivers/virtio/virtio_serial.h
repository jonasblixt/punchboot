/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_TEST_VIRTIO_SERIAL_H_
#define PLAT_TEST_VIRTIO_SERIAL_H_

#include <stdint.h>
#include <plat/qemu/virtio_mmio.h>
#include <plat/qemu/virtio_queue.h>
#include <plat/qemu/virtio.h>

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

struct virtio_serial_device
{
    uint8_t _rx_data[VIRTIO_QUEUE_SZ_WITH_PADDING(VIRTIO_SERIAL_QSZ+1)] PB_ALIGN_4k;
    uint8_t _tx_data[VIRTIO_QUEUE_SZ_WITH_PADDING(VIRTIO_SERIAL_QSZ+1)] PB_ALIGN_4k;
    uint8_t _ctrl_rx_data[VIRTIO_QUEUE_SZ_WITH_PADDING(VIRTIO_SERIAL_QSZ+1)] PB_ALIGN_4k;
    uint8_t _ctrl_tx_data[VIRTIO_QUEUE_SZ_WITH_PADDING(VIRTIO_SERIAL_QSZ+1)] PB_ALIGN_4k;
    struct virtio_device dev;
    struct virtio_serial_config *config;
    struct virtq rx;
    struct virtq tx;
    struct virtq ctrl_rx;
    struct virtq ctrl_tx;
};

int virtio_serial_init(struct virtio_serial_device *d);
int virtio_serial_write(struct virtio_serial_device *d, void *buf,
                                                        size_t len);
int virtio_serial_read(struct virtio_serial_device *d, void *buf,
                                                       size_t len);
#endif  // PLAT_TEST_VIRTIO_SERIAL_H_
