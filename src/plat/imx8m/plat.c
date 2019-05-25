#include <stdio.h>
#include <pb.h>
#include <io.h>
#include <uuid.h>
#include <plat.h>
#include <board.h>
#include <fuse.h>
#include <timing_report.h>
#include <plat/regs.h>
#include <plat/imx/imx_uart.h>
#include <plat/imx8m/clock.h>
#include <plat/imx8m/plat.h>
#include <plat/imx/usdhc.h>
#include <plat/imx/gpt.h>
#include <plat/imx/caam.h>
#include <plat/imx/dwc3.h>
#include <plat/imx/hab.h>
#include <plat/imx/ocotp.h>
#include <plat/imx/wdog.h>

static struct pb_platform_setup plat;
extern struct fuse fuses[];

static __no_bss struct fsl_caam_jr caam;
static struct fuse fuse_uid0 = 
        IMX8M_FUSE_BANK_WORD(0, 1, "UID0");

static struct fuse fuse_uid1 = 
        IMX8M_FUSE_BANK_WORD(0, 2, "UID1");

/* Platform API Calls */

uint32_t plat_get_security_state(uint32_t *state)
{
    uint32_t err;
    (*state) = PB_SECURITY_STATE_NOT_SECURE;

    /* Read fuses */
    foreach_fuse(f, (struct fuse *) fuses)
    {
        err = plat_fuse_read(f);
 
        if (err != PB_OK)
        {
            LOG_ERR("Could not access fuse '%s'",f->description);
            return err;
        }  

        if (f->value)
        {
            (*state) = PB_SECURITY_STATE_CONFIGURED_ERR;
            break;
        }

    }


    if ((*state) == PB_SECURITY_STATE_NOT_SECURE)
        return PB_OK;

    if (hab_has_no_errors() == PB_OK)
        (*state) = PB_SECURITY_STATE_CONFIGURED_OK;
    else
        (*state) = PB_SECURITY_STATE_CONFIGURED_ERR;

    if (hab_secureboot_active())
        (*state) = PB_SECURITY_STATE_SECURE;

    return PB_OK;
}

static const char platform_namespace_uuid[] = 
    "\x32\x92\xd7\xd2\x28\x25\x41\x00\x90\xc3\x96\x8f\x29\x60\xc9\xf2";
uint32_t plat_get_uuid(char *out)
{
    plat_fuse_read(&fuse_uid0);
    plat_fuse_read(&fuse_uid1);

    uint32_t uid[2];
    uid[0] = fuse_uid0.value;
    uid[1] = fuse_uid1.value;

    LOG_INFO("%08x %08x",fuse_uid0.value, fuse_uid1.value);

    return uuid_gen_uuid3(platform_namespace_uuid,16,
                          (const char *) uid, 8, out);
}

uint32_t plat_get_params(struct param **pp)
{
    char uuid_raw[16];

    param_add_str((*pp)++, "Platform", "NXP IMX8M");
    plat_get_uuid(uuid_raw);
    param_add_uuid((*pp)++, "Device UUID",uuid_raw);
    return PB_OK;
}

uint32_t plat_setup_device(struct param *params)
{
    uint32_t err;

    /* Read fuses */
    foreach_fuse(f, (struct fuse *) fuses)
    {
        err = plat_fuse_read(f);
 
        LOG_DBG("Fuse %s: 0x%08x",f->description,f->value);
        if (err != PB_OK)
        {
            LOG_ERR("Could not access fuse '%s'",f->description);
            return err;
        }  
    }

    /* Perform the actual fuse programming */
    
    LOG_INFO("Writing fuses");

    foreach_fuse(f, fuses)
    {
        f->value = f->default_value;
        err = plat_fuse_write(f);

        if (err != PB_OK)
            return err;
    }

    return board_setup_device(params);
}

uint32_t plat_setup_lock(void)
{
    return PB_ERR;
}

uint32_t plat_prepare_recovery(void)
{
    return board_prepare_recovery(&plat);
}

bool plat_force_recovery(void)
{
    return board_force_recovery(&plat);
}

void plat_reset(void)
{
    imx_wdog_reset_now();
}

uint32_t plat_get_us_tick(void)
{
    return gp_timer_get_tick(&plat.tmr0);
}

void plat_preboot_cleanup(void)
{
}

void plat_wdog_init(void)
{
    /* Configure PAD_GPIO1_IO02 as wdog output */
    pb_write32((1 << 7)|(1 << 6) | 6, 0x30330298);
    pb_write32(1, 0x30330030);

    plat.wdog.base = 0x30280000;
    imx_wdog_init(&plat.wdog, 5);
}

void plat_wdog_kick(void)
{
    imx_wdog_kick();
}

extern void ddr_init(void);

uint32_t imx8m_clock_cfg(uint32_t clk_id, uint32_t flags)
{
    if (clk_id > 133)
        return PB_ERR;

    pb_write32(flags, (0x30388004 + 0x80*clk_id));

    return PB_OK;
}

