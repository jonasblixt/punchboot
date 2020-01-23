#include <pb/pb.h>
#include <plat/imx8m/umctl2.h>


void main(void)
{
    struct imx_uart_device uart0;

    /* Enable UART1 clock */
    pb_write32((1 << 28), 0x30388004 + 94*0x80);
    /* Ungate UART1 clock */
    pb_write32(3, 0x30384004 + 0x10*73);

    /* Ungate GPIO blocks */

    pb_write32(3, 0x30384004 + 0x10*11);
    pb_write32(3, 0x30384004 + 0x10*12);
    pb_write32(3, 0x30384004 + 0x10*13);
    pb_write32(3, 0x30384004 + 0x10*14);
    pb_write32(3, 0x30384004 + 0x10*15);


    pb_write32(3, 0x30384004 + 0x10*27);
    pb_write32(3, 0x30384004 + 0x10*28);
    pb_write32(3, 0x30384004 + 0x10*29);
    pb_write32(3, 0x30384004 + 0x10*30);
    pb_write32(3, 0x30384004 + 0x10*31);


    /* UART1 pad mux */
    pb_write32(0, 0x30330234);
    pb_write32(0, 0x30330238);

    /* UART1 PAD settings */
    pb_write32(7, 0x3033049C);
    pb_write32(7, 0x303304A0);

    uart0.base = 0x30860000;
    imx_uart_init(&uart0);

    /* Configure main clocks */
    imx8m_clock_cfg(ARM_A53_CLK_ROOT, CLK_ROOT_ON);

    /* Configure PLL's */
    /* bypass the clock */
    pb_write32(pb_read32(ARM_PLL_CFG0) | FRAC_PLL_BYPASS_MASK, ARM_PLL_CFG0);
    /* Set CPU core clock to 1 GHz */
    pb_write32(FRAC_PLL_INT_DIV_CTL_VAL(49), ARM_PLL_CFG1);

    pb_write32((FRAC_PLL_CLKE_MASK | FRAC_PLL_REFCLK_SEL_OSC_25M |
               FRAC_PLL_LOCK_SEL_MASK | FRAC_PLL_NEWDIV_VAL_MASK |
               FRAC_PLL_REFCLK_DIV_VAL(4) |
               FRAC_PLL_OUTPUT_DIV_VAL(0) | FRAC_PLL_BYPASS_MASK),
               ARM_PLL_CFG0);

    /* unbypass the clock */
    pb_clrbit32(FRAC_PLL_BYPASS_MASK, ARM_PLL_CFG0);

    while (((pb_read32(ARM_PLL_CFG0) & FRAC_PLL_LOCK_MASK) !=
            (uint32_t)FRAC_PLL_LOCK_MASK))
        __asm__("nop");

    pb_clrbit32(FRAC_PLL_NEWDIV_VAL_MASK, ARM_PLL_CFG0);

    pb_setbit32(SSCG_PLL_CLKE_MASK | SSCG_PLL_DIV2_CLKE_MASK |
        SSCG_PLL_DIV3_CLKE_MASK | SSCG_PLL_DIV4_CLKE_MASK |
        SSCG_PLL_DIV5_CLKE_MASK | SSCG_PLL_DIV6_CLKE_MASK |
        SSCG_PLL_DIV8_CLKE_MASK | SSCG_PLL_DIV10_CLKE_MASK |
        SSCG_PLL_DIV20_CLKE_MASK, SYS_PLL1_CFG0);

    pb_setbit32(SSCG_PLL_CLKE_MASK | SSCG_PLL_DIV2_CLKE_MASK |
        SSCG_PLL_DIV3_CLKE_MASK | SSCG_PLL_DIV4_CLKE_MASK |
        SSCG_PLL_DIV5_CLKE_MASK | SSCG_PLL_DIV6_CLKE_MASK |
        SSCG_PLL_DIV8_CLKE_MASK | SSCG_PLL_DIV10_CLKE_MASK |
        SSCG_PLL_DIV20_CLKE_MASK, SYS_PLL2_CFG0);

    LOG_INFO("PB SPL Init");

    while(1);

}
