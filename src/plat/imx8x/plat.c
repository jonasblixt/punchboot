/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/crypto/imx_caam.h>
#include <drivers/crypto/mbedtls.h>
#include <drivers/timer/imx_gpt.h>
#include <drivers/uart/imx_lpuart.h>
#include <drivers/usb/imx_cdns3_udc.h>
#include <drivers/usb/imx_ci_udc.h>
#include <drivers/usb/imx_usb2_phy.h>
#include <pb/console.h>
#include <pb/crypto.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/utils_def.h>
#include <plat/imx8x/fusebox.h>
#include <plat/imx8x/imx8x.h>
#include <plat/imx8x/sci/rpc.h>
#include <plat/imx8x/sci/svc/misc/api.h>
#include <plat/imx8x/sci/svc/pm/api.h>
#include <plat/imx8x/sci/svc/seco/api.h>
#include <plat/imx8x/sci/svc/timer/api.h>
#include <plat/imx8x/seco_log.h>
#include <stdio.h>
#include <string.h>
#include <uuid.h>
#include <xlat_tables.h>

#include "sci/svc/misc/rpc.h"

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

    sc_rpc_msg_t msg;

    if (*length < sizeof(plat_unique.uid_bytes))
        return -PB_ERR_BUF_TOO_SMALL;

    /*
     * sc_misc_unique_id()'s message, which the SCFW API answers without an
     * error (F-416): the function is void, and the answer's error byte isn't
     * one (Linux's SCU driver ignores it for this function). What can be
     * checked is the answer's size: the header and the id's two words. One
     * that is shorter didn't carry the id, and its words are the request's.
     * The id's bytes and their order are sc_misc_unique_id()'s, the
     * identity's (platform.md 3.5, 3.7).
     *
     * An id of zero or of all ones isn't refused: the SCFW reads it from
     * the fuses NXP programs, and nothing says either can't be one. Refusing
     * it would leave such a device without command mode (cm_setup() fails
     * without an identity). Not tried on a board.
     */
    memset(&msg, 0, sizeof(msg));
    RPC_VER(&msg) = SC_RPC_VERSION;
    RPC_SIZE(&msg) = 1U;
    RPC_SVC(&msg) = (uint8_t)SC_RPC_SVC_MISC;
    RPC_FUNC(&msg) = (uint8_t)MISC_FUNC_UNIQUE_ID;
    sc_call_rpc(plat.ipc, &msg, SC_FALSE);

    if (RPC_SIZE(&msg) < 3U) {
        LOG_ERR("Unique id: an answer of %u words", (unsigned int)RPC_SIZE(&msg));
        return -PB_ERR_IO;
    }

    plat_unique.uid[0] = RPC_U32(&msg, 0U);
    plat_unique.uid[1] = RPC_U32(&msg, 4U);
    *length = sizeof(plat_unique.uid_bytes);
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

/* Add a line to the specifics when it fits whole (platform.md 3.11) */
static void put_line(char *extra, size_t size, const char *line)
{
    size_t len = strlen(extra);

    if (len + strlen(line) < size)
        memcpy(&extra[len], line, strlen(line) + 1);
}

/* SC_C_ID's SoC ids, as U-Boot names them (arch/arm/include/asm/arch-imx/cpu.h) */
static const char *soc_name(uint32_t soc_id)
{
    switch (soc_id) {
    case 0x1:
        return "i.MX8QM";
    case 0x2:
        return "i.MX8QXP";
    case 0xe:
        return "i.MX8DXL";
    default:
        return "unknown";
    }
}

