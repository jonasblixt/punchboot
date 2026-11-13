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
#include <drivers/usb/imx_cdns3_udc.h>
#include <drivers/usb/imx_ci_udc.h>
#include <drivers/usb/imx_usb2_phy.h>
#include <pb/command.h>
#include <pb/console.h>
#include <pb/crypto.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/utils_def.h>
#include <plat/imx8x/fusebox.h>
#include <plat/imx8x/imx8x.h>
#include <plat/imx8x/sci/svc/misc/api.h>
#include <plat/imx8x/sci/svc/pm/api.h>
#include <plat/imx8x/sci/svc/seco/api.h>
#include <plat/imx8x/sci/svc/timer/api.h>
#include <plat/imx8x/seco_log.h>
#include <stdbool.h>
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

const struct imx8x_platform *imx8x_plat(void)
{
    return &plat;
}

sc_ipc_t imx8x_ipc(void)
{
    return plat.ipc;
}

int imx8x_pads_setup(const struct imx8x_pad *pads, size_t length)
{
    for (size_t n = 0; n < length; n++) {
        if (sc_pad_set(plat.ipc, pads[n].pad, pads[n].ctrl) != SC_ERR_NONE) {
            LOG_ERR("Pad %u failed", pads[n].pad);
            return -PB_ERR_IO;
        }
    }

    return PB_OK;
}

/* The SCFW's reset reason as plat_early_init() got it, negative before that
 * and when the SCFW didn't give one */
static int boot_reason = -1;

static const mmap_region_t imx_mmap[] = {
    /* Map all of the normal I/O space */
    MAP_REGION_FLAT(IMX_REG_BASE, IMX_REG_SIZE, MT_DEVICE | MT_RW),
    /* CAAM JR2 */
    MAP_REGION_FLAT(IMX_CAAM_JR2_BASE, (64 * 1024), MT_DEVICE | MT_RW),
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

    *soc_rev = IMX8X_SC_ID_REV(id);
    *soc_id = IMX8X_SC_ID_SOC(id);

    return 0;
}

/* Platform API Calls */

void plat_reset(void)
{
    /* Valid before plat_early_init() has opened the channel to the SCFW
     * (platform.md 3.1): the arch's exception handlers call it. Without a
     * handle the call below sends nothing, so open it here, as there */
    if (plat.ipc == 0)
        sc_ipc_open(&plat.ipc, SC_IPC_BASE);

    sc_pm_reset(plat.ipc, SC_PM_RESET_TYPE_BOARD);

    /* sc_pm_reset() returns when the SCFW refused the reset (SCFW API,
     * sc_pm_reset()). Nothing else resets from here: the watchdog does,
     * when it is armed */
    while (true) {
    }
}

uint32_t plat_get_us_tick(void)
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
#endif
    return SC_ERR_NONE;
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
        BOARD_RAM_BASE, BOARD_RAM_BASE, IMX8X_ATF_SIZE, MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

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

/* The SCFW's and the SECO's builds, the SoC and its temperature */
struct imx8x_info {
    uint32_t scfw_build;
    uint32_t scfw_commit;
    uint32_t seco_version;
    uint32_t seco_commit;
    bool temp_read;
    const char *temp_sign;
    unsigned int temp_tenths; /* Without the sign */
};

static void imx8x_read_info(struct imx8x_info *info)
{
    int16_t celsius;
    int8_t tenths;

    sc_misc_build_info(plat.ipc, &info->scfw_build, &info->scfw_commit);
    sc_seco_build_info(plat.ipc, &info->seco_version, &info->seco_commit);

    info->temp_read =
        (sc_misc_get_temp(plat.ipc, SC_R_SYSTEM, SC_MISC_TEMP, &celsius, &tenths) == SC_ERR_NONE);
    if (info->temp_read) {
        /* Both parts have the sign */
        int temp = celsius * 10 + tenths;

        info->temp_sign = (temp < 0) ? "-" : "";
        info->temp_tenths = (unsigned int)(temp < 0 ? -temp : temp);
    }
}

