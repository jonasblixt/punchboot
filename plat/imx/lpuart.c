
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
#include <pb/plat.h>
#include <plat/imx/lpuart.h>
#include <plat/defs.h>

int imx_lpuart_write(char *buf, size_t size)
{
    for (unsigned int i = 0; i < size; i++)
    {
        while (!(pb_read32(IMX_LPUART_BASE + STAT) & (1 << 22)))
            __asm__("nop");

        pb_write32(buf[i], IMX_LPUART_BASE + DATA);
    }

    return PB_OK;
}

int imx_lpuart_init(void)
{
    uint32_t tmp;

    tmp = pb_read32(IMX_LPUART_BASE + CTRL);
    tmp &= ~(CTRL_TE | CTRL_RE);
    pb_write32(tmp, IMX_LPUART_BASE + CTRL);

    pb_write32(0, IMX_LPUART_BASE + MODIR);
    pb_write32(~(FIFO_TXFE | FIFO_RXFE), IMX_LPUART_BASE + FIFO);

    pb_write32(0, IMX_LPUART_BASE + MATCH);

    pb_write32(IMX_LPUART_BAUDRATE, IMX_LPUART_BASE + BAUD);


    tmp = pb_read32(IMX_LPUART_BASE + CTRL);
    tmp &= ~(LPUART_CTRL_PE_MASK | LPUART_CTRL_PT_MASK | LPUART_CTRL_M_MASK);
    pb_write32(tmp, IMX_LPUART_BASE + CTRL);

    pb_write32(CTRL_RE | CTRL_TE, IMX_LPUART_BASE + CTRL);

    return PB_OK;
}


