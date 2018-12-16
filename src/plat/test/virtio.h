#ifndef __VIRTIO_H__
#define __VIRTIO_H__

#include <stdint.h>

struct virtio_device {
    uint32_t base;
    uint32_t queue_max_elements;
};

uint32_t virtio_mmio_init(struct virtio_device *d);

#endif
