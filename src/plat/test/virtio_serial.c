/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include <stdio.h>
#include <pb.h>
#include <io.h>
#include <plat/test/virtio_serial.h>
#include <plat/test/virtio_queue.h>


uint32_t virtio_serial_init(struct virtio_serial_device *d)
{
    if (virtio_mmio_init(&d->dev) != PB_OK)
        return PB_ERR;

    uint32_t cap = virtio_mmio_get_features(&d->dev);

    if (cap & VIRTIO_CONSOLE_F_EMERG_WRITE)
    {
        if (virtio_mmio_select_features(&d->dev, VIRTIO_CONSOLE_F_EMERG_WRITE)
                                                                    != PB_OK)
        {
            LOG_ERR("Could not select features");
            return PB_ERR;
        }
    }

    d->config = (struct virtio_serial_config *) (d->dev.base + 0x100);

    d->config->cols = 0;
    d->config->rows = 0;
    d->config->max_nr_ports = 1;
    d->config->emerg_wr = 1;

    virtio_init_queue(d->_rx_data, VIRTIO_SERIAL_QSZ, &d->rx);
    virtio_init_queue(d->_tx_data, VIRTIO_SERIAL_QSZ, &d->tx);
    virtio_init_queue(d->_ctrl_rx_data, VIRTIO_SERIAL_QSZ, &d->ctrl_rx);
    virtio_init_queue(d->_ctrl_tx_data, VIRTIO_SERIAL_QSZ, &d->ctrl_tx);

    virtio_mmio_init_queue(&d->dev, &d->rx, 4, VIRTIO_SERIAL_QSZ);
    virtio_mmio_init_queue(&d->dev, &d->tx, 5, VIRTIO_SERIAL_QSZ);
    virtio_mmio_init_queue(&d->dev, &d->ctrl_rx, 2, VIRTIO_SERIAL_QSZ);
    virtio_mmio_init_queue(&d->dev, &d->ctrl_tx, 3, VIRTIO_SERIAL_QSZ);


    virtio_mmio_driver_ok(&d->dev);


    struct virtio_serial_control ctrlm;
    ctrlm.id = 1;
    ctrlm.event = 6;
    ctrlm.value = 1;

    virtio_mmio_write_one(&d->dev, &d->ctrl_tx, (uint8_t *) &ctrlm,
                        sizeof(struct virtio_serial_control));


    return PB_OK;
}


uint32_t virtio_serial_write(struct virtio_serial_device *d, uint8_t *buf,
                                                        uint32_t len)
{
    return virtio_mmio_write_one(&d->dev, &d->tx, buf, len);
}

uint32_t virtio_serial_read(struct virtio_serial_device *d, uint8_t *buf,
                                                        uint32_t len)
{
    return virtio_mmio_read_one(&d->dev, &d->rx, buf, len);
}

