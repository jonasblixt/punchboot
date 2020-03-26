/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <board.h>
#include <plat.h>
#include <io.h>
#include <uuid.h>
#include <plat/imx/gpt.h>
#include <plat/imx/caam.h>
#include <plat/imx/ocotp.h>
#include <plat/imx/imx_uart.h>
#include <plat/imx/usdhc.h>
#include <plat/imx/wdog.h>
#include <plat/imx/hab.h>
#include <plat/imx/ehci.h>
#include <board/config.h>
#include <fuse.h>
#include <plat/imx6ul/plat.h>

static struct pb_platform_setup plat;
extern const struct fuse fuses[];

static struct fuse lock_fuse =
        IMX6UL_FUSE_BANK_WORD(0, 6, "lockfuse");

static struct fuse fuse_uid0 =
        IMX6UL_FUSE_BANK_WORD(0, 1, "UID0");

static struct fuse fuse_uid1 =
        IMX6UL_FUSE_BANK_WORD(0, 2, "UID1");

#define IMX6UL_FUSE_SHADOW_BASE 0x021BC000

/* Platform API Calls */

uint32_t plat_setup_device(struct param *params)
{
    uint32_t err;

    /* Read fuses */
    foreach_fuse(f, (struct fuse *) fuses)
    {
        err = plat_fuse_read(f);

        LOG_DBG("Fuse %s: 0x%08x", f->description, f->value);
        if (err != PB_OK)
        {
            LOG_ERR("Could not access fuse '%s'", f->description);
            return err;
        }
    }

    /* Perform the actual fuse programming */

    LOG_INFO("Writing fuses");

    foreach_fuse(f, fuses)
    {
        if ((f->value & f->default_value) != f->default_value)
        {
            f->value = f->default_value;
            err = plat_fuse_write(f);

            if (err != PB_OK)
                return err;
        }
        else
        {
            LOG_DBG("Fuse %s already programmed", f->description);
        }
    }

    return board_setup_device(params);
}

uint32_t plat_setup_lock(void)
{
    uint32_t err;
    uint32_t security_state;


    err = plat_get_security_state(&security_state);

    if (err != PB_OK)
        return err;

    if (security_state != PB_SECURITY_STATE_CONFIGURED_OK)
    {
        LOG_ERR("Device security state is not CONFIGURED_OK, aborting (%u)",
                security_state);
        return PB_ERR;
    }

    LOG_INFO("About to change security state to locked");

    lock_fuse.value = 0x02;

    err = plat_fuse_write(&lock_fuse);

    if (err != PB_OK)
        return err;

    return PB_OK;
}

bool plat_force_recovery(void)
{
    return board_force_recovery(&plat);
}

uint32_t plat_prepare_recovery(void)
{
    return board_prepare_recovery(&plat);
}

void plat_preboot_cleanup(void)
{
    pb_setbit32(1<<1, plat.usb0.base + EHCI_CMD);
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
            LOG_ERR("Could not access fuse '%s'", f->description);
            return err;
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

static __a16b const char platform_namespace_uuid[] =
    "\xae\xda\x39\xbe\x79\x2b\x4d\xe5\x85\x8a\x4c\x35\x7b\x9b\x63\x02";

static __a16b uint8_t out_tmp[16];

uint32_t plat_get_uuid(char *out)
{
    uint32_t err;
    plat_fuse_read(&fuse_uid0);
    plat_fuse_read(&fuse_uid1);

    uint32_t uid[2];

    uid[0] = fuse_uid0.value;
    uid[1] = fuse_uid1.value;

    LOG_INFO("%08x %08x", fuse_uid0.value, fuse_uid1.value);

    err = uuid_gen_uuid3(platform_namespace_uuid, 16,
                          (const char *) uid, 8, (char *)out_tmp);
    memcpy(out, out_tmp, 16);

    return err;
}

uint32_t plat_get_params(struct param **pp)
{
    char uuid_raw[16];

    param_add_str((*pp)++, "Platform", "NXP IMX6UL");
    plat_get_uuid(uuid_raw);
    param_add_uuid((*pp)++, "Device UUID", uuid_raw);
    return PB_OK;
}


void plat_reset(void)
{
    imx_wdog_reset_now();
}

uint32_t  plat_get_us_tick(void)
{
    return gp_timer_get_tick(&plat.tmr0);
}

void plat_wdog_init(void)
{
    imx_wdog_init(&plat.wdog, 5);
}

void plat_wdog_kick(void)
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
        LOG_ERR("Could not write fuse %s", s);
        return PB_ERR;
    }

    LOG_INFO("Writing: %s", s);

    return ocotp_write(f->bank, f->word, f->value);
}

uint32_t  plat_fuse_to_string(struct fuse *f, char *s, uint32_t n)
{
    return snprintf(s, n,
            "   FUSE<%u,%u> 0x%04x %s = 0x%08x",
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
                                  (uint8_t *) bfr,
                                  no_of_blocks,
                                  1, 0);
}

uint32_t plat_read_block(uint32_t lba_offset,
                         uintptr_t bfr,
                         uint32_t no_of_blocks)
{
    return usdhc_emmc_xfer_blocks(&plat.usdhc0,
                                  lba_offset,
                                  (uint8_t *) bfr,
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


/* USB Interface API */
uint32_t  plat_usb_init(struct usb_device *dev)
{
    uint32_t reg;

    dev->platform_data = (void *) &plat.usb0;

    /* Enable USB PLL */
    reg = pb_read32(0x020C8000+0x10);
    reg |= (1<<6);
    pb_write32(reg, 0x020C8000+0x10);

    /* Power up USB */
    pb_write32((1 << 31) | (1 << 30), 0x020C9038);
    pb_write32(0xFFFFFFFF, 0x020C9008);
    return ehci_usb_init(dev);
}

void plat_usb_task(struct usb_device *dev)
{
    ehci_usb_task(dev);
}

uint32_t plat_usb_transfer(struct usb_device *dev, uint8_t ep,
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


uint32_t plat_early_init(void)
{
    uint32_t reg;
    uint32_t err;

    board_early_init(&plat);
    /* Unmask wdog in SRC control reg */
    pb_write32(0, 0x020D8000);
    plat_wdog_init();
    gp_timer_init(&plat.tmr0);

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
        __asm__("nop");

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

    imx_uart_init(&plat.uart0);

    plat.ocotp.base = 0x021BC000;
    plat.ocotp.words_per_bank = 8;
    ocotp_init(&plat.ocotp);

    err = usdhc_emmc_init(&plat.usdhc0);

    if (err != PB_OK)
    {
        LOG_ERR("Could not initialize eMMC");
        return err;
    }

    /* Configure CAAM */
    plat.caam.base = 0x02141000;

    if (caam_init(&plat.caam) != PB_OK)
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

    if (imx_wdog_kick() != PB_OK)
        LOG_ERR("WDOG kick failed");
    return PB_OK;
}

