#include <board.h>
#include <io.h>
#include <tinyprintf.h>
#include "virtio.h"
#include "virtio_mmio.h"
#include "virtio_queue.h"


__a4k struct virtq_desc in_desc;
__a4k struct virtq_desc out_desc;

uint32_t virtio_mmio_init(struct virtio_device *d)
{
    if (pb_readl(d->base + VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976)
        return PB_ERR;

    LOG_INFO ("Found VIRTIO @ 0x%8.8X, type: 0x%8.8X", d->base,
                pb_readl(d->base + VIRTIO_MMIO_DEVICE_ID));



    LOG_INFO ("Features: 0x%8.8X",pb_readl(d->base + VIRTIO_MMIO_DEVICE_FEATURES));



    uint32_t q_size ;

    pb_writel(0, d->base + VIRTIO_MMIO_QUEUE_SEL);


    if (pb_readl(d->base + VIRTIO_MMIO_QUEUE_READY) != 0)
    {
        LOG_ERR("VIRTIO Queue 0, not ready");
        return PB_ERR;
    }

    q_size = pb_readl(d->base + VIRTIO_MMIO_QUEUE_NUM_MAX);

    LOG_INFO("Queue 0 size: %lu", q_size);

    pb_writel(1, d->base + VIRTIO_MMIO_QUEUE_SEL);


    if (pb_readl(d->base + VIRTIO_MMIO_QUEUE_READY) != 0)
    {
        LOG_ERR("VIRTIO Queue 1, not ready");
        return PB_ERR;
    }

    q_size = pb_readl(d->base + VIRTIO_MMIO_QUEUE_NUM_MAX);

    LOG_INFO("Queue 1 size: %lu", q_size);
    return PB_OK;
}
