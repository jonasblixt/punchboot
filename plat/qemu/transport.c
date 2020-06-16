#include <pb/pb.h>
#include <pb/plat.h>
#include <plat/qemu/virtio_serial.h>

static struct virtio_serial_device dev __a4k =
{
    .dev =
    {
        .device_id = 3,
        .vendor_id = 0x554D4551,
        .base = 0x0A003E00,
    },
};

int plat_transport_write(void *buf, size_t size)
{
    size_t bytes_to_write = size;
    size_t chunk = 0;
    size_t written = 0;
    uint8_t *p = (uint8_t *) buf;

    while (bytes_to_write)
    {
        chunk = (bytes_to_write > (CONFIG_TRANSPORT_MAX_CHUNK_KB*1024)) ? \
            (CONFIG_TRANSPORT_MAX_CHUNK_KB*1024):bytes_to_write;

        written += virtio_serial_write(&dev, p, chunk);

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

int plat_transport_read(void *buf, size_t size)
{
    size_t bytes_to_read = size;
    size_t chunk = 0;
    size_t read_bytes = 0;
    uint8_t *p = (uint8_t *) buf;

    while (bytes_to_read)
    {
        chunk = (bytes_to_read > (CONFIG_TRANSPORT_MAX_CHUNK_KB*1024)) ? \
                (CONFIG_TRANSPORT_MAX_CHUNK_KB*1024):bytes_to_read;

        read_bytes += virtio_serial_read(&dev, p, chunk);

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

int plat_transport_init(void)
{
    return virtio_serial_init(&dev);
}

int plat_transport_process(void)
{
    return PB_OK;
}

bool plat_transport_ready(void)
{
    return true;
}
