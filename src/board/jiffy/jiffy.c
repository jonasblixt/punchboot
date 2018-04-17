#include <board.h>
#include <regs.h>
#include <io.h>
#include <uart.h>
#include <usb.h>
#include <emmc.h>


void board_critical_init(void)
{

    /* Ungate all clocks */
    pb_writel(0xFFFFFFFF, REG(0x020C4000,0x68)); /* Ungate usdhc clk*/
    pb_writel(0xFFFFFFFF, REG(0x020C4000,0x6C)); /* Ungate usdhc clk*/
    pb_writel(0xFFFFFFFF, REG(0x020C4000,0x70)); /* Ungate usdhc clk*/
    pb_writel(0xFFFFFFFF, REG(0x020C4000,0x74)); /* Ungate usdhc clk*/
    pb_writel(0xFFFFFFFF, REG(0x020C4000,0x78)); /* Ungate usdhc clk*/
    pb_writel(0xFFFFFFFF, REG(0x020C4000,0x7C)); /* Ungate usdhc clk*/
    pb_writel(0xFFFFFFFF, REG(0x020C4000,0x80)); /* Ungate usdhc clk*/


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

void board_emmc_init(void) {

    pb_writel(0, REG(0x020E0000, 0x1C0)); /* CLK MUX */
    pb_writel(0, REG(0x020E0000, 0x1BC)); /* CMD MUX */
    pb_writel(0, REG(0x020E0000, 0x1C4)); /* DATA0 MUX */
    pb_writel(0, REG(0x020E0000, 0x1C8)); /* DATA1 MUX */
    pb_writel(0, REG(0x020E0000, 0x1CC)); /* DATA2 MUX */
    pb_writel(0, REG(0x020E0000, 0x1D0)); /* DATA3 MUX */
 
    pb_writel(1, REG(0x020E0000, 0x1A8)); /* DATA4 MUX */
    pb_writel(1, REG(0x020E0000, 0x1AC)); /* DATA5 MUX */
    pb_writel(1, REG(0x020E0000, 0x1B0)); /* DATA6 MUX */
    pb_writel(1, REG(0x020E0000, 0x1B4)); /* DATA7 MUX */
    pb_writel(1, REG(0x020E0000, 0x1A4)); /* RESET MUX */



    soc_emmc_init();
}
