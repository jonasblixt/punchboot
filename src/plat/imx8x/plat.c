/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/timer/imx_gpt.h>
#include <drivers/uart/imx_lpuart.h>
#include <drivers/usb/imx_ci_udc.h>
#include <pb/console.h>
#include <pb/crypto.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/timestamp.h>
#include <pb/utils_def.h>
#include <plat/imx8x/fusebox.h>
#include <plat/imx8x/imx8x.h>
#include <plat/imx8x/sci/svc/misc/api.h>
#include <plat/imx8x/sci/svc/pm/api.h>
#include <plat/imx8x/sci/svc/seco/api.h>
#include <plat/imx8x/sci/svc/timer/api.h>
#include <stdio.h>
#include <string.h>
#include <uuid.h>
#include <xlat_tables.h>

IMPORT_SYM(uintptr_t, _code_start, code_start);
IMPORT_SYM(uintptr_t, _code_end, code_end);
IMPORT_SYM(uintptr_t, _data_region_start, data_start);
IMPORT_SYM(uintptr_t, _data_region_end, data_end);
IMPORT_SYM(uintptr_t, _ro_data_region_start, ro_data_start);
IMPORT_SYM(uintptr_t, _ro_data_region_end, ro_data_end);
IMPORT_SYM(uintptr_t, _stack_start, stack_start);
IMPORT_SYM(uintptr_t, _stack_end, stack_end);
IMPORT_SYM(uintptr_t, _zero_region_start, rw_nox_start);
IMPORT_SYM(uintptr_t, _no_init_end, rw_nox_end);

const char *platform_ns_uuid = "\xae\xda\x39\xbe\x79\x2b\x4d\xe5\x85\x8a\x4c\x35\x7b\x9b\x63\x02";

static struct imx8x_platform plat;
static int boot_reason;

static const mmap_region_t imx_mmap[] = {
    /* Map all of the normal I/O space */
    MAP_REGION_FLAT(IMX_REG_BASE, IMX_REG_SIZE, MT_DEVICE | MT_RW),
    /* CAAM JR2 */
    MAP_REGION_FLAT(0x31430000, (64 * 1024), MT_DEVICE | MT_RW),
    { 0 }
};

int plat_get_unique_id(uint8_t *output, size_t *length)
{
    union {
        uint32_t uid[2];
        uint8_t uid_bytes[8];
    } plat_unique;

    if (*length < sizeof(plat_unique.uid_bytes))
        return -PB_ERR_BUF_TOO_SMALL;
    *length = sizeof(plat_unique.uid_bytes);

    sc_misc_unique_id(plat.ipc, &plat_unique.uid[0], &plat_unique.uid[1]);
    memcpy(output, plat_unique.uid_bytes, sizeof(plat_unique.uid_bytes));
    return PB_OK;
}

static int get_soc_rev(uint32_t *soc_id, uint32_t *soc_rev)
{
    uint32_t id;
    sc_err_t err;

    if (!soc_id || !soc_rev)
        return -1;

    err = sc_misc_get_control(plat.ipc, SC_R_SYSTEM, SC_C_ID, &id);
    if (err != SC_ERR_NONE)
        return err;

    *soc_rev = (id >> 5) & 0xf;
    *soc_id = id & 0x1f;

    return 0;
}

/* Platform API Calls */

void plat_reset(void)
{
    sc_pm_reset(plat.ipc, SC_PM_RESET_TYPE_BOARD);
}

unsigned int plat_get_us_tick(void)
{
    return imx_gpt_get_tick();
}

static sc_err_t imx8x_wdog_init(void)
{
#ifdef CONFIG_ENABLE_WATCHDOG
    sc_rm_pt_t partition;
    sc_err_t err;

    err = sc_rm_get_partition(plat.ipc, &partition);
    if (err) {
        return err;
    }

    err = sc_timer_set_wdog_action(plat.ipc, partition, SC_TIMER_WDOG_ACTION_BOARD);
    if (err) {
        return err;
    }

    err = sc_timer_set_wdog_timeout(plat.ipc, CONFIG_WATCHDOG_TIMEOUT * 1000);
    if (err) {
        return err;
    }

    /* If the last argument to sc_timer_start_wdog is set to true, the
     * watchdog will be locked and cannot be reconfigured. Currently,
     * it should be possible to reconfigure the watchdog from Linux. */

    err = sc_timer_start_wdog(plat.ipc, false);
    if (err) {
        return err;
    }

    return 0;
#endif
}

