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
#include <pb/mmio_poll.h>
#include <pb/pb.h>
#include <stdbool.h>

/* A transmitter that never finished a character: the console prints nothing
 * more rather than wait the bound for each (F-462) */
static bool stuck;

void imx_uart_putc(uintptr_t base, char c)
{
    /* Counted, not timed: the console may print before the timer runs (the
     * 8M SPL's, drivers.md 3) */
    if (stuck || mmio_poll_32(base + USR2, USR2_TXDC, USR2_TXDC, MMIO_POLL_UART_CHAR) != PB_OK) {
        stuck = true;
        return;
    }

    mmio_write_32(base + UTXD, c);
}

int imx_uart_init(uintptr_t base, unsigned int input_clock_Hz, unsigned int baudrate)
{
    mmio_write_32(base + UCR1, 0);
    mmio_write_32(base + UCR2, 0);

    /* The software reset ends when SRST reads 1 again */
    if (mmio_poll_32(base + UCR2, UCR2_SRST, UCR2_SRST, MMIO_POLL_HANDSHAKE) != PB_OK)
        return -PB_ERR_TIMEOUT;

    stuck = false;

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
