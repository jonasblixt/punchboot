/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <pb/pb.h>
#include <pb/mmio.h>
#include <drivers/uart/imx_uart.h>

void imx_uart_putc(uintptr_t base, char c)
{
    uint32_t usr2;

    for (;;) {
        usr2 = mmio_read_32(base + USR2);

        if (usr2 & (1<<3))
            break;
    }

    mmio_write_32(base + UTXD, c);
}

int imx_uart_init(uintptr_t base, unsigned int input_clock_Hz, unsigned int baudrate)
{
    uint32_t reg;

    mmio_write_32(base + UCR1, 0);
    mmio_write_32(base + UCR2, 0);

    for (;;) {
        reg = mmio_read_32(base + UCR2);

        if ((reg & UCR2_SRST) == UCR2_SRST)
            break;
    }

    mmio_write_32(base + UCR3, 0x0704);
    mmio_write_32(base + UCR4, 0x8000);
    mmio_write_32(base + UESC, 0x002b);
    mmio_write_32(base + UTIM, 0x0000);
    mmio_write_32(base + UTS, 0x0000);
    mmio_write_32(base + UFCR, (4 << 7));
    mmio_write_32(base + UBIR, 0x000f);
    mmio_write_32(base + UBMR, (uint32_t)(input_clock_Hz / (2 * baudrate)));
    mmio_write_32(base + UCR2, (UCR2_WS | UCR2_IRTS | UCR2_RXEN | UCR2_TXEN | UCR2_SRST));
    mmio_write_32(base + UCR1, UCR1_UARTEN);

    return PB_OK;
}
