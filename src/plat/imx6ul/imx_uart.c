#include <plat.h>
#include <io.h>
#include "imx_uart.h"
#include "imx_regs.h"

volatile u32 _uart_base;

void imx_uart_putc(char c) {
    volatile u32 usr2;
    
    for (;;) {
        usr2 = pb_readl(REG(_uart_base, USR2));

        if (usr2 & (1<<3))
            break;
    }
    pb_writel(c, REG(_uart_base, UTXD));

}

void plat_uart_putc(u8 *ptr, u8 c) {
    imx_uart_putc(c);
}

void imx_uart_init(__iomem uart_base) {
    volatile u32 reg;
    _uart_base = UART_PHYS;

    
    pb_writel(0, REG(_uart_base, UCR1));
    pb_writel(0, REG(_uart_base, UCR2));

    for (;;) {
        reg = pb_readl(REG(_uart_base,UCR2));
        
        if (reg & UCR2_SRST)
            break;
    }

    pb_writel(0x0704, REG(_uart_base, UCR3));
    pb_writel(0x8000, REG(_uart_base, UCR4));
    pb_writel(0x002b, REG(_uart_base, UESC));
    pb_writel(0x0000, REG(_uart_base, UTIM));
    pb_writel(0x0000, REG(_uart_base, UTS));
    pb_writel((4 << 7), REG(_uart_base, UFCR));
    pb_writel(0x000f, REG(_uart_base, UBIR));
    pb_writel(80000000 / (2 * 115200),REG(_uart_base, UBMR));
    pb_writel((UCR2_WS | UCR2_IRTS | UCR2_RXEN | 
                UCR2_TXEN | UCR2_SRST), REG(_uart_base, UCR2));

    pb_writel(UCR1_UARTEN, REG(_uart_base, UCR1));

}