static void imx8x_log_info(void)
{
    struct imx8x_info info;

    imx8x_read_info(&info);
    LOG_INFO("SCFW %u (%x), SECO %u (%x)",
             (unsigned int)info.scfw_build,
             (unsigned int)info.scfw_commit,
             (unsigned int)info.seco_version,
             (unsigned int)info.seco_commit);
    LOG_INFO("SoC id %u, rev %u", (unsigned int)plat.soc_id, (unsigned int)plat.soc_rev);

    if (info.temp_read) {
        LOG_INFO(
            "Temperature %s%u.%u C", info.temp_sign, info.temp_tenths / 10, info.temp_tenths % 10);
    } else {
        LOG_ERR("Could not read the temperature");
    }
}

/* The named command 'info' (command-mode.md 5.6): what imx8x_log_info()
 * logs at boot, read again, as text that ends with a NUL */
static int
cmd_info(const uint8_t *request, size_t request_size, uint8_t *response, size_t *response_size)
{
    struct imx8x_info info;
    char temp[32];
    size_t room = *response_size;
    int n;

    (void)request;
    (void)request_size;

    imx8x_read_info(&info);

    if (info.temp_read) {
        (void)snprintf(temp,
                       sizeof(temp),
                       "%s%u.%u C",
                       info.temp_sign,
                       info.temp_tenths / 10,
                       info.temp_tenths % 10);
    } else {
        (void)snprintf(temp, sizeof(temp), "not read");
    }

    n = snprintf((char *)response,
                 room,
                 "SCFW %u (%x)\n"
                 "SECO %u (%x)\n"
                 "SoC id %u, rev %u\n"
                 "Temperature %s\n",
                 (unsigned int)info.scfw_build,
                 (unsigned int)info.scfw_commit,
                 (unsigned int)info.seco_version,
                 (unsigned int)info.seco_commit,
                 (unsigned int)plat.soc_id,
                 (unsigned int)plat.soc_rev,
                 temp);

    /* The text and its NUL, cut to the room there is */
    if (n < 0 || room == 0) {
        *response_size = 0;
    } else {
        *response_size = ((size_t)n < room) ? (size_t)n : room - 1;
        response[(*response_size)++] = 0;
    }

    return PB_OK;
}

DEFINE_COMMAND("info", "imx8x system info", cmd_info);

static sc_err_t wdog_rc;

int plat_early_init(void)
{
    sc_ipc_open(&plat.ipc, SC_IPC_BASE);

    wdog_rc = imx8x_wdog_init();
    imx8x_systick_setup();
    log_time_start();
    imx8x_load_boot_reason();
    return PB_OK;
}

int plat_init(void)
{
    if (wdog_rc != 0) {
        /* We'll continue anyway, best effort */
        LOG_ERR("Failed to enable watchdog (%i)", wdog_rc);
    }

    imx8x_mmu_init();
    LOG_DBG("MMU on");

    imx8x_log_info();

    imx8x_fuse_init(plat.ipc);
    imx8x_rot_helpers_init(plat.ipc);
    imx8x_slc_helpers_init(plat.ipc);

    return PB_OK;
}

#ifdef CONFIG_IMX8X_CM_USB3
/* Command mode's USB (platform.md 3.10): the i.MX8QXP's USB 3.0 port, the
 * Cadence USB3 controller with its PHY, SC_R_USB_2 */
