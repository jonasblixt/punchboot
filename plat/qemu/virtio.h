/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_TEST_VIRTIO_H_
#define PLAT_TEST_VIRTIO_H_

#include <stdint.h>
#include <plat/qemu/virtio_queue.h>

#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_DRIVER_OK   4
#define VIRTIO_STATUS_FAILED      128

struct virtio_device {
    uint32_t base;
    uint32_t device_id;
    uint32_t vendor_id;
    uint32_t status;
    uint32_t page_size;
};

uint32_t virtio_mmio_init(struct virtio_device *d);
uint32_t virtio_mmio_get_features(struct virtio_device *d);
uint32_t virtio_mmio_select_features(struct virtio_device *d, uint32_t feat);

uint32_t virtio_mmio_init_queue(struct virtio_device *d,
                                struct virtq *q,
                                uint32_t queue_index,
                                uint32_t queue_sz);

uint32_t virtio_mmio_driver_ok(struct virtio_device *d);

uint32_t virtio_mmio_write_one(struct virtio_device *d,
                           struct virtq *q,
                           uint8_t *buf,
                           uint32_t len);


uint32_t virtio_mmio_read_one(struct virtio_device *d,
                           struct virtq *q,
                           uint8_t *buf,
                           uint32_t len);

uint32_t virtio_mmio_notify_queue(struct virtio_device *d,
                                  struct virtq *q);
#endif  // PLAT_TEST_VIRTIO_H_
