/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <stdio.h>
#include <pb/fuse.h>
#include <plat/qemu/test_fuse.h>
#include <plat/qemu/virtio.h>
#include <plat/qemu/virtio_block.h>

#define TEST_FUSE_MAX 16

static uint32_t _fuse_box[TEST_FUSE_MAX];

uint32_t test_fuse_init(struct virtio_block_device *dev)
{
    LOG_DBG("using block device %p", dev);

    if (dev == NULL)
        return PB_ERR;
return PB_OK;
//    return virtio_block_read(dev, 0,
//                        (uintptr_t) _fuse_box, 1);
}

uint32_t test_fuse_write(struct virtio_block_device *dev,
                        uint32_t id, uint32_t val)
{
    if (id >= TEST_FUSE_MAX)
        return PB_ERR;

    _fuse_box[id] |= val;

    LOG_DBG("_fuse_box[%u] = %x, blkdev: %p", id, _fuse_box[id], dev);
    return PB_OK;
    //return virtio_block_write(dev, 0, (uintptr_t) _fuse_box, 1);
}


uint32_t test_fuse_read(uint32_t id, uint32_t *val)
{
    if (id >= TEST_FUSE_MAX)
        return PB_ERR;

    *val = _fuse_box[id];

    return PB_OK;
}
