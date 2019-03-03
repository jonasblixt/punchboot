#include <stdio.h>
#include <pb.h>
#include <io.h>
#include <plat.h>
#include <board.h>
#include <uuid.h>
#include <plat/imx8x/plat.h>
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

#define LPCG_CLOCK_MASK         0x3U
#define LPCG_CLOCK_OFF          0x0U
#define LPCG_CLOCK_ON           0x2U
#define LPCG_CLOCK_AUTO         0x3U
#define LPCG_CLOCK_STOP         0x8U

#define LPCG_ALL_CLOCK_OFF      0x00000000U
#define LPCG_ALL_CLOCK_ON       0x22222222U
#define LPCG_ALL_CLOCK_AUTO     0x33333333U
#define LPCG_ALL_CLOCK_STOP     0x88888888U

static struct pb_platform_setup plat;
extern struct fuse fuses[];

uint32_t plat_setup_lock(void)
{
    uint32_t err;

    LOG_INFO("About to change security state to locked");

    err = sc_misc_seco_forward_lifecycle(plat.ipc_handle, 16);

    return (err == SC_ERR_NONE)?PB_OK:PB_ERR;
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


uint32_t plat_get_security_state(uint32_t *state)
{
    uint32_t err;
    (*state) = PB_SECURITY_STATE_NOT_SECURE;

    /* Read fuses */
    foreach_fuse(f, (struct fuse *) fuses)
    {
        err = plat_fuse_read(f);
 
        if (f->value)
        {
            (*state) = PB_SECURITY_STATE_CONFIGURED_ERR;
            break;
        }

        if (err != PB_OK)
        {
            LOG_ERR("Could not access fuse '%s'",f->description);
            return err;
        }  
    }

    /*TODO: Check SECO for error events */
    (*state) = PB_SECURITY_STATE_CONFIGURED_OK;

    uint16_t lc;
    uint16_t monotonic;
    uint32_t uid_l;
    uint32_t uid_h;

    sc_misc_seco_chip_info(plat.ipc_handle, &lc, &monotonic, &uid_l, &uid_h);

    if (lc == 128)
        (*state) = PB_SECURITY_STATE_SECURE;

    return PB_OK;
}

static const char platform_namespace_uuid[] = 
    "\xae\xda\x39\xbe\x79\x2b\x4d\xe5\x85\x8a\x4c\x35\x7b\x9b\x63\x02";

uint32_t plat_get_uuid(char *out)
{

    uint16_t lc;
    uint16_t monotonic;
    uint32_t uid[2];

    sc_misc_seco_chip_info(plat.ipc_handle, &lc, &monotonic, &uid[0], &uid[1]);
    
    return uuid_gen_uuid3(platform_namespace_uuid,16,
                          (const char *) uid, 8, out);
}

uint32_t plat_get_params(struct param **pp)
{
    char uuid_raw[16];

    param_add_str((*pp)++, "Platform", "NXP IMX8X");
    plat_get_uuid(uuid_raw);
    param_add_uuid((*pp)++, "Device UUID",uuid_raw);
    return PB_OK;
}


/* Platform API Calls */
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
    sc_pm_reset(plat.ipc_handle,SC_PM_RESET_TYPE_BOARD);
}

uint32_t  plat_get_us_tick(void)
{
    return gp_timer_get_tick(&plat.tmr0);
}

void plat_wdog_init(void)
{
    sc_timer_set_wdog_timeout(plat.ipc_handle, 15000);
    sc_timer_set_wdog_action(plat.ipc_handle,SC_RM_PT_ALL,SC_TIMER_WDOG_ACTION_BOARD);
    sc_timer_start_wdog(plat.ipc_handle, true);
}

void plat_wdog_kick(void)
{
    sc_timer_ping_wdog(plat.ipc_handle);
}

