#include <board.h>
#include <plat.h>

#include <regs.h>
#include <io.h>
#include <uart.h>
#include <usb.h>
#include <emmc.h>

#include <plat/imx6ul/gpt.h>

static struct gp_timer platform_timer;


__inline u32 plat_get_ms_tick(void) {
    return gp_timer_get_tick(&platform_timer);
}

/*
 * --- Root clocks and their maximum rates ---
 *
 * ARM_CLK_ROOT                        528 MHz
 * MMDC_CLK_ROOT / FABRIC_CLK_ROOT     396 MHz
 * AXI_CLK_ROOT                        264 MHz
 * AHB_CLK_ROOT                        132 MHz
 * PERCLK_CLK_ROOT                     66  MHz
 * IPG_CLK_ROOT                        66  MHz
 * USDHCn_CLK_ROOT                     198 MHz
 *
 *
 */

void board_critical_init(void)
{
    u32 reg;

    platform_timer.base = GP_TIMER1_BASE;
    gp_timer_init(&platform_timer);

    /*** Configure ARM Clock ***/
    reg = pb_readl(REG(0x020C400C,0));
    /* Select step clock, so we can change arm PLL */
    pb_writel(reg | (1<<2), REG(0x020C400C,0));


    /* Power down */
    pb_writel((1<<12) , REG(0x020C8000,0));

    /* Configure divider and enable */
    /* f_CPU = 24 MHz * 88 / 4 = 528 MHz */
    pb_writel((1<<13) | 88, REG(0x020C8000,0));


    /* Wait for PLL to lock */
    while (!(pb_readl(REG(0x020C8000,0)) & (1<<31)))
        asm("nop");

    /* Select re-connect ARM PLL */
    pb_writel(reg & ~(1<<2), REG(0x020C400C, 0));
    
    /*** End of ARM Clock config ***/




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

    /* Configure pinmux for usdhc1 */
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
