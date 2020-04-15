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

static __iomem base;

void imx_uart_putc(char c)
{
    volatile uint32_t usr2;

    for (;;) {
        usr2 = pb_read32(base+USR2);

        if (usr2 & (1<<3))
            break;
    }
    pb_write32(c, base+UTXD);
}

int imx_uart_init(__iomem base_addr, unsigned int baudrate)
{
    volatile uint32_t reg;

    base = base_addr;

    pb_write32(0, base+UCR1);
    pb_write32(0, base+UCR2);


    for (;;)
    {
        reg = pb_read32(base+UCR2);

        if ((reg & UCR2_SRST) == UCR2_SRST)
            break;
    }

    pb_write32(0x0704, base + UCR3);
    pb_write32(0x8000, base + UCR4);
    pb_write32(0x002b, base + UESC);
    pb_write32(0x0000, base + UTIM);
    pb_write32(0x0000, base + UTS);
    pb_write32((4 << 7), base + UFCR);
    pb_write32(0x000f, base + UBIR);

    pb_write32((uint32_t)(80000000L / (2 * baudrate)), base + UBMR);
    pb_write32((UCR2_WS | UCR2_IRTS | UCR2_RXEN |
                UCR2_TXEN | UCR2_SRST), base + UCR2);

    pb_write32(UCR1_UARTEN, base + UCR1);

    return PB_OK;
}
