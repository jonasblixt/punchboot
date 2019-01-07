#include <board.h>
#include <plat.h>
#include <io.h>
#include <tinyprintf.h>
#include <plat/imx6ul/gpt.h>
#include <plat/imx6ul/caam.h>
#include <plat/imx6ul/ocotp.h>
#include <plat/imx6ul/imx_uart.h>
#include <plat/imx6ul/usdhc.h>
#include <plat/imx6ul/hab.h>
#include <board/config.h>
#include <fuse.h>

static struct ocotp_dev ocotp;
static struct gp_timer platform_timer;
static struct fsl_caam caam;

#define IMX6UL_FUSE_SHADOW_BASE 0x021BC000

uint32_t  plat_fuse_read(struct fuse *f)
{
    if (!(f->status & FUSE_VALID))
        return PB_ERR;

    if (!f->addr)
    {
        f->addr = f->bank*0x80 + f->word*0x10 + 0x400;

        if (f->bank >= 6)
            f->addr += 0x100;
    }

    if (!f->shadow)
        f->shadow = IMX6UL_FUSE_SHADOW_BASE + f->addr;

    f->value = pb_read32(f->shadow);

    return PB_OK;
}

uint32_t  plat_fuse_write(struct fuse *f)
{
    char s[64];    

    plat_fuse_to_string(f, s, 64);

    if ((f->status & FUSE_VALID) != FUSE_VALID)
    {
        LOG_ERR("Could not write fuse %s\n", s);
        return PB_ERR;
    }

    LOG_INFO("Writing: %s\n\r", s);

    return ocotp_write(f->bank, f->word, f->value);
}

uint32_t  plat_fuse_to_string(struct fuse *f, char *s, uint32_t n)
{
    return tfp_snprintf(s, n,
            "   FUSE<%lu,%lu> 0x%4.4lX %s = 0x%8.8lX\n",
                f->bank, f->word, f->addr,
                f->description, f->value);
}

uint32_t plat_get_us_tick(void) 
{
    return gp_timer_get_tick(&platform_timer);
}

uint32_t plat_early_init(void)
{
    uint32_t reg;

    platform_timer.base = GP_TIMER1_BASE;
    gp_timer_init(&platform_timer);


    /**
     * TODO: Some imx6ul can run at 696 MHz and some others at 528 MHz
     *   implement handeling of that.
     *
     */

    /*** Configure ARM Clock ***/
    reg = pb_read32(0x020C400C);
    /* Select step clock, so we can change arm PLL */
    pb_write32(reg | (1<<2), 0x020C400C);


    /* Power down */
    pb_write32((1<<12) , 0x020C8000);

    /* Configure divider and enable */
    /* f_CPU = 24 MHz * 88 / 4 = 528 MHz */
    pb_write32((1<<13) | 88, 0x020C8000);


    /* Wait for PLL to lock */
    while (!(pb_read32(0x020C8000) & (1<<31)))
        asm("nop");

    /* Select re-connect ARM PLL */
    pb_write32(reg & ~(1<<2), 0x020C400C);
    
    /*** End of ARM Clock config ***/



    /* Ungate all clocks */
    pb_write32(0xFFFFFFFF, 0x020C4000+0x68); /* Ungate usdhc clk*/
    pb_write32(0xFFFFFFFF, 0x020C4000+0x6C); /* Ungate usdhc clk*/
    pb_write32(0xFFFFFFFF, 0x020C4000+0x70); /* Ungate usdhc clk*/
    pb_write32(0xFFFFFFFF, 0x020C4000+0x74); /* Ungate usdhc clk*/
    pb_write32(0xFFFFFFFF, 0x020C4000+0x78); /* Ungate usdhc clk*/
    pb_write32(0xFFFFFFFF, 0x020C4000+0x7C); /* Ungate usdhc clk*/
    pb_write32(0xFFFFFFFF, 0x020C4000+0x80); /* Ungate usdhc clk*/


    uint32_t csu = 0x21c0000;
    /* Allow everything */
    for (int i = 0; i < 40; i ++) {
		*((uint32_t *)csu + i) = 0xffffffff;
	}
    
    board_init();

    if (board_get_debug_uart() != 0)
    {
        imx_uart_init(board_get_debug_uart());
        init_printf(NULL, &plat_uart_putc);
    }

    usdhc_emmc_init();

    ocotp.base = 0x021BC000;
    ocotp_init(&ocotp);

    /* Configure CAAM */
    caam.base = 0x02140000;

    if (caam_init(&caam) != PB_OK)
        return PB_ERR;

    if (hab_secureboot_active())
    {
        LOG_INFO("Secure boot active");
    } else {
        LOG_INFO("Secure boot disabled");
    }

    if (hab_has_no_errors() == PB_OK)
    {
        LOG_INFO("No HAB errors found");
    } else {
        LOG_ERR("HAB is reporting errors");
    }

    return PB_OK;
}

