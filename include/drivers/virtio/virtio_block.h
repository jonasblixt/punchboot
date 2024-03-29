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

#include <pb/bio.h>
#include <stdint.h>
#include <uuid.h>

bio_dev_t virtio_block_init(uintptr_t base, const uuid_t uu);

#endif // INCLUDE_DRIVERS_VIRTIO_BLOCK_H
