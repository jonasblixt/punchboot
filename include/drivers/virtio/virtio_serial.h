/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef DRIVERS_VIRTIO_SERIAL_H
#define DRIVERS_VIRTIO_SERIAL_H

#include <stdint.h>

int virtio_serial_init(uintptr_t base);
int virtio_serial_write(const void *buf, size_t length);
int virtio_serial_read(void *buf, size_t length);

#endif  // DRIVERS_VIRTIO_SERIAL_H
