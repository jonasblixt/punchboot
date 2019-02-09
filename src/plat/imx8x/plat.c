#include <stdio.h>
#include <pb.h>
#include <io.h>
#include <plat.h>
#include <board.h>
#include <plat/regs.h>
#include <plat/imx/lpuart.h>
#include <plat/imx/usdhc.h>
#include <plat/imx/gpt.h>
#include <plat/imx/ehci.h>
#include <plat/imx/caam.h>
#include <plat/sci/ipc.h>
#include <plat/sci/sci.h>
#include <plat/imx8qxp_pads.h>
#include <plat/iomux.h>

#define ESDHC_PAD_CTRL	(PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | \
                         (SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | \
                         (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
						 (SC_PAD_28FDSOI_DSE_18V_HS << PADRING_DSE_SHIFT) | \
                         (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define ESDHC_CLK_PAD_CTRL	(PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | \
                             (SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | \
                             (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
						     (SC_PAD_28FDSOI_DSE_18V_HS << PADRING_DSE_SHIFT) | \
                             (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define UART_PAD_CTRL	(PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | \
			(SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | \
			(SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
			(SC_PAD_28FDSOI_DSE_DV_LOW << PADRING_DSE_SHIFT) | \
			(SC_PAD_28FDSOI_PS_PD << PADRING_PULL_SHIFT))

static struct usdhc_device usdhc0;
static struct gp_timer tmr0;
static struct lpuart_device uart_device;
static __no_bss struct fsl_caam_jr caam;

sc_ipc_t ipc_handle;

/* Platform API Calls */
void plat_reset(void)
{
    sc_pm_reset(ipc_handle,SC_PM_RESET_TYPE_BOARD);
}

uint32_t  plat_get_us_tick(void)
{
    return gp_timer_get_tick(&tmr0);
}

void plat_wdog_init(void)
{


}

void plat_wdog_kick(void)
{
}

uint32_t  plat_early_init(void)
{
    volatile uint32_t reg;
    uint32_t err = PB_OK;

    
	sc_ipc_open(&ipc_handle, SC_IPC_BASE);

	/* Power up UART0 */
	sc_pm_set_resource_power_mode(ipc_handle, SC_R_UART_0, SC_PM_PW_MODE_ON);

	/* Set UART0 clock root to 80 MHz */
	sc_pm_clock_rate_t rate = 80000000;
	sc_pm_set_clock_rate(ipc_handle, SC_R_UART_0, 2, &rate);

	/* Enable UART0 clock root */
	sc_pm_clock_enable(ipc_handle, SC_R_UART_0, 2, true, false);

	/* Configure UART pads */
	sc_pad_set(ipc_handle, SC_P_UART0_RX, UART_PAD_CTRL);
	sc_pad_set(ipc_handle, SC_P_UART0_TX, UART_PAD_CTRL);

    //board_early_init();

    uart_device.base = 0x5A060000;//board_get_debug_uart();
    uart_device.baudrate = 0x402008b;

    lpuart_init(&uart_device);
    
    LOG_INFO("IMX8X init");
    /* Setup GPT0 */
	sc_pm_set_resource_power_mode(ipc_handle, SC_R_GPT_0, SC_PM_PW_MODE_ON);
	rate = 24000000;
	sc_pm_set_clock_rate(ipc_handle, SC_R_GPT_0, 2, &rate);

	err = sc_pm_clock_enable(ipc_handle, SC_R_GPT_0, 2, true, false);

    if (err != SC_ERR_NONE)
    {
        LOG_ERR("Could not enable GPT0 clock");
        return PB_ERR;
    }

    tmr0.base = 0x5D140000;
    tmr0.pr = 24;

    gp_timer_init(&tmr0);

    /* Setup USDHC0 */
	sc_pm_set_resource_power_mode(ipc_handle, SC_R_SDHC_0, SC_PM_PW_MODE_ON);
	rate = 200000000;
	sc_pm_set_clock_rate(ipc_handle, SC_R_SDHC_0, 2, &rate);

	err = sc_pm_clock_enable(ipc_handle, SC_R_SDHC_0, SC_PM_CLK_PER, 
                                true, false);

	if (err != SC_ERR_NONE) 
    {
		LOG_ERR("SDHC_0 per clk enable failed!");
		return err;
	}

	sc_pad_set(ipc_handle, SC_P_EMMC0_CLK, ESDHC_CLK_PAD_CTRL);
	sc_pad_set(ipc_handle, SC_P_EMMC0_CMD, ESDHC_PAD_CTRL);
	sc_pad_set(ipc_handle, SC_P_EMMC0_DATA0, ESDHC_PAD_CTRL);
	sc_pad_set(ipc_handle, SC_P_EMMC0_DATA1, ESDHC_PAD_CTRL);
	sc_pad_set(ipc_handle, SC_P_EMMC0_DATA2, ESDHC_PAD_CTRL);
	sc_pad_set(ipc_handle, SC_P_EMMC0_DATA3, ESDHC_PAD_CTRL);
	sc_pad_set(ipc_handle, SC_P_EMMC0_DATA4, ESDHC_PAD_CTRL);
	sc_pad_set(ipc_handle, SC_P_EMMC0_DATA5, ESDHC_PAD_CTRL);
	sc_pad_set(ipc_handle, SC_P_EMMC0_DATA6, ESDHC_PAD_CTRL);
	sc_pad_set(ipc_handle, SC_P_EMMC0_DATA7, ESDHC_PAD_CTRL);
	sc_pad_set(ipc_handle, SC_P_EMMC0_STROBE, ESDHC_PAD_CTRL);
	sc_pad_set(ipc_handle, SC_P_EMMC0_RESET_B, ESDHC_PAD_CTRL);

    usdhc0.base = 0x5B010000;
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


	sc_pm_set_resource_power_mode(ipc_handle, 
                                SC_R_CAAM_JR2, SC_PM_PW_MODE_ON);
	sc_pm_set_resource_power_mode(ipc_handle, 
                                SC_R_CAAM_JR2_OUT, SC_PM_PW_MODE_ON);
	sc_pm_set_resource_power_mode(ipc_handle, 
                                SC_R_CAAM_JR3, SC_PM_PW_MODE_ON);
	sc_pm_set_resource_power_mode(ipc_handle, 
                                SC_R_CAAM_JR3_OUT, SC_PM_PW_MODE_ON);


    caam.base = 0x31430000;
    err = caam_init(&caam);

    if (err != PB_OK)
    {
        LOG_ERR("Could not initialize CAAM");
        return err;
    }
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

void plat_preboot_cleanup(void)
{
    usdhc_emmc_reset(&usdhc0);

	sc_pm_clock_enable(ipc_handle, SC_R_SDHC_0, SC_PM_CLK_PER, false, false);
	sc_pm_set_resource_power_mode(ipc_handle, SC_R_SDHC_0, SC_PM_PW_MODE_OFF);

	sc_pm_clock_enable(ipc_handle, SC_R_GPT_0, SC_PM_CLK_PER, false, false);
}

/* USB Interface API */
uint32_t  plat_usb_init(struct usb_device *dev)
{
    uint32_t err = PB_OK;
    uint32_t reg;


	sc_pm_clock_rate_t rate = 166000000;

	sc_pm_set_resource_power_mode(ipc_handle, SC_R_USB_0, SC_PM_PW_MODE_ON);
	sc_pm_set_resource_power_mode(ipc_handle, SC_R_USB_0_PHY, SC_PM_PW_MODE_ON);



    pb_clrbit32((1 << 31) | (1 << 30), 0x5B100030);

    /* Enable USB PLL */
    pb_write32(0x00E03040, 0x5B100000+0xa0);

    /* Power up USB */
    pb_write32(0x00, 0x5B100000);
    LOG_DBG("usb pll: 0x%x",pb_read32(0x5B100000+0xa0));
    return ehci_usb_init(dev);
}

void plat_usb_task(struct usb_device *dev)
{
    ehci_usb_task(dev);
}

uint32_t plat_usb_transfer (struct usb_device *dev, uint8_t ep, 
                            uint8_t *bfr, uint32_t sz) 
{
    struct ehci_device *ehci = (struct ehci_device *) dev->platform_data;
    return ehci_transfer(ehci, ep, bfr, sz);
}

void plat_usb_set_address(struct usb_device *dev, uint32_t addr)
{
    struct ehci_device *ehci = (struct ehci_device *) dev->platform_data;
    pb_write32((addr << 25) | (1 <<24), ehci->base+EHCI_DEVICEADDR);
}

void plat_usb_set_configuration(struct usb_device *dev)
{
    ehci_usb_set_configuration(dev);
}

void plat_usb_wait_for_ep_completion(struct usb_device *dev, uint32_t ep)
{
    ehci_usb_wait_for_ep_completion(dev, ep);
}

/* UART Interface */

void plat_uart_putc(void *ptr, char c) 
{
    UNUSED(ptr);
    lpuart_putc(&uart_device, c);
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

