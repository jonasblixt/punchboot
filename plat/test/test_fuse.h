/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_TEST_TEST_FUSE_H_
#define PLAT_TEST_TEST_FUSE_H_

#include <pb.h>
#include <plat/test/virtio_block.h>

uint32_t test_fuse_init(struct virtio_block_device *dev);
uint32_t test_fuse_write(struct virtio_block_device *dev,
                                uint32_t id, uint32_t val);
uint32_t test_fuse_read(uint32_t id, uint32_t *val);

#endif  // PLAT_TEST_TEST_FUSE_H_
