
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <pb/io.h>
#include <pb/console.h>
#include <plat/imx/lpuart.h>


static int lpuart_write(struct pb_console_driver *drv, char *buf, size_t size)
{
    struct imx_lpuart_device *dev = (struct imx_lpuart_device *) drv->private;

    for (unsigned int i = 0; i < size; i++)
    {
        while (!(pb_read32(dev->base + STAT) & (1 << 22)))
            __asm__("nop");

        pb_write32(buf[i], dev->base + DATA);
    }

    return PB_OK;
}


int imx_lpuart_free(struct pb_console_driver *drv)
{
    return PB_OK;
}

int imx_lpuart_init(struct pb_console_driver *drv)
{
    struct imx_lpuart_device *dev = (struct imx_lpuart_device *) drv->private;
    uint32_t tmp;

    tmp = pb_read32(dev->base + CTRL);
    tmp &= ~(CTRL_TE | CTRL_RE);
    pb_write32(tmp, dev->base + CTRL);

    pb_write32(0, dev->base + MODIR);
    pb_write32(~(FIFO_TXFE | FIFO_RXFE), dev->base + FIFO);

    pb_write32(0, dev->base + MATCH);

    pb_write32(dev->baudrate, dev->base + BAUD);


    tmp = pb_read32(dev->base + CTRL);
    tmp &= ~(LPUART_CTRL_PE_MASK | LPUART_CTRL_PT_MASK | LPUART_CTRL_M_MASK);
    pb_write32(tmp, dev->base + CTRL);

    pb_write32(CTRL_RE | CTRL_TE, dev->base + CTRL);

    drv->write = lpuart_write;
    drv->ready = true;

    return PB_OK;
}


