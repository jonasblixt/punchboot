
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
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
#include <pb/transport.h>

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


uint32_t plat_setup_device(void)
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

    return board_setup_device();
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

int plat_get_uuid(struct pb_crypto *crypto, char *out)
{
    uint32_t uid[2];

    sc_misc_unique_id(plat.ipc_handle, &uid[0], &uid[1]);

    return uuid_gen_uuid3(crypto, platform_namespace_uuid,
                          (const char *) uid, 8, out);
}

/* Platform API Calls */

bool plat_force_recovery(void)
{
    return board_force_recovery(&plat);
}

void plat_reset(void)
{
    sc_pm_reset(plat.ipc_handle, SC_PM_RESET_TYPE_BOARD);
}

uint32_t  plat_get_us_tick(void)
{
    return gp_timer_get_tick(&plat.tmr0);
}

void plat_wdog_init(void)
{
    sc_timer_set_wdog_timeout(plat.ipc_handle, 3000);
    sc_timer_set_wdog_action(plat.ipc_handle, SC_RM_PT_ALL,
                             SC_TIMER_WDOG_ACTION_BOARD);
    sc_timer_start_wdog(plat.ipc_handle, true);
}

void plat_wdog_kick(void)
{
    sc_timer_ping_wdog(plat.ipc_handle);
}

int plat_early_init(struct pb_storage *storage,
                    struct pb_transport *transport,
                    struct pb_console *console,
                    struct pb_crypto *crypto,
                    struct pb_command_context *command_ctx,
                    struct pb_boot_context *boot,
                    struct bpak_keystore *keystore)
{
    int err = PB_OK;

    sc_ipc_open(&plat.ipc_handle, SC_IPC_BASE);

    plat_wdog_init();

    err = board_early_init(&plat, storage, transport, console, crypto,
                            command_ctx, boot, keystore);

    if (err != PB_OK)
        return err;

    err = gp_timer_init(&plat.tmr0);

    if (err != PB_OK)
        return err;



    return err;
}


void plat_preboot_cleanup(void)
{

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

    err = sc_misc_otp_fuse_read(plat.ipc_handle, f->addr,
                                (uint32_t *) &(f->value));

    return (err == SC_ERR_NONE)?PB_OK:PB_ERR;
}

uint32_t  plat_fuse_write(struct fuse *f)
{
    char s[64];
    uint32_t err;

    plat_fuse_to_string(f, s, 64);

    LOG_INFO("Fusing %s", s);

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