#if LOGLEVEL >= 3
uint32_t imx8m_clock_print(uint32_t clk_id)
{
    uint32_t reg;
    uint32_t addr = (0x30388000 + 0x80*clk_id);

    if (clk_id > 133)
        return PB_ERR;

    reg = pb_read32(addr);

    uint32_t mux = (reg >> 24) & 0x7;
    uint32_t enabled = ((reg & (1 << 28)) > 0);
    uint32_t pre_podf = (reg >> 16) & 7;
    uint32_t post_podf = reg & 0x1f;

    LOG_INFO("CLK %u, 0x%08x = 0x%08x", clk_id,addr, reg);
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
    LOG_INFO("CG %u 0x%08x = 0x%08x",cg_id,addr,reg);
    return PB_OK;
}
#endif

uint32_t plat_early_init(void)
{
    volatile uint32_t reg;
    uint32_t err;

    board_early_init(&plat);

    plat_wdog_init();

    /* PLL1 div10 */
    imx8m_clock_cfg(GPT1_CLK_ROOT | (5 << 24), CLK_ROOT_ON);
    gp_timer_init(&plat.tmr0);
    tr_stamp_begin(TR_POR);

    /* Enable and ungate WDOG clocks */
    pb_write32((1 << 28) ,0x30388004 + 0x80*114);
    pb_write32(3, 0x30384004 + 0x10*83);
    pb_write32(3, 0x30384004 + 0x10*84);
    pb_write32(3, 0x30384004 + 0x10*85);

    imx_uart_init(&plat.uart0);

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

	while ((pb_read32(ARM_PLL_CFG0) & FRAC_PLL_LOCK_MASK) != FRAC_PLL_LOCK_MASK)
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

    err = board_late_init(&plat);

    if (err != PB_OK)
    {
        LOG_ERR("Board late init failed");
        return err;
    }

    pb_write32((1<<2), 0x303A00F8);


    pb_write32(0x03030303, 0x30384004 + 0x10*48);
    pb_write32(0x03030303, 0x30384004 + 0x10*81);

    err = usdhc_emmc_init(&plat.usdhc0);

    if (err != PB_OK)
    {
        LOG_ERR("Could not initialize eMMC");
        return err;
    }

    caam.base = 0x30901000;
    err = caam_init(&caam);

    if (err != PB_OK)
    {
        LOG_ERR("Could not initialize CAAM");
        return err;
    }
    ocotp_init(&plat.ocotp);

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

    tr_stamp_end(TR_POR);
    return err;
}

/* EMMC Interface */

uint32_t plat_write_block_async(uint32_t lba_offset,
                          uintptr_t bfr,
                          uint32_t no_of_blocks)
{
    return usdhc_emmc_xfer_blocks(&plat.usdhc0,
                                  lba_offset,
                                  (uint8_t*)bfr,
                                  no_of_blocks,
                                  1, 1);
}

uint32_t plat_flush_block(void)
{
    return usdhc_emmc_wait_for_de(&plat.usdhc0);
}

uint32_t plat_write_block(uint32_t lba_offset, 
                          uintptr_t bfr, 
                          uint32_t no_of_blocks) 
{
    return usdhc_emmc_xfer_blocks(&plat.usdhc0, 
                                  lba_offset, 
                                  (uint8_t*)bfr, 
                                  no_of_blocks, 
                                  1, 0);
}

uint32_t plat_read_block(uint32_t lba_offset, 
                         uintptr_t bfr, 
                         uint32_t no_of_blocks) 
{
    return usdhc_emmc_xfer_blocks(&plat.usdhc0,
                                  lba_offset, 
                                  (uint8_t *)bfr, 
                                  no_of_blocks, 
                                  0, 0);
}

uint32_t plat_switch_part(uint8_t part_no) 
{
    return usdhc_emmc_switch_part(&plat.usdhc0, part_no);
}

uint64_t plat_get_lastlba(void) 
{
    return (plat.usdhc0.sectors-1);
}

/* USB Interface API */
uint32_t  plat_usb_init(struct usb_device *dev)
{
    uint32_t err;

    dev->platform_data = (void *) &plat.usb0;
    err = dwc3_init(&plat.usb0);

    if (err != PB_OK)
    {
        LOG_ERR("Could not initalize dwc3");
        return err;
    }

    return PB_OK;
}

void plat_usb_task(struct usb_device *dev)
{
    dwc3_task(dev);
}

uint32_t plat_usb_transfer (struct usb_device *dev, 
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
    if (!(f->status & FUSE_VALID))
        return PB_ERR;

    if (!f->addr)
    {
        f->addr = f->bank*0x40 + f->word*0x10 + 0x400;
    }

    if (!f->shadow)
        f->shadow = IMX8M_FUSE_SHADOW_BASE + f->addr;

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
    return snprintf(s, n,
            "   FUSE<%u,%u> 0x%x %s = 0x%x\n",
                f->bank, f->word, f->addr,
                f->description, f->value);
}


