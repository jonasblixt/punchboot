#include <pb.h>
#include <io.h>
#include <plat.h>
#include <tinyprintf.h>
#include <board.h>
#include <plat/regs.h>
#include <plat/imx/imx_uart.h>
#include <plat/imx8m/clock.h>
#include <plat/imx/usdhc.h>
#include <plat/imx/gpt.h>
#include <plat/imx/caam.h>
#include <plat/imx/xhci.h>


static struct usdhc_device usdhc0;
static struct gp_timer tmr0;
static struct xhci_device *xhci0;
static __no_bss struct fsl_caam caam;

/* Platform API Calls */
void      plat_reset(void)
{
}

uint32_t  plat_get_us_tick(void)
{
    return gp_timer_get_tick(&tmr0);
}

void      plat_wdog_init(void)
{
}

void      plat_wdog_kick(void)
{
}

extern void ddr_init(void);

uint32_t imx8m_clock_cfg(uint32_t clk_id, uint32_t flags)
{
    if (clk_id > 133)
        return PB_ERR;

    pb_write32(flags, 0x30388004 + 0x80*clk_id);

    return PB_OK;
}

uint32_t  plat_early_init(void)
{
    volatile uint32_t reg;
    uint32_t err;

    tmr0.base = 0x302D0000;
    tmr0.pr = 24;

    gp_timer_init(&tmr0);

    /* Enable and ungate WDOG clocks */
    pb_write32((1 << 28) ,0x30388004 + 0x80*114);
    pb_write32(3, 0x30384004 + 0x10*83);
    pb_write32(3, 0x30384004 + 0x10*84);
    pb_write32(3, 0x30384004 + 0x10*85);

    board_early_init();

    imx_uart_init(board_get_debug_uart());

    init_printf(NULL, &plat_uart_putc);


    /* Configure main clocks */
    imx8m_clock_cfg(ARM_A53_CLK_ROOT, CLK_ROOT_ON);

    /* Configure PLL's */

	/* bypass the clock */
	pb_write32(pb_read32(ARM_PLL_CFG0) | FRAC_PLL_BYPASS_MASK, ARM_PLL_CFG0);
	/* Set CPU core clock to 1 GHz */
	pb_write32(FRAC_PLL_INT_DIV_CTL_VAL(49), ARM_PLL_CFG1);

	pb_write32(FRAC_PLL_CLKE_MASK | FRAC_PLL_REFCLK_SEL_OSC_25M |
			   FRAC_PLL_LOCK_SEL_MASK | FRAC_PLL_NEWDIV_VAL_MASK |
			   FRAC_PLL_REFCLK_DIV_VAL(4) |
			   FRAC_PLL_OUTPUT_DIV_VAL(0) | FRAC_PLL_BYPASS_MASK, 
               ARM_PLL_CFG0);

	reg = pb_read32(ARM_PLL_CFG0);

	/* unbypass the clock */
	pb_write32(reg & ~FRAC_PLL_BYPASS_MASK, ARM_PLL_CFG0);

	while (!(pb_read32(ARM_PLL_CFG0) & FRAC_PLL_LOCK_MASK))
		asm("nop");

    reg = pb_read32(ARM_PLL_CFG0);
    reg &= ~FRAC_PLL_NEWDIV_VAL_MASK;
    pb_write32(reg, ARM_PLL_CFG0);

	reg = pb_read32(SYS_PLL1_CFG0);
	reg |= SSCG_PLL_CLKE_MASK | SSCG_PLL_DIV2_CLKE_MASK |
		SSCG_PLL_DIV3_CLKE_MASK | SSCG_PLL_DIV4_CLKE_MASK |
		SSCG_PLL_DIV5_CLKE_MASK | SSCG_PLL_DIV6_CLKE_MASK |
		SSCG_PLL_DIV8_CLKE_MASK | SSCG_PLL_DIV10_CLKE_MASK |
		SSCG_PLL_DIV20_CLKE_MASK;
	pb_write32(reg, SYS_PLL1_CFG0);

	reg = pb_read32(SYS_PLL2_CFG0);
	reg |= SSCG_PLL_CLKE_MASK | SSCG_PLL_DIV2_CLKE_MASK |
		SSCG_PLL_DIV3_CLKE_MASK | SSCG_PLL_DIV4_CLKE_MASK |
		SSCG_PLL_DIV5_CLKE_MASK | SSCG_PLL_DIV6_CLKE_MASK |
		SSCG_PLL_DIV8_CLKE_MASK | SSCG_PLL_DIV10_CLKE_MASK |
		SSCG_PLL_DIV20_CLKE_MASK;
	pb_write32(reg, SYS_PLL2_CFG0);

    /* Configure USB clock */
    imx8m_clock_cfg(USB_BUS_CLK_ROOT | (1 << 24), CLK_ROOT_ON);
    imx8m_clock_cfg(USB_CORE_REF_CLK_ROOT | (1 << 24), CLK_ROOT_ON);
    imx8m_clock_cfg(USB_PHY_REF_CLK_ROOT | (1 << 24), CLK_ROOT_ON);

    pb_write32(3, 0x30384004 + 0x10*77);

    pb_write32(3, 0x30384004 + 0x10*78);
    pb_write32(3, 0x30384004 + 0x10*79);

    pb_write32(3, 0x30384004 + 0x10*80);
    LOG_INFO("Main PLL configured");

    LOG_INFO("ARM PLL %8.8X",pb_read32(0x30360028));
    LOG_INFO("SYS PLL1 %8.8X",pb_read32(0x30360030));
    LOG_INFO("SYS PLL2 %8.8X",pb_read32(0x3036003C));
    LOG_INFO("SYS PLL3 %8.8X",pb_read32(0x30360048));
    LOG_INFO("DRAM PLL %8.8X",pb_read32(0x30360060));

    ddr_init();

    LOG_INFO("LPDDR4 training complete");

    err = board_late_init();

    if (err != PB_OK)
    {
        LOG_ERR("Board late init failed");
        return err;
    }


    /* Enable USDHC1 clock root, source: PLL1/2 = 400 MHz */

 /*   imx8m_clock_cfg(USDHC1_CLK_ROOT, CLK_ROOT_ON | (1 << 24) );
    imx8m_clock_cfg(NAND_USDHC_BUS_CLK_ROOT, CLK_ROOT_ON | (1 << 24) | 1);
    imx8m_clock_cfg(IPG_CLK_ROOT, CLK_ROOT_ON | (1 << 24) | 1);
    imx8m_clock_cfg(AHB_CLK_ROOT, CLK_ROOT_ON | (1 << 24) | 1);
    imx8m_clock_cfg(MXC_IPG_CLK, CLK_ROOT_ON |(1 << 24));
    imx8m_clock_cfg(NOC_CLK_ROOT, CLK_ROOT_ON |(1 << 24));
*/
    pb_write32(0x03030303, 0x30384004 + 0x10*48);
    pb_write32(0x03030303, 0x30384004 + 0x10*81);

    usdhc0.base = 0x30B40000;
    usdhc0.clk_ident = 0x20EF;
    usdhc0.clk = 0x000F;

    err = usdhc_emmc_init(&usdhc0);

    LOG_INFO("tick %u", plat_get_us_tick());
    if (err != PB_OK)
    {
        LOG_ERR("Could not initialize eMMC");
        return err;
    }

    caam.base = 0x30900000;
    err = caam_init(&caam);

    if (err != PB_OK)
    {
        LOG_ERR("Could not initialize CAAM");
        return err;
    }


/*
    pb_write32(5, 0x303301C4);
    pb_setbit32(1<<23, 0x30230004);
    uint32_t t,t2;
    bool toggle;

    t2 = plat_get_us_tick();

    while (1)
    {
        t = plat_get_us_tick();

        if ((t-t2) > 1000)
        {
            t2 = plat_get_us_tick();
            toggle = !toggle;

            if (toggle)
                pb_setbit32(1<<23, 0x30230000);
            else
                pb_clrbit32(1<<23, 0x30230000);
        }
    }
*/

    return err;
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
    return PB_ERR;
}