void plat_wdog_kick(void)
{
    sc_timer_ping_wdog(plat.ipc);
}

static void imx8x_systick_setup(void)
{
    int rc;
    sc_pm_clock_rate_t rate;

    /* Setup GPT0 */
    sc_pm_set_resource_power_mode(plat.ipc, SC_R_GPT_0, SC_PM_PW_MODE_ON);
    rate = 24000000;
    sc_pm_set_clock_rate(plat.ipc, SC_R_GPT_0, 2, &rate);

    rc = sc_pm_clock_enable(plat.ipc, SC_R_GPT_0, 2, true, false);

    if (rc != SC_ERR_NONE)
        goto systick_fatal;

    rc = imx_gpt_init(IMX_GPT_BASE, rate);
    if (rc == 0)
        return;

systick_fatal:
    plat_reset();
}

static void imx8x_load_boot_reason(void)
{
    int rc;
    sc_pm_reset_reason_t sc_reset_reason;

    if ((rc = sc_pm_reset_reason(plat.ipc, &sc_reset_reason)) == 0) {
        boot_reason = (int)sc_reset_reason;
    } else {
        boot_reason = -rc;
    }

    rc = get_soc_rev(&plat.soc_id, &plat.soc_rev);

    if (rc != 0) {
        plat.soc_id = 0;
        plat.soc_rev = 0;
    }
}

static void imx8x_mmu_init(void)
{
    /* Configure MMU */
    reset_xlat_tables();

    /* Map ATF hole */
    mmap_add_region(
        BOARD_RAM_BASE, BOARD_RAM_BASE, (0x20000), MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add_region(code_start, code_start, code_end - code_start, MT_RO | MT_MEMORY | MT_EXECUTE);

    mmap_add_region(
        data_start, data_start, data_end - data_start, MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add_region(ro_data_start,
                    ro_data_start,
                    ro_data_end - ro_data_start,
                    MT_RO | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add_region(
        stack_start, stack_start, stack_end - stack_start, MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add_region(rw_nox_start,
                    rw_nox_start,
                    rw_nox_end - rw_nox_start,
                    MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    /* Add the rest of the RAM */
    mmap_add_region(
        rw_nox_end, rw_nox_end, BOARD_RAM_END - rw_nox_end, MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add(imx_mmap);

    init_xlat_tables();

    enable_mmu_el3(0);
}

int plat_init(void)
{
    sc_ipc_open(&plat.ipc, SC_IPC_BASE);

    sc_err_t wdog_rc = imx8x_wdog_init();
    imx8x_systick_setup();
    ts("Init"); /* This is the earliest TS we can have since it needs systick */
    imx8x_load_boot_reason();

    board_console_init(&plat);

    if (wdog_rc != 0) {
        /* We'll continue anyway, best effort */
        LOG_ERR("Failed to enable watchdog (%i)", wdog_rc);
    }

    ts("MMU start");
    imx8x_mmu_init();
    ts("MMU end");

    imx8x_fuse_init(plat.ipc);
    imx8x_rot_helpers_init(plat.ipc);
    imx8x_slc_helpers_init(plat.ipc);

    return PB_OK;
}

int plat_board_init(void)
{
    return board_init(&plat);
}

int plat_boot_reason(void)
{
    return boot_reason;
}

const char *plat_boot_reason_str(void)
{
    switch (boot_reason) {
    case SC_PM_RESET_REASON_POR:
        return "POR";
    case SC_PM_RESET_REASON_JTAG:
        return "JTAG";
    case SC_PM_RESET_REASON_SW:
        return "SW";
    case SC_PM_RESET_REASON_WDOG:
        return "WDOG";
    case SC_PM_RESET_REASON_LOCKUP:
        return "LOCKUP";
    case SC_PM_RESET_REASON_SNVS:
        return "SNVS";
    case SC_PM_RESET_REASON_TEMP:
        return "TEMP";
    case SC_PM_RESET_REASON_MSI:
        return "MSI";
    case SC_PM_RESET_REASON_UECC:
        return "UECC";
    case SC_PM_RESET_REASON_SCFW_WDOG:
        return "SCFW WDOG";
    case SC_PM_RESET_REASON_ROM_WDOG:
        return "ROM WDOG";
    case SC_PM_RESET_REASON_SECO:
        return "SECO";
    case SC_PM_RESET_REASON_SCFW_FAULT:
        return "SCFW Fault";
    default:
        return "Unknown";
    }
}
