#include <pb.h>
#include <io.h>
#include <plat.h>
#include <plat/regs.h>
#include <plat/imx/imx_uart.h>

/* Platform API Calls */
void      plat_reset(void)
{
}

uint32_t  plat_get_us_tick(void)
{
}

void      plat_wdog_init(void)
{
}

void      plat_wdog_kick(void)
{
}

uint32_t  plat_early_init(void)
{
    uint32_t reg;
    /* Enable and ungate WDOG clocks */
    pb_write32((1 << 28) ,0x30388004 + 0x80*114);
    pb_write32(3, 0x30384004 + 0x10*83);
    pb_write32(3, 0x30384004 + 0x10*84);
    pb_write32(3, 0x30384004 + 0x10*85);

    board_init();

    imx_uart_init(board_get_debug_uart());

    init_printf(NULL, &plat_uart_putc);

    return PB_OK;
}

/* EMMC Interface */
uint32_t  plat_write_block(uint32_t lba_offset, 
                                uint8_t *bfr, 
                                uint32_t no_of_blocks)
{
}

uint32_t  plat_read_block( uint32_t lba_offset, 
                                uint8_t *bfr, 
                                uint32_t no_of_blocks)
{
}

uint32_t  plat_switch_part(uint8_t part_no)
{
}

uint64_t  plat_get_lastlba(void)
{
}

/* Crypto Interface */
uint32_t  plat_sha256_init(void)
{
}

uint32_t  plat_sha256_update(uint8_t *bfr, uint32_t sz)
{
}

uint32_t  plat_sha256_finalize(uint8_t *out)
{
}

uint32_t  plat_rsa_enc(uint8_t *sig, uint32_t sig_sz, uint8_t *out, 
                        struct asn1_key *k)
{
}

/* USB Interface API */
uint32_t  plat_usb_init(struct usb_device *dev)
{
}

void      plat_usb_task(struct usb_device *dev)
{
}

uint32_t  plat_usb_transfer (struct usb_device *dev, uint8_t ep, 
                            uint8_t *bfr, uint32_t sz)
{
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
}

uint32_t  plat_fuse_write(struct fuse *f)
{
}

uint32_t  plat_fuse_to_string(struct fuse *f, char *s, uint32_t n)
{
}