static int cm_usb_init(void)
{
    int rc;

    /* Request power domains */
    sc_pm_set_resource_power_mode(plat.ipc, SC_R_USB_2, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(plat.ipc, SC_R_USB_2_PHY, SC_PM_PW_MODE_ON);

    /* Request clocks */
    rc = sc_pm_clock_enable(plat.ipc, SC_R_USB_2, SC_PM_CLK_PER, true, false);

    if (rc != SC_ERR_NONE) {
        LOG_ERR("USB_2 per clk enable failed!");
        return -PB_ERR;
    }

    rc = sc_pm_clock_enable(plat.ipc, SC_R_USB_2, SC_PM_CLK_MISC, true, false);

    if (rc != SC_ERR_NONE) {
        LOG_ERR("USB_2 misc clk enable failed!");
        return -PB_ERR;
    }

    rc = sc_pm_clock_enable(plat.ipc, SC_R_USB_2, SC_PM_CLK_MST_BUS, true, false);

    if (rc != SC_ERR_NONE) {
        LOG_ERR("USB_2 mst bus clk enable failed!");
        return -PB_ERR;
    }

    static const struct imx_cdns3_udc_config usb_cfg = {
        .base = IMX8QXP_USB3_CORE_BASE,
        .non_core_base = IMX8QXP_USB3_NON_CORE_BASE,
        .phy_base = IMX8QXP_USB3_PHY_BASE,
    };

    return imx_cdns3_udc_init(&usb_cfg);
}
#endif

#ifdef CONFIG_IMX8X_CM_USB_OTG2
/* Command mode's USB (platform.md 3.10): the i.MX8DXL's USB OTG2, the second
 * ChipIdea controller with its PHY, SC_R_USB_1 */
static int cm_usb_init(void)
{
    /* Enable usb stuff */
    sc_pm_set_resource_power_mode(plat.ipc, SC_R_USB_1, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(plat.ipc, SC_R_USB_1_PHY, SC_PM_PW_MODE_ON);

    imx_usb2_phy_init(IMX8DXL_USB2_PHY2_BASE);

    return imx_ci_udc_init(IMX8DXL_USB_OTG2_BASE);
}
#endif

#if defined(CONFIG_IMX8X_CM_USB3) || defined(CONFIG_IMX8X_CM_USB_OTG2)
int plat_cm_init(void)
{
    /* Only read over USB, so not on the boot path (D-023); the SECO keeps
     * its events until the next reset */
    if (imx8x_seco_log_init(plat.ipc) != PB_OK)
        LOG_ERR("No room for the SECO log in the logs");

    return cm_usb_init();
}
#endif

/* By the SCFW's reason (plat/imx8x/imx8x.h) */
static const struct {
    enum pb_boot_reason_kind kind;
    const char *name;
} boot_reasons[] = {
    [SC_PM_RESET_REASON_POR] = { PB_BOOT_REASON_POR, "POR" },
    [SC_PM_RESET_REASON_JTAG] = { PB_BOOT_REASON_DEBUG, "JTAG" },
    [SC_PM_RESET_REASON_SW] = { PB_BOOT_REASON_SW, "SW" },
    [SC_PM_RESET_REASON_WDOG] = { PB_BOOT_REASON_WDOG, "WDOG" },
    [SC_PM_RESET_REASON_LOCKUP] = { PB_BOOT_REASON_FAULT, "LOCKUP" },
    [SC_PM_RESET_REASON_SNVS] = { PB_BOOT_REASON_FAULT, "SNVS" },
    [SC_PM_RESET_REASON_TEMP] = { PB_BOOT_REASON_FAULT, "TEMP" },
    [SC_PM_RESET_REASON_MSI] = { PB_BOOT_REASON_UNKNOWN, "MSI" },
    [SC_PM_RESET_REASON_UECC] = { PB_BOOT_REASON_FAULT, "UECC" },
    [SC_PM_RESET_REASON_SCFW_WDOG] = { PB_BOOT_REASON_WDOG, "SCFW WDOG" },
    [SC_PM_RESET_REASON_ROM_WDOG] = { PB_BOOT_REASON_WDOG, "ROM WDOG" },
    [SC_PM_RESET_REASON_SECO] = { PB_BOOT_REASON_FAULT, "SECO" },
    [SC_PM_RESET_REASON_SCFW_FAULT] = { PB_BOOT_REASON_FAULT, "SCFW Fault" },
    [SC_PM_RESET_REASON_V2X_DEBUG] = { PB_BOOT_REASON_UNKNOWN, "V2X DEBUG" },
    [SC_PM_RESET_REASON_BOOT_STAGE] = { PB_BOOT_REASON_UNKNOWN, "BOOT STAGE" },
};

int plat_get_boot_reason(struct pb_boot_reason *reason)
{
    reason->kind = PB_BOOT_REASON_UNKNOWN;
    reason->raw = 0;
    reason->name = "Unknown";

    if (boot_reason < 0)
        return -PB_ERR_IO;

    reason->raw = (uint32_t)boot_reason;

    if ((size_t)boot_reason < ARRAY_SIZE(boot_reasons)) {
        reason->kind = boot_reasons[boot_reason].kind;
        reason->name = boot_reasons[boot_reason].name;
    }

    return PB_OK;
}

int plat_get_boot_source(struct pb_boot_source *src)
{
    (void)src;
    return -PB_ERR_NOT_SUPPORTED;
}

int plat_lockdown(void)
{
    /* Nothing yet: what the SECO can lock is unchecked (F-244) */
    return PB_OK;
}
