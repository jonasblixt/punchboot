#ifndef __TEST_FUSE_H__
#define __TEST_FUSE_H__

#include <pb.h>
#include <plat/test/virtio_block.h>

uint32_t test_fuse_init(struct virtio_block_device *dev);
uint32_t test_fuse_write(struct virtio_block_device *dev,
                                uint32_t id, uint32_t val);
uint32_t test_fuse_read(uint32_t id, uint32_t *val);

#endif
