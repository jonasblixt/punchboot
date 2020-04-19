
/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <pb/pb.h>
#include <pb/io.h>
#include <pb/plat.h>
#include <pb/board.h>
#include <uuid/uuid.h>
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
#include <plat/defs.h>

#define LPCG_CLOCK_MASK         0x3U
#define LPCG_CLOCK_OFF          0x0U
#define LPCG_CLOCK_ON           0x2U
#define LPCG_CLOCK_AUTO         0x3U
#define LPCG_CLOCK_STOP         0x8U

#define LPCG_ALL_CLOCK_OFF      0x00000000U
#define LPCG_ALL_CLOCK_ON       0x22222222U
#define LPCG_ALL_CLOCK_AUTO     0x33333333U
#define LPCG_ALL_CLOCK_STOP     0x88888888U

static struct imx8x_private private;
extern struct fuse fuses[];
extern const uint32_t rom_key_map[];

static struct pb_result_slc_key_status key_status;

static enum pb_slc slc_status;

static struct fuse rom_key_revoke_fuse = IMX8X_FUSE_ROW(11, "Revoke");

static const char platform_namespace_uuid[] =
    "\xae\xda\x39\xbe\x79\x2b\x4d\xe5\x85\x8a\x4c\x35\x7b\x9b\x63\x02";

int plat_get_uuid(char *out)
{
    uint32_t uid[2];

    sc_misc_unique_id(private.ipc, &uid[0], &uid[1]);

    return uuid_gen_uuid3(platform_namespace_uuid,
                          (const char *) uid, 8, out);
}

/* Platform API Calls */

bool plat_force_command_mode(void)
{
    return board_force_command_mode(&private);
}

void plat_reset(void)
{
    sc_pm_reset(private.ipc, SC_PM_RESET_TYPE_BOARD);
}

unsigned int plat_get_us_tick(void)
{
    return gp_timer_get_tick();
}

void plat_wdog_init(void)
{
    sc_timer_set_wdog_timeout(private.ipc, 3000);
    sc_timer_set_wdog_action(private.ipc, SC_RM_PT_ALL,
                             SC_TIMER_WDOG_ACTION_BOARD);
    sc_timer_start_wdog(private.ipc, true);
}

void plat_wdog_kick(void)
{
    sc_timer_ping_wdog(private.ipc);
}