int plat_get_info(struct pb_plat_info *info, char *extra, size_t extra_size)
{
    static char rev[3];
    uint32_t scfw_build, scfw_commit;
    uint32_t seco_version, seco_commit;
    sc_pm_clock_rate_t rate;
    int16_t celsius;
    int8_t tenths;
    char line[64];

    if (extra_size == 0)
        return -PB_ERR_BUF_TOO_SMALL;
    extra[0] = '\0';

    /* SC_C_ID's revision: 0 is A0, 1 B0, 2 C0 */
    info->soc = soc_name(plat.soc_id);
    if (plat.soc_id != 0 && plat.soc_rev < 26) {
        rev[0] = (char)('A' + plat.soc_rev);
        rev[1] = '0';
        rev[2] = '\0';
        info->rev = rev;
    } else {
        info->rev = "unknown";
    }

    info->cpu_hz = 0;
    if (sc_pm_get_clock_rate(plat.ipc, SC_R_A35, SC_PM_CLK_CPU, &rate) == SC_ERR_NONE)
        info->cpu_hz = rate;

    /* The DRC's root clock (SCFW API, "Clock List": SC_R_DRC_0's
     * SC_PM_CLK_SLV_BUS, 600 MHz by default) is the DFI clock, half the
     * DRAM's: the MEK's LPDDR4 runs at 1.2 GHz */
    info->mem_hz = 0;
    if (sc_pm_get_clock_rate(plat.ipc, SC_R_DRC_0, SC_PM_CLK_SLV_BUS, &rate) == SC_ERR_NONE)
        info->mem_hz = rate * 2;

    sc_misc_build_info(plat.ipc, &scfw_build, &scfw_commit);
    (void)snprintf(
        line, sizeof(line), "SCFW %u (%x)\n", (unsigned int)scfw_build, (unsigned int)scfw_commit);
    put_line(extra, extra_size, line);

    sc_seco_build_info(plat.ipc, &seco_version, &seco_commit);
    (void)snprintf(line,
                   sizeof(line),
                   "SECO %u (%x)\n",
                   (unsigned int)seco_version,
                   (unsigned int)seco_commit);
    put_line(extra, extra_size, line);

    if (sc_misc_get_temp(plat.ipc, SC_R_SYSTEM, SC_MISC_TEMP, &celsius, &tenths) == SC_ERR_NONE) {
        /* Both parts have the sign */
        int temp = celsius * 10 + tenths;
        unsigned int abs_temp = (unsigned int)(temp < 0 ? -temp : temp);

        (void)snprintf(line,
                       sizeof(line),
                       "Temperature %s%u.%u C\n",
                       temp < 0 ? "-" : "",
                       abs_temp / 10,
                       abs_temp % 10);
    } else {
        (void)snprintf(line, sizeof(line), "Temperature not read\n");
    }
    put_line(extra, extra_size, line);

    return PB_OK;
}

/* The SCFW's answer to arming the watchdog, once plat_early_init() asked */
static sc_err_t wdog_rc;
static bool wdog_asked;

int plat_early_init(void)
{
    sc_ipc_open(&plat.ipc, SC_IPC_BASE);

    wdog_rc = imx8x_wdog_init();
    wdog_asked = true;
    imx8x_systick_setup();
    log_time_start();
    imx8x_load_boot_reason();
    return PB_OK;
}

/* Whether plat_init() set the build's crypto backends up */
static bool crypto_engine;

/*
 * What plat_early_init() and plat_init() recorded (platform.md 3.8). The
 * DRAM filter is a later stage's: BL31 partitions the memory through the
 * SCFW. The ROM's logs aren't saved.
 */
void plat_get_status(struct plat_status *status)
{
    status->tz_dram_filter = PLAT_TZ_DRAM_LATER_STAGE;
#ifdef CONFIG_ENABLE_WATCHDOG
    status->wdog_armed = wdog_asked && wdog_rc == SC_ERR_NONE;
#else
    status->wdog_armed = false;
#endif
    status->crypto_engine = crypto_engine;
    status->rom_log = false;
}

