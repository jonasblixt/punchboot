/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_DRIVERS_VIRTIO_BLOCK_H
#define INCLUDE_DRIVERS_VIRTIO_BLOCK_H

#include <stdint.h>
#include <uuid.h>
#include <drivers/block/bio.h>

bio_dev_t virtio_block_init(uintptr_t base, uuid_t uu);

#endif  // INCLUDE_DRIVERS_VIRTIO_BLOCK_H