int plat_early_init(void)
{
    int rc = PB_OK;
    sc_pm_clock_rate_t rate;

    sc_ipc_open(&private.ipc, SC_IPC_BASE);
    plat_console_init();

    /* Setup GPT0 */
    sc_pm_set_resource_power_mode(private.ipc, SC_R_GPT_0, SC_PM_PW_MODE_ON);
    rate = 24000000;
    sc_pm_set_clock_rate(private.ipc, SC_R_GPT_0, 2, &rate);

    rc = sc_pm_clock_enable(private.ipc, SC_R_GPT_0, 2, true, false);

    if (rc != SC_ERR_NONE)
        return -PB_ERR;

    /* Enable usb stuff */
    sc_pm_set_resource_power_mode(private.ipc, SC_R_USB_0, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(private.ipc, SC_R_USB_0_PHY, SC_PM_PW_MODE_ON);

    pb_clrbit32((1 << 31) | (1 << 30), 0x5B100030);

    /* Enable USB PLL */
    pb_write32(0x00E03040, 0x5B100000+0xa0);

    /* Power up USB */
    pb_write32(0x00, 0x5B100000);

    rc = board_early_init(&private);

    if (rc != PB_OK)
        return rc;

    rc = gp_timer_init(CONFIG_IMX_GPT_BASE, CONFIG_IMX_GPT_PR);

    if (rc != PB_OK)
        return rc;

    return rc;
}

/* FUSE Interface */
int plat_fuse_read(struct fuse *f)
{
    sc_err_t err;

    if (!(f->status & FUSE_VALID))
        return PB_ERR;

    if (!f->addr)
    {
        f->addr = f->bank;
    }

    err = sc_misc_otp_fuse_read(private.ipc, f->addr,
                                (uint32_t *) &(f->value));

    return (err == SC_ERR_NONE)?PB_OK:PB_ERR;
}

int plat_fuse_write(struct fuse *f)
{
    char s[64];
    uint32_t err;

    plat_fuse_to_string(f, s, 64);

    LOG_INFO("Fusing %s", s);

    err = sc_misc_otp_fuse_write(private.ipc, f->addr, f->value);

    return (err == SC_ERR_NONE)?PB_OK:PB_ERR;
}

int plat_fuse_to_string(struct fuse *f, char *s, uint32_t n)
{
    return snprintf(s, n,
            "   FUSE<%u> %s = 0x%08x",
                f->bank,
                f->description, f->value);
}

/* Console API */

int plat_console_init(void)
{
    sc_pm_clock_rate_t rate;

    /* Power up UART0 */
    sc_pm_set_resource_power_mode(private.ipc, SC_R_UART_0, SC_PM_PW_MODE_ON);

    /* Set UART0 clock root to 80 MHz */
    rate = 80000000;
    sc_pm_set_clock_rate(private.ipc, SC_R_UART_0, SC_PM_CLK_PER, &rate);

    /* Enable UART0 clock root */
    sc_pm_clock_enable(private.ipc, SC_R_UART_0, SC_PM_CLK_PER, true, false);

    /* Configure UART pads */
    sc_pad_set(private.ipc, SC_P_UART0_RX, UART_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_UART0_TX, UART_PAD_CTRL);

    return imx_lpuart_init();
}

int plat_console_putchar(char c)
{
    imx_lpuart_write((char *) &c, 1);
    return PB_OK;
}

/* Crypto API */

int plat_crypto_init(void)
{

    sc_pm_set_resource_power_mode(private.ipc,
                                SC_R_CAAM_JR2, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(private.ipc,
                                SC_R_CAAM_JR2_OUT, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(private.ipc,
                                SC_R_CAAM_JR3, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(private.ipc,
                                SC_R_CAAM_JR3_OUT, SC_PM_PW_MODE_ON);
    return imx_caam_init();
}

int plat_hash_init(struct pb_hash_context *ctx, enum pb_hash_algs alg)
{
    return caam_hash_init(ctx, alg);
}

int plat_hash_update(struct pb_hash_context *ctx, void *buf, size_t size)
{
    return caam_hash_update(ctx, buf, size);
}

int plat_hash_finalize(struct pb_hash_context *ctx, void *buf, size_t size)
{
    return caam_hash_finalize(ctx, buf, size);
}

int plat_pk_verify(void *signature, size_t size, struct pb_hash_context *hash,
                        struct bpak_key *key)
{
    return caam_pk_verify(hash, key, signature, size);
}

/* SLC API */

int plat_slc_init(void)
{
    return plat_slc_read(&slc_status);
}

int plat_slc_set_configuration(void)
{
    int err;

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

    return PB_OK;
}

int plat_slc_set_configuration_lock(void)
{
    int err;
    uint16_t lc;
    uint16_t monotonic;
    uint32_t uid_l;
    uint32_t uid_h;

    sc_misc_seco_chip_info(private.ipc, &lc, &monotonic, &uid_l, &uid_h);

    if (lc == 128)
    {
        LOG_INFO("Configuration already locked");
        return PB_OK;
    }

    LOG_INFO("About to change security state to locked");

    err = sc_misc_seco_forward_lifecycle(private.ipc, 16);

    return (err == SC_ERR_NONE)?PB_OK:PB_ERR;
}

int plat_slc_set_end_of_life(void)
{
    return -PB_ERR;
}

int plat_slc_read(enum pb_slc *slc)
{
    int err;
    (*slc) = PB_SLC_NOT_CONFIGURED;

    /* Read fuses */
    foreach_fuse(f, (struct fuse *) fuses)
    {
        err = plat_fuse_read(f);

        if (f->value)
        {
            (*slc) = PB_SLC_CONFIGURATION;
            break;
        }

        if (err != PB_OK)
        {
            LOG_ERR("Could not access fuse '%s'", f->description);
            return err;
        }
    }

    uint16_t lc;
    uint16_t monotonic;
    uint32_t uid_l;
    uint32_t uid_h;

    sc_misc_seco_chip_info(private.ipc, &lc, &monotonic, &uid_l, &uid_h);

    if (lc == 128)
    {
        (*slc) = PB_SLC_CONFIGURATION_LOCKED;
    }

    return PB_OK;
}

int plat_slc_key_active(uint32_t id, bool *active)
{
    int rc;
    unsigned int rom_index = 0;
    bool found_key = false;

    *active = false;

    for (int i = 0; i < 16; i++)
    {
        if (!rom_key_map[i])
            break;

        if (rom_key_map[i] == id)
        {
            rom_index = i;
            found_key = true;
        }
    }

    if (!found_key)
    {
        LOG_ERR("Could not find key");
        return -PB_ERR;
    }

    rc =  plat_fuse_read(&rom_key_revoke_fuse);

    if (rc != PB_OK)
    {
        LOG_ERR("Could not read revoke fuse");
        return rc;
    }

    uint32_t revoke_value = (1 << rom_index);

    if ((rom_key_revoke_fuse.value & revoke_value) == revoke_value)
        (*active) = false;
    else
        (*active) = true;

    return PB_OK;
}

int plat_slc_revoke_key(uint32_t id)
{
    int rc;
    unsigned int rom_index = 0;
    bool found_key = false;
    LOG_INFO("Revoking key 0x%x", id);


    for (int i = 0; i < 16; i++)
    {
        if (!rom_key_map[i])
            break;

        if (rom_key_map[i] == id)
        {
            rom_index = i;
            found_key = true;
        }
    }

    if (!found_key)
    {
        LOG_ERR("Could not find key");
        return -PB_ERR;
    }

    rc =  plat_fuse_read(&rom_key_revoke_fuse);

    if (rc != PB_OK)
    {
        LOG_ERR("Could not read revoke fuse");
        return rc;
    }

    LOG_DBG("Revoke fuse = 0x%x", rom_key_revoke_fuse.value);

    uint32_t revoke_value = (1 << rom_index);

    if ((rom_key_revoke_fuse.value & revoke_value) == revoke_value)
    {
        LOG_INFO("Key already revoked");
        return PB_OK;
    }

    LOG_DBG("About to write 0x%x", revoke_value);

    rom_key_revoke_fuse.value |= revoke_value;
    rom_key_revoke_fuse.value |= (revoke_value << 16);

    return plat_fuse_write(&rom_key_revoke_fuse);
}

int plat_slc_get_key_status(struct pb_result_slc_key_status **status)
{
    int rc;

    memset(&key_status, 0, sizeof(key_status));

    if (status)
        (*status) = &key_status;

    rc = plat_fuse_read(&rom_key_revoke_fuse);

    if (rc != PB_OK)
        return rc;

    for (int i = 0; i < 16; i++)
    {
        if (!rom_key_map[i])
            break;

        if (rom_key_revoke_fuse.value & (1 << i))
        {
            key_status.active[i] = 0;
            key_status.revoked[i] = rom_key_map[i];
        }
        else
        {
            key_status.revoked[i] = 0;
            key_status.active[i] = rom_key_map[i];
        }
    }

    return PB_OK;
}

/* Transport API */

int imx_ehci_set_address(uint32_t addr)
{
    pb_write32((addr << 25) | (1 <<24), CONFIG_EHCI_BASE+EHCI_DEVICEADDR);
    return PB_OK;
}

int plat_transport_init(void)
{

    return imx_ehci_usb_init();
}

int plat_transport_process(void)
{
    return imx_ehci_usb_process();
}

int plat_transport_write(void *buf, size_t size)
{
    return imx_ehci_usb_write(buf, size);
}

int plat_transport_read(void *buf, size_t size)
{
    return imx_ehci_usb_read(buf, size);
}

bool plat_transport_ready(void)
{
    return imx_ehci_usb_ready();
}

int plat_patch_bootargs(void *fdt, int offset, bool verbose_boot)
{
    return board_patch_bootargs(&private, fdt, offset, verbose_boot);
}

int imx_usdhc_plat_init(struct usdhc_device *dev)
{
    int rc;
    unsigned int rate;

    sc_pm_set_resource_power_mode(private.ipc, SC_R_SDHC_0, SC_PM_PW_MODE_ON);


    sc_pm_clock_enable(private.ipc, SC_R_SDHC_0, SC_PM_CLK_PER, false, false);

    rc = sc_pm_set_clock_parent(private.ipc, SC_R_SDHC_0, 2, SC_PM_PARENT_PLL1);

    if (rc != SC_ERR_NONE)
    {
        LOG_ERR("usdhc set clock parent failed");
        return -PB_ERR;
    }

    rate = 200000000;
    sc_pm_set_clock_rate(private.ipc, SC_R_SDHC_0, 2, &rate);

    if (rate != 200000000)
    {
        LOG_INFO("USDHC rate %u Hz", rate);
    }

    rc = sc_pm_clock_enable(private.ipc, SC_R_SDHC_0, SC_PM_CLK_PER,
                                true, false);

    if (rc != SC_ERR_NONE)
    {
        LOG_ERR("SDHC_0 per clk enable failed!");
        return -PB_ERR;
    }


    sc_pad_set(private.ipc, SC_P_EMMC0_CLK, ESDHC_CLK_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_CMD, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_DATA0, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_DATA1, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_DATA2, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_DATA3, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_DATA4, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_DATA5, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_DATA6, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_DATA7, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_STROBE, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_RESET_B, ESDHC_PAD_CTRL);

    return PB_OK;
}

int plat_status(void *response_bfr,
                    size_t *response_size)
{
    return board_status(&private, response_bfr, response_size);
}

int plat_command(uint32_t command,
                     void *bfr,
                     size_t size,
                     void *response_bfr,
                     size_t *response_size)
{
    return board_command(&private, command, bfr, size,
                            response_bfr, response_size);
}
