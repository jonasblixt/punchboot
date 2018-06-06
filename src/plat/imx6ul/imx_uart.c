/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <plat.h>
#include <io.h>
#include "imx_uart.h"
#include "imx_regs.h"

volatile uint32_t _uart_base;

void imx_uart_putc(char c) {
    volatile uint32_t usr2;
    
    for (;;) {
        usr2 = pb_readl(_uart_base+USR2);

        if (usr2 & (1<<3))
            break;
    }
    pb_writel(c, _uart_base+UTXD);

}

void plat_uart_putc(void *ptr, char c) {
    (void) (ptr); /* Supress warning of unused parameter */
    imx_uart_putc(c);
}

void imx_uart_init(__iomem uart_base) {
    volatile uint32_t reg;
    _uart_base = uart_base;

    
    pb_writel(0, _uart_base+UCR1);
    pb_writel(0, _uart_base+UCR2);

    for (;;) {
        reg = pb_readl(_uart_base+UCR2);
        
        if (reg & UCR2_SRST)
            break;
    }

    pb_writel(0x0704, _uart_base+ UCR3);
    pb_writel(0x8000, _uart_base+ UCR4);
    pb_writel(0x002b, _uart_base+ UESC);
    pb_writel(0x0000, _uart_base+ UTIM);
    pb_writel(0x0000, _uart_base+ UTS);
    pb_writel((4 << 7), _uart_base+ UFCR);
    pb_writel(0x000f, _uart_base+ UBIR);
    pb_writel(80000000 / (2 * 115200),_uart_base+ UBMR);
    pb_writel((UCR2_WS | UCR2_IRTS | UCR2_RXEN | 
                UCR2_TXEN | UCR2_SRST), _uart_base+ UCR2);

    pb_writel(UCR1_UARTEN, _uart_base+ UCR1);

}
