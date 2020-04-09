#include <pb/pb.h>
#include <pb/transport.h>
#include <plat/qemu/virtio_serial.h>

static int transport_write(struct pb_transport_driver *drv,
                                void *buf, size_t size)
{
    struct virtio_serial_device *dev = \
                     (struct virtio_serial_device *) drv->private;

    LOG_DBG("%p, %zu", buf, size);

    size_t bytes_to_write = size;
    size_t chunk = 0;
    size_t written = 0;
    uint8_t *p = (uint8_t *) buf;

    while (bytes_to_write)
    {
        chunk = (bytes_to_write > drv->max_chunk_bytes) ? \
                        drv->max_chunk_bytes:bytes_to_write;

        written += virtio_serial_write(dev, p, chunk);

        bytes_to_write -= chunk;
        p += chunk;
    }

    if (written == size)
        return PB_OK;
    else
    {
        LOG_ERR("Wanted to write %zu but only %zu bytes was written",
                                size, written);
        return -PB_ERR;
    }
}

static int transport_read(struct pb_transport_driver *drv,
                                void *buf, size_t size)
{
    struct virtio_serial_device *dev = \
                     (struct virtio_serial_device *) drv->private;

    LOG_DBG("%p, %zu", buf, size);

    size_t bytes_to_read = size;
    size_t chunk = 0;
    size_t read_bytes = 0;
    uint8_t *p = (uint8_t *) buf;

    while (bytes_to_read)
    {
        chunk = (bytes_to_read > drv->max_chunk_bytes) ? \
                        drv->max_chunk_bytes:bytes_to_read;

        read_bytes += virtio_serial_read(dev, p, chunk);

        bytes_to_read -= chunk;
        p += chunk;
    }

    if (read_bytes == size)
        return PB_OK;
    else
    {
        LOG_ERR("Wanted to read %zu but got %zu bytes", size, read_bytes);
        return -PB_ERR;
    }
}

static int transport_init(struct pb_transport_driver *drv)
{
    struct virtio_serial_device *dev = \
                     (struct virtio_serial_device *) drv->private;

    return virtio_serial_init(dev);
}

static int transport_process(struct pb_transport_driver *drv)
{
    return PB_OK;
}

int virtio_serial_transport_setup(struct pb_transport *transport,
                                  struct pb_transport_driver *drv)
{
    transport->driver = drv;

    drv->max_chunk_bytes = 4096;
    drv->init = transport_init;
    drv->write = transport_write;
    drv->read = transport_read;
    drv->process = transport_process;
    drv->ready = true;

    return PB_OK;
}
