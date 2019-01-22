#include <board.h>
#include <plat.h>
#include <io.h>
#include <tinyprintf.h>
#include <plat/imx/gpt.h>
#include <plat/imx/caam.h>
#include <plat/imx/ocotp.h>
#include <plat/imx/imx_uart.h>
#include <plat/imx/usdhc.h>
#include <plat/imx/wdog.h>
#include <plat/imx/hab.h>
#include <board/config.h>
#include <fuse.h>

static struct ocotp_dev ocotp;
static struct gp_timer platform_timer;
static struct fsl_caam caam;
static struct usdhc_device usdhc0;

#define IMX6UL_FUSE_SHADOW_BASE 0x021BC000

static struct imx_wdog_device wdog_device;

/* Platform API Calls */
void      plat_reset(void)
{
    imx_wdog_reset_now();
}

uint32_t  plat_get_us_tick(void)
{
    return gp_timer_get_tick(&platform_timer);
}

void      plat_wdog_init(void)
{
    wdog_device.base = 0x020BC000;
    imx_wdog_init(&wdog_device, 1);
}

void      plat_wdog_kick(void)
{
    imx_wdog_kick();
}

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


/* UART Interface */

void plat_uart_putc(void *ptr, char c) 
{
    UNUSED(ptr);
    imx_uart_putc(c);
}


/* EMMC Interface */

uint32_t plat_write_block(uint32_t lba_offset, 
                          uint8_t *bfr, 
                          uint32_t no_of_blocks) 
{
    return usdhc_emmc_xfer_blocks(&usdhc0, 
                                  lba_offset, 
                                  bfr, 
                                  no_of_blocks, 
                                  1, 0);
}

uint32_t plat_read_block(uint32_t lba_offset, 
                         uint8_t *bfr, 
                         uint32_t no_of_blocks) 
{
    return usdhc_emmc_xfer_blocks(&usdhc0,
                                  lba_offset, 
                                  bfr, 
                                  no_of_blocks, 
                                  0, 0);
}

uint32_t plat_switch_part(uint8_t part_no) 
{
    return usdhc_emmc_switch_part(&usdhc0, part_no);
}

uint64_t plat_get_lastlba(void) 
{
    return usdhc0.sectors-1;
}

/* Crypto Interface */
uint32_t  plat_sha256_init(void)
{
    return caam_sha256_init();
}

uint32_t  plat_sha256_update(uint8_t *bfr, uint32_t sz)
{
    return caam_sha256_update(bfr,sz);
}

uint32_t  plat_sha256_finalize(uint8_t *out)
{
    return caam_sha256_finalize(out);
}

uint32_t  plat_rsa_enc(uint8_t *sig, uint32_t sig_sz, uint8_t *out, 
                        struct asn1_key *k)
{
    return caam_rsa_enc(sig, sig_sz, out, k);
}



uint32_t plat_early_init(void)
{
    uint32_t reg;
    uint32_t err;

    platform_timer.base = 0x02098000;
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

    ocotp.base = 0x021BC000;
    ocotp_init(&ocotp);


    usdhc0.base = 0x30B40000;
    usdhc0.clk_ident = 0x20EF;
    usdhc0.clk = 0x000F;

    err = usdhc_emmc_init(&usdhc0);

    if (err != PB_OK)
    {
        LOG_ERR("Could not initialize eMMC");
        return err;
    }

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

