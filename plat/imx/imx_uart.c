/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/io.h>
#include <plat/imx/imx_uart.h>

struct imx_uart_device *_dev;

void imx_uart_putc(char c)
{
    volatile uint32_t usr2;

    for (;;) {
        usr2 = pb_read32(_dev->base+USR2);

        if (usr2 & (1<<3))
            break;
    }
    pb_write32(c, _dev->base+UTXD);
}

uint32_t imx_uart_init(struct imx_uart_device *dev)
{
    volatile uint32_t reg;
    _dev = dev;

    if (dev == NULL)
        return PB_ERR;

    pb_write32(0, _dev->base+UCR1);
    pb_write32(0, _dev->base+UCR2);


    for (;;)
    {
        reg = pb_read32(_dev->base+UCR2);

        if ((reg & UCR2_SRST) == UCR2_SRST)
            break;
    }

    pb_write32(0x0704, _dev->base+ UCR3);
    pb_write32(0x8000, _dev->base+ UCR4);
    pb_write32(0x002b, _dev->base+ UESC);
    pb_write32(0x0000, _dev->base+ UTIM);
    pb_write32(0x0000, _dev->base+ UTS);
    pb_write32((4 << 7), _dev->base+ UFCR);
    pb_write32(0x000f, _dev->base+ UBIR);
    pb_write32(dev->baudrate, _dev->base+ UBMR);
    pb_write32((UCR2_WS | UCR2_IRTS | UCR2_RXEN |
                UCR2_TXEN | UCR2_SRST), _dev->base+ UCR2);

    pb_write32(UCR1_UARTEN, _dev->base+ UCR1);

    return PB_OK;
}