/*
 * The build's crypto backends (platform.md 2.2, crypto.md 2), each behind
 * its driver's option: the CAAM on job ring 2, which the SCFW powers (job
 * rings 0 and 1 are the SECO's), and Mbed TLS. A board that needs the
 * software implementation turns the CAAM's option off and Mbed TLS's on in
 * its defconfig. The RNG isn't pb's here (plat-imx.md 4.4).
 *
 * Command mode can start without a backend, so a failure isn't plat_init()'s
 * error: main() would reset. A backend that failed isn't registered and no
 * image verifies: it is recorded (plat_get_status()), and the boot's
 * preconditions refuse the boot before anything is loaded (platform.md 2.3).
 *
 * Returns whether the build's backends all came up. A build without one has
 * none that works.
 */
static bool crypto_init(void)
{
    unsigned int backends = 0;
    bool failed = false;
    int rc = PB_OK;

#ifdef CONFIG_DRIVERS_IMX_CAAM
    sc_pm_set_resource_power_mode(plat.ipc, SC_R_CAAM_JR2, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(plat.ipc, SC_R_CAAM_JR2_OUT, SC_PM_PW_MODE_ON);

    rc = imx_caam_init(IMX_CAAM_JR2_BASE);
    backends++;

    if (rc != PB_OK) {
        LOG_ERR("CAAM init failed (%i)", rc);
        failed = true;
    }
#endif

#ifdef CONFIG_DRIVERS_CRYPTO_MBEDTLS
    rc = mbedtls_pb_init();
    backends++;

    if (rc != PB_OK) {
        LOG_ERR("Mbed TLS init failed (%i)", rc);
        failed = true;
    }
#endif

    (void)rc;
    return backends > 0 && !failed;
}

/*
 * Before the jump the platform stops what plat_init() started (platform.md
 * 4.5, drivers.md 2.3): the CAAM's job ring holds no address in pb's memory
 * any more (crypto.md 6). Mbed TLS has nothing to stop. After it the CAAM's
 * backend answers -PB_ERR_STATE until a reset. A stop that fails is a failed
 * boot: plat_lockdown() returns its error and pb doesn't jump.
 */
static int crypto_stop(void)
{
#ifdef CONFIG_DRIVERS_IMX_CAAM
    int rc = imx_caam_stop();

    if (rc != PB_OK)
        LOG_ERR("CAAM stop failed (%i)", rc);

    return rc;
#else
    return PB_OK;
#endif
}

int plat_init(void)
{
    if (wdog_rc != 0) {
        /* Not plat_init()'s error: it is recorded (plat_get_status()), and
         * the boot goes on and reports it (platform.md 2.3) */
        LOG_ERR("Failed to enable watchdog (%i)", wdog_rc);
    }

    imx8x_mmu_init();
    LOG_DBG("MMU on");

    imx8x_fuse_init(plat.ipc);
    imx8x_rot_helpers_init(plat.ipc);
    imx8x_slc_helpers_init(plat.ipc);

    /* Last: until F-247 the board's init() did it, as its first step */
    crypto_engine = crypto_init();

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

/*
 * Where pb was loaded from (platform.md 3.9): the SCFW's boot type, which
 * it has from the ROM. Serial is the ROM's serial downloader, USB on the
 * i.MX8X. The other types are a boot from the boot device, whose partition
 * isn't named: unknown, and so is an SCFW that doesn't answer. Asked when it
 * is called, as the boards' early_boot() did: valid from plat_early_init(),
 * which opens the channel.
 */
int plat_get_boot_source(struct pb_boot_source *src)
{
    sc_misc_bt_t type = SC_MISC_BT_PRIMARY;

    memset(src, 0, sizeof(*src));
    src->kind = PB_BOOT_SOURCE_UNKNOWN;

    if (sc_misc_get_boot_type(plat.ipc, &type) != SC_ERR_NONE)
        return PB_OK;

    if (type == SC_MISC_BT_SERIAL)
        src->kind = PB_BOOT_SOURCE_USB;

    return PB_OK;
}

int plat_lockdown(void)
{
    /* Nothing locked yet: what the SECO can lock is unchecked (F-244) */
    return crypto_stop();
}
