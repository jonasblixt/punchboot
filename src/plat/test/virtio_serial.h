/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __VIRTIO_SERIAL_H__
#define __VIRTIO_SERIAL_H__

#include <stdint.h>
#include <plat/test/virtio_mmio.h>
#include <plat/test/virtio_queue.h>
#include <plat/test/virtio.h>

#define VIRTIO_SERIAL_QSZ 1024

#define VIRTIO_CONSOLE_F_SIZE (1 << 0)
#define VIRTIO_CONSOLE_F_MULTIPORT (1 << 1)
#define VIRTIO_CONSOLE_F_EMERG_WRITE (1 << 2)

struct virtio_serial_config {
    uint16_t cols;
    uint16_t rows;
    uint32_t max_nr_ports;
    uint32_t emerg_wr;
};

struct virtio_serial_control {
    uint32_t id; /* Port number */
    uint16_t event; /* The kind of control event */
    uint16_t value; /* Extra information for the event */
};



struct virtio_serial_device {
    struct virtio_device dev;
    struct virtio_serial_config *config;

    __a4k uint8_t _rx_data[VIRTIO_QUEUE_SZ_WITH_PADDING(VIRTIO_SERIAL_QSZ+1)];
    __a4k uint8_t _tx_data[VIRTIO_QUEUE_SZ_WITH_PADDING(VIRTIO_SERIAL_QSZ+1)];
    __a4k uint8_t _ctrl_rx_data[VIRTIO_QUEUE_SZ_WITH_PADDING(VIRTIO_SERIAL_QSZ+1)];
    __a4k uint8_t _ctrl_tx_data[VIRTIO_QUEUE_SZ_WITH_PADDING(VIRTIO_SERIAL_QSZ+1)];

    struct virtq rx;
    struct virtq tx;
    struct virtq ctrl_rx;
    struct virtq ctrl_tx;
};

uint32_t virtio_serial_init(struct virtio_serial_device *d);
uint32_t virtio_serial_write(struct virtio_serial_device *d, uint8_t *buf,
                                                        uint32_t len);
uint32_t virtio_serial_read(struct virtio_serial_device *d, uint8_t *buf,
                                                        uint32_t len);
#endif
