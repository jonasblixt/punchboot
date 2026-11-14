/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/uart/imx_uart.h>
#include <pb/mmio.h>
#include <pb/pb.h>

void imx_uart_putc(uintptr_t base, char c)
{
    uint32_t usr2;

    for (;;) {
        usr2 = mmio_read_32(base + USR2);

        if (usr2 & USR2_TXDC)
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

    mmio_write_32(base + UCR3, UCR3_DSR | UCR3_DCD | UCR3_RI | UCR3_RXDMUXSEL);
    mmio_write_32(base + UCR4, UCR4_CTSTL(32));
    mmio_write_32(base + UESC, UESC_ESCAPE);
    mmio_write_32(base + UTIM, 0);
    mmio_write_32(base + UTS, 0);
    mmio_write_32(base + UFCR, UFCR_RFDIV(UFCR_RFDIV_1));
    mmio_write_32(base + UBIR, UBIR_INC);
    mmio_write_32(base + UBMR, (uint32_t)(input_clock_Hz / (2 * baudrate)));
    mmio_write_32(base + UCR2, (UCR2_WS | UCR2_IRTS | UCR2_RXEN | UCR2_TXEN | UCR2_SRST));
    mmio_write_32(base + UCR1, UCR1_UARTEN);

    return PB_OK;
}
