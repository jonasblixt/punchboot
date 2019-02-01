#include <pb.h>
#include <io.h>
#include <plat.h>
#include <tinyprintf.h>
#include <board.h>
#include <plat/regs.h>
#include <plat/imx/lpuart.h>
#include <plat/imx/usdhc.h>
#include <plat/imx/gpt.h>
#include <plat/imx/wdog.h>
#include <plat/sci/ipc.h>
#include <plat/sci/sci.h>
#include <plat/imx8qxp_pads.h>
#include <plat/iomux.h>

static struct usdhc_device usdhc0;
static struct gp_timer tmr0;
static struct imx_wdog_device wdog_device;
static struct lpuart_device uart_device;
sc_ipc_t ipc_handle;

/* Platform API Calls */
void      plat_reset(void)
{
}

uint32_t  plat_get_us_tick(void)
{
    return 0;// gp_timer_get_tick(&tmr0);
}

void      plat_wdog_init(void)
{


}

void      plat_wdog_kick(void)
{
}

uint32_t  plat_early_init(void)
{
    volatile uint32_t reg;
    uint32_t err;

    
	if (sc_ipc_open(&ipc_handle, SC_IPC_CH) != SC_ERR_NONE) 
    {
		/* No console available now */
		while (1);
	}

	/* Power up UART0 */
	sc_pm_set_resource_power_mode(ipc_handle, SC_R_UART_0, SC_PM_PW_MODE_ON);

	/* Set UART0 clock root to 80 MHz */
	sc_pm_clock_rate_t rate = 80000000;
	sc_pm_set_clock_rate(ipc_handle, SC_R_UART_0, 2, &rate);

	/* Enable UART0 clock root */
	sc_pm_clock_enable(ipc_handle, SC_R_UART_0, 2, true, false);

#define UART_PAD_CTRL	(PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | \
			(SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | \
			(SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
			(SC_PAD_28FDSOI_DSE_DV_LOW << PADRING_DSE_SHIFT) | \
			(SC_PAD_28FDSOI_PS_PD << PADRING_PULL_SHIFT))
	/* Configure UART pads */
	sc_pad_set(ipc_handle, SC_P_UART0_RX, UART_PAD_CTRL);

	sc_pad_set(ipc_handle, SC_P_UART0_TX, UART_PAD_CTRL);

    //board_early_init();

    uart_device.base = 0x5A060000;//board_get_debug_uart();
    uart_device.baudrate = 13;

    lpuart_init(&uart_device);

    init_printf(NULL, &plat_uart_putc);

    tfp_printf("Hello IMX8X\n\r");


    tmr0.base = 0x5D140000;
    tmr0.pr = 24;

    gp_timer_init(&tmr0);

    while(1);

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
}

uint32_t  plat_sha256_update(uintptr_t bfr, uint32_t sz)
{
}

uint32_t  plat_sha256_finalize(uintptr_t out)
{
}

uint32_t  plat_rsa_enc(uint8_t *sig, uint32_t sig_sz, uint8_t *out, 
                        struct asn1_key *k)
{
}

/* USB Interface API */
uint32_t  plat_usb_init(struct usb_device *dev)
{

    return PB_OK;
}

void      plat_usb_task(struct usb_device *dev)
{
}

uint32_t  plat_usb_transfer (struct usb_device *dev, 
                             uint8_t ep, 
                             uint8_t *bfr, 
                             uint32_t sz)
{
}

void plat_usb_set_address(struct usb_device *dev, uint32_t addr)
{
}

void plat_usb_set_configuration(struct usb_device *dev)
{
}

void plat_usb_wait_for_ep_completion(struct usb_device *dev, uint32_t ep)
{
    struct dwc3_device *d = (struct dwc3_device*) dev->platform_data;
}

/* UART Interface */

void plat_uart_putc(void *ptr, char c) 
{
    UNUSED(ptr);
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