uint32_t  plat_early_init(void)
{
    uint32_t err = PB_OK;
    sc_pm_clock_rate_t rate;
    
	sc_ipc_open(&plat.ipc_handle, SC_IPC_BASE);

	sc_pm_set_resource_power_mode(plat.ipc_handle, SC_R_GPIO_0, SC_PM_PW_MODE_ON);
	rate = 1000000;
	sc_pm_set_clock_rate(plat.ipc_handle, SC_R_GPIO_0, 2, &rate);
	sc_pm_clock_enable(plat.ipc_handle, SC_R_GPIO_0, 2, true, false);
    pb_setbit32(1 << 16, GPIO_BASE+0x04);
    pb_setbit32(1 << 16, GPIO_BASE);
    sc_pad_set(plat.ipc_handle, SC_P_SPI3_CS0, GPIO_PAD_CTRL | (4 << 27));

    plat_wdog_init();

    err = board_early_init(&plat);

    if (err != PB_OK)
    {
        return err;
    }

	/* Write to LPCG */
	pb_write32(LPCG_ALL_CLOCK_ON, 0x5B200000);

	/* Wait for clocks to start */
	while ((pb_read32(0x5B200000) & LPCG_ALL_CLOCK_STOP) != 0U)
        __asm__("nop");

    err = lpuart_init(&plat.uart0);

    if (err != PB_OK)
        return err;

    err = gp_timer_init(&plat.tmr0);

    if (err != PB_OK)
        return err;

    err = usdhc_emmc_init(&plat.usdhc0);

    if (err != PB_OK)
    {
        return err;
    }

	sc_pm_set_resource_power_mode(plat.ipc_handle, 
                                SC_R_CAAM_JR2, SC_PM_PW_MODE_ON);
	sc_pm_set_resource_power_mode(plat.ipc_handle, 
                                SC_R_CAAM_JR2_OUT, SC_PM_PW_MODE_ON);
	sc_pm_set_resource_power_mode(plat.ipc_handle, 
                                SC_R_CAAM_JR3, SC_PM_PW_MODE_ON);
	sc_pm_set_resource_power_mode(plat.ipc_handle, 
                                SC_R_CAAM_JR3_OUT, SC_PM_PW_MODE_ON);

    plat.caam.base = 0x31430000;
    err = caam_init(&plat.caam);

    if (err != PB_OK)
        return err;

    return err;
}

/* EMMC Interface */

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
    return plat.usdhc0.sectors-1;
}

void plat_preboot_cleanup(void)
{
    pb_clrbit32(1 << 16, GPIO_BASE);
}

/* USB Interface API */
uint32_t  plat_usb_init(struct usb_device *dev)
{

    dev->platform_data = (void *) &plat.usb;
	sc_pm_set_resource_power_mode(plat.ipc_handle, SC_R_USB_0, SC_PM_PW_MODE_ON);
	sc_pm_set_resource_power_mode(plat.ipc_handle, SC_R_USB_0_PHY, SC_PM_PW_MODE_ON);

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
    lpuart_putc(&plat.uart0, c);
}

/* FUSE Interface */
uint32_t  plat_fuse_read(struct fuse *f)
{
    sc_err_t err;

    if (!(f->status & FUSE_VALID))
        return PB_ERR;

    if (!f->addr)
    {
        f->addr = f->bank;
    }

    err = sc_misc_otp_fuse_read(plat.ipc_handle, f->addr, (uint32_t *) &(f->value));

    return (err == SC_ERR_NONE)?PB_OK:PB_ERR;
}

uint32_t  plat_fuse_write(struct fuse *f)
{
    char s[64];
    uint32_t err;

    plat_fuse_to_string(f, s, 64);

    LOG_INFO("Fusing %s",s);

    err = sc_misc_otp_fuse_write(plat.ipc_handle, f->addr, f->value);

    return (err == SC_ERR_NONE)?PB_OK:PB_ERR;
}

uint32_t  plat_fuse_to_string(struct fuse *f, char *s, uint32_t n)
{

    return snprintf(s, n,
            "   FUSE<%u> %s = 0x%08x",
                f->bank,
                f->description, f->value);
}


