#include <board.h>
#include <regs.h>
#include <io.h>
#include <uart.h>
#include <usb.h>

void board_critical_init(void)
{
}

void board_uart_init(void)
{
    pb_writel(0, REG(0x020E0094,0));
    pb_writel(0, REG(0x020E0098,0));
    pb_writel(UART_PAD_CTRL, REG(0x020E0320,0));
    pb_writel(UART_PAD_CTRL, REG(0x020E0324,0));

    soc_uart_init(UART_PHYS);

}

void board_usb_init(void) {
    
    soc_usb_init(USBC_PHYS);

}
