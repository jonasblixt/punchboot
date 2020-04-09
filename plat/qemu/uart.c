/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <pb/plat.h>
#include <pb/io.h>
#include "uart.h"

static int qemu_uart_write(struct pb_console_driver *drv, char *buf, size_t size)
{
    struct qemu_uart_device *dev = (struct qemu_uart_device*) drv->private;

    for (unsigned int i = 0; i < size; i++)
        pb_write32(buf[i], dev->base); /* Transmit char */

    return PB_OK;
}

static int qemu_uart_init(struct pb_console_driver *drv)
{
    drv->ready = true;
    return PB_OK;
}

int qemu_uart_setup(struct pb_console_driver *drv)
{
    drv->write = qemu_uart_write;
    drv->init = qemu_uart_init;
    return PB_OK;
}
