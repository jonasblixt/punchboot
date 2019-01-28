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
#include <plat/imx/dwc3.h>
#include <plat/imx/hab.h>
#include <plat/imx/ocotp.h>
#include <plat/imx/wdog.h>

static struct usdhc_device usdhc0;
static struct gp_timer tmr0;
static struct dwc3_device *dwc30;
static __no_bss struct fsl_caam caam;
static struct ocotp_dev ocotp;
static struct imx_wdog_device wdog_device;
static struct imx_uart_device uart_device;

/* Platform API Calls */
void      plat_reset(void)
{
    imx_wdog_reset_now();
}

uint32_t  plat_get_us_tick(void)
{
    return gp_timer_get_tick(&tmr0);
}

void      plat_wdog_init(void)
{
    /* Configure PAD_GPIO1_IO02 as wdog output */
    pb_write32((1 << 7)|(1 << 6) | 6, 0x30330298);
    pb_write32(1, 0x30330030);

    wdog_device.base = 0x30280000;
    imx_wdog_init(&wdog_device, 1);

}

void      plat_wdog_kick(void)
{
    imx_wdog_kick();
}

extern void ddr_init(void);

uint32_t imx8m_clock_cfg(uint32_t clk_id, uint32_t flags)
{
    if (clk_id > 133)
        return PB_ERR;

    pb_write32(flags, 0x30388004 + 0x80*clk_id);

    return PB_OK;
}

#if LOGLEVEL >= 3
uint32_t imx8m_clock_print(uint32_t clk_id)
{
    uint32_t reg;
    uint32_t addr = 0x30388000 + 0x80*clk_id;

    if (clk_id > 133)
        return PB_ERR;

    reg = pb_read32(addr);

    uint32_t mux = (reg >> 24) & 0x7;
    uint32_t enabled = ((reg & (1 << 28)) > 0);
    uint32_t pre_podf = (reg >> 16) & 7;
    uint32_t post_podf = reg & 0x1f;

    LOG_INFO("CLK %u, 0x%8.8X = 0x%8.8X", clk_id,addr, reg);
    LOG_INFO("  MUX %u", mux);
    LOG_INFO("  En: %u", enabled);
    LOG_INFO("  Prepodf: %u", pre_podf);
    LOG_INFO("  Postpodf: %u", post_podf);

    return PB_OK;
}

uint32_t imx8m_cg_print(uint32_t cg_id)
{
    uint32_t reg;
    uint32_t addr = 0x30384000 + 0x10*cg_id;
    reg = pb_read32(addr);
    LOG_INFO("CG %u 0x%8.8X = 0x%8.8X",cg_id,addr,reg);
    return PB_OK;
}
#endif

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

    uart_device.base = board_get_debug_uart();
    uart_device.baudrate = 0x6C;

    imx_uart_init(&uart_device);

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
		__asm__("nop");

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
    imx8m_clock_cfg(USB_CORE_REF_CLK_ROOT, CLK_ROOT_ON);
    imx8m_clock_cfg(USB_PHY_REF_CLK_ROOT | (1 << 24), CLK_ROOT_ON);

    pb_write32(3, 0x30384004 + 0x10*77);
    pb_write32(3, 0x30384004 + 0x10*78);
    pb_write32(3, 0x30384004 + 0x10*79);
    pb_write32(3, 0x30384004 + 0x10*80);

    LOG_INFO("Main PLL configured");

    LOG_DBG("ARM PLL %8.8X",pb_read32(0x30360028));
    LOG_DBG("SYS PLL1 %8.8X",pb_read32(0x30360030));
    LOG_DBG("SYS PLL2 %8.8X",pb_read32(0x3036003C));
    LOG_DBG("SYS PLL3 %8.8X",pb_read32(0x30360048));
    LOG_DBG("DRAM PLL %8.8X",pb_read32(0x30360060));

    ddr_init();

    LOG_DBG("LPDDR4 training complete");

    err = board_late_init();

    if (err != PB_OK)
    {
        LOG_ERR("Board late init failed");
        return err;
    }

    pb_write32((1<<2), 0x303A00F8);


    pb_write32(0x03030303, 0x30384004 + 0x10*48);
    pb_write32(0x03030303, 0x30384004 + 0x10*81);

    usdhc0.base = 0x30B40000;
    usdhc0.clk_ident = 0x20EF;
    usdhc0.clk = 0x000F;
    usdhc0.bus_mode = USDHC_BUS_HS200;
    usdhc0.bus_width = USDHC_BUS_8BIT;

    err = usdhc_emmc_init(&usdhc0);

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


    ocotp.base = 0x30350000;
    ocotp_init(&ocotp);

    if (hab_secureboot_active())
    {
        LOG_INFO("Secure boot active");
    } else {
        LOG_INFO("Secure boot disabled");
    }
