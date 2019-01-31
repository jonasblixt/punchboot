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

static struct usdhc_device usdhc0;
static struct gp_timer tmr0;
static struct imx_wdog_device wdog_device;
static struct lpuart_device uart_device;

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

uint32_t  plat_early_init(void)
{
    volatile uint32_t reg;
    uint32_t err;

    


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