uint32_t  plat_sha256_update(uint8_t *bfr, uint32_t sz)
{
    return PB_ERR;
}

uint32_t  plat_sha256_finalize(uint8_t *out)
{
    return PB_ERR;
}

uint32_t  plat_rsa_enc(uint8_t *sig, uint32_t sig_sz, uint8_t *out, 
                        struct asn1_key *k)
{
    return PB_ERR;
}

/* USB Interface API */
uint32_t  plat_usb_init(struct usb_device *dev)
{
    uint32_t err;

    xhci0 = (struct xhci_device *) dev->platform_data;

    err = xhci_init(xhci0);

    if (err != PB_OK)
    {
        LOG_ERR("Could not initalize xhci");
        return err;
    }

    return PB_OK;
}

void      plat_usb_task(struct usb_device *dev)
{
    struct xhci_device *d = (struct xhci_device*) dev->platform_data;
    xhci_task(d);
}

uint32_t  plat_usb_transfer (struct usb_device *dev, 
                             uint8_t ep, 
                             uint8_t *bfr, 
                             uint32_t sz)
{
    struct xhci_device *d = (struct xhci_device*) dev->platform_data;
    return xhci_transfer(d, ep, bfr, sz);
}

void      plat_usb_set_address(struct usb_device *dev, uint32_t addr)
{
}

void      plat_usb_set_configuration(struct usb_device *dev)
{
}

void      plat_usb_wait_for_ep_completion(uint32_t ep)
{
}

/* UART Interface */

void plat_uart_putc(void *ptr, char c) 
{
    UNUSED(ptr);
    imx_uart_putc(c);
}

/* FUSE Interface */
uint32_t  plat_fuse_read(struct fuse *f)
{
    return PB_ERR;
}

uint32_t  plat_fuse_write(struct fuse *f)
{
    return PB_ERR;
}

uint32_t  plat_fuse_to_string(struct fuse *f, char *s, uint32_t n)
{
    return PB_ERR;
}