/*
    if (hab_has_no_errors() == PB_OK)
    {
        LOG_INFO("No HAB errors found");
    } else {
        LOG_ERR("HAB is reporting errors");
    }
*/
    return err;
}

/* EMMC Interface */

uint32_t plat_write_block(uint32_t lba_offset, 
                          uintptr_t bfr, 
                          uint32_t no_of_blocks) 
{
    return usdhc_emmc_xfer_blocks(&usdhc0, 
                                  lba_offset, 
                                  (uint8_t*)bfr, 
                                  no_of_blocks, 
                                  1, 0);
}

uint32_t plat_read_block(uint32_t lba_offset, 
                         uintptr_t bfr, 
                         uint32_t no_of_blocks) 
{
    return usdhc_emmc_xfer_blocks(&usdhc0,
                                  lba_offset, 
                                  (uint8_t *)bfr, 
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

uint32_t  plat_sha256_update(uintptr_t bfr, uint32_t sz)
{
    return caam_sha256_update((uint8_t *)bfr,sz);
}

uint32_t  plat_sha256_finalize(uintptr_t out)
{
    return caam_sha256_finalize((uint8_t *) out);
}

uint32_t  plat_rsa_enc(uint8_t *sig, uint32_t sig_sz, uint8_t *out, 
                        struct asn1_key *k)
{
    return caam_rsa_enc(sig, sig_sz, out, k);
}

/* USB Interface API */
uint32_t  plat_usb_init(struct usb_device *dev)
{
    uint32_t err;

    dwc30 = (struct dwc3_device *) dev->platform_data;

    err = dwc3_init(dwc30);

    if (err != PB_OK)
    {
        LOG_ERR("Could not initalize dwc3");
        return err;
    }

    return PB_OK;
}

void      plat_usb_task(struct usb_device *dev)
{
    dwc3_task(dev);
}

uint32_t  plat_usb_transfer (struct usb_device *dev, 
                             uint8_t ep, 
                             uint8_t *bfr, 
                             uint32_t sz)
{
    struct dwc3_device *d = (struct dwc3_device*) dev->platform_data;
    return dwc3_transfer(d, ep, bfr, sz);
}

void plat_usb_set_address(struct usb_device *dev, uint32_t addr)
{
    struct dwc3_device *d = (struct dwc3_device*) dev->platform_data;
    dwc3_set_addr(d, addr);
}

void plat_usb_set_configuration(struct usb_device *dev)
{
    dwc3_set_configuration(dev);
}

void plat_usb_wait_for_ep_completion(struct usb_device *dev, uint32_t ep)
{
    struct dwc3_device *d = (struct dwc3_device*) dev->platform_data;
    dwc3_wait_for_ep_completion(d, ep);
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
    UNUSED(f);
    return PB_ERR;
}

uint32_t  plat_fuse_write(struct fuse *f)
{
    UNUSED(f);
    return PB_ERR;
}

uint32_t  plat_fuse_to_string(struct fuse *f, char *s, uint32_t n)
{
    UNUSED(f);
    UNUSED(s);
    UNUSED(n);
    return PB_ERR;
}


