/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/crypto/imx_caam.h>
#include <drivers/crypto/mbedtls.h>
#include <drivers/fuse/imx_hab_fuses.h>
#include <drivers/fuse/imx_ocotp.h>
#include <drivers/mmc/mmc_core.h>
#include <drivers/rom_log/imx_hab_log.h>
#include <drivers/rom_log/imx_rom_log.h>
#include <drivers/timer/imx_gpt.h>
#include <drivers/tzc/tzc380.h>
#include <drivers/usb/imx_ci_udc.h>
#include <drivers/usb/imx_usb2_phy.h>
#include <drivers/wdog/imx_wdog.h>
#include <pb/console.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <plat/imx6ul/imx6ul.h>
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

/* USB OTG1 device address, see imx_ci_udc.c */
#define USB_DEVICEADDR      0x154
#define USB_DEVICEADDR_ADDR GENMASK_32(31, 25)

/* The ROM's serial downloader was given an address by a USB host */
static bool rom_usb_addressed;

static void save_rom_usb_state(void)
{
    uint32_t addr;

    /* Accessing a peripheral with its clock gated hangs the bus */
    if ((mmio_read_32(IMX6UL_CCM_CCGR6) & CCM_CCGR6_USBOH3) == 0)
        return;

    addr = mmio_read_32(IMX6UL_USB1_BASE + USB_DEVICEADDR);

    rom_usb_addressed = (addr & USB_DEVICEADDR_ADDR) != 0;
}

/* b36693cd-d32e-4cd5-b2bb-91406ed68840 */
const char *platform_ns_uuid = "\xb3\x66\x93\xcd\xd3\x2e\x4c\xd5\xb2\xbb\x91\x40\x6e\xd6\x88\x40";

static struct imx6ul_platform plat;

const struct imx6ul_platform *imx6ul_plat(void)
{
    return &plat;
}

static const mmap_region_t imx_mmap[] = {
    /* Boot ROM API*/
    MAP_REGION_FLAT(IMX6UL_ROM_BASE, (128 * 1024), MT_MEMORY | MT_RO | MT_EXECUTE),
    /* Needed for HAB*/
    MAP_REGION_FLAT(IMX6UL_OCRAM_BASE, (256 * 1024), MT_MEMORY | MT_RW),
    /* AIPS-1 */
    MAP_REGION_FLAT(AIPS1_ARB_BASE_ADDR, (1024 * 1024), MT_DEVICE | MT_RW),
    /* AIPS-2 */
    MAP_REGION_FLAT(AIPS2_ARB_BASE_ADDR, (1024 * 1024), MT_DEVICE | MT_RW),
    { 0 }
};

static uint32_t
fuse_read_helper(uint8_t bank, uint8_t row, uint8_t shift, uint32_t mask, uint32_t default_value)
{
    int rc;
    uint32_t fuse_data;

    rc = imx_ocotp_read(bank, row, &fuse_data);

    if (rc != 0) {
        LOG_ERR("Could not read fuse %i (%i)", row, rc);
        return default_value;
    }

    fuse_data >>= shift;
    return (fuse_data & mask);
}

/* SRC_SRSR as plat_early_init() read it */
static uint32_t srsr;

/* What plat_early_init() and plat_init() recorded (platform.md 3.8). The
 * TZASC counts as bypassed until tzasc_init() has shown that it isn't */
static struct plat_status status = {
    .tz_dram_filter = PLAT_TZ_DRAM_BYPASSED,
};

void plat_get_status(struct plat_status *out)
{
    *out = status;
}

/* The ROM's event log was saved: a dump into no room fails with
 * -PB_ERR_NOT_FOUND only when there is no log, which it logs */
static bool rom_log_found(void)
{
    char none;
    size_t length = 0;

    return imx_rom_log_dump(&none, &length) != -PB_ERR_NOT_FOUND;
}

/* The causes, the highest priority first (plat/imx6ul/imx6ul.h) */
static const struct {
    uint32_t bit;
    enum pb_boot_reason_kind kind;
    const char *name;
} boot_reasons[] = {
    { IMX6UL_SRSR_TEMPSENSE, PB_BOOT_REASON_FAULT, "temp" },
    { IMX6UL_SRSR_WDOG3, PB_BOOT_REASON_WDOG, "wdog3" },
    { IMX6UL_SRSR_JTAG_SW, PB_BOOT_REASON_DEBUG, "jtag_sw" },
    { IMX6UL_SRSR_JTAG, PB_BOOT_REASON_DEBUG, "jtag_rst" },
    { IMX6UL_SRSR_WDOG, PB_BOOT_REASON_WDOG, "wdog" },
    { IMX6UL_SRSR_USER, PB_BOOT_REASON_UNKNOWN, "User" },
    { IMX6UL_SRSR_CSU, PB_BOOT_REASON_FAULT, "CSU" },
    { IMX6UL_SRSR_POR, PB_BOOT_REASON_POR, "POR" },
};

int plat_get_boot_reason(struct pb_boot_reason *reason)
{
    reason->kind = PB_BOOT_REASON_UNKNOWN;
    reason->raw = srsr;
    reason->name = "Unknown";

    for (size_t i = 0; i < ARRAY_SIZE(boot_reasons); i++) {
        if (srsr & boot_reasons[i].bit) {
            reason->kind = boot_reasons[i].kind;
            reason->name = boot_reasons[i].name;
            break;
        }
    }

    return PB_OK;
}

int plat_get_unique_id(uint8_t *output, size_t *length)
{
    union {
        uint32_t uid[2];
        uint8_t uid_bytes[8];
    } plat_unique;

    int rc;

    if (*length < sizeof(plat_unique.uid_bytes))
        return -PB_ERR_BUF_TOO_SMALL;

    /* A read that fails writes nothing: the device's UUID is made of this */
    rc = imx_ocotp_read(IMX6UL_FUSE_UNIQUE_BANK, IMX6UL_FUSE_UNIQUE1_WORD, &plat_unique.uid[0]);

    if (rc != PB_OK)
        return rc;

    rc = imx_ocotp_read(IMX6UL_FUSE_UNIQUE_BANK, IMX6UL_FUSE_UNIQUE2_WORD, &plat_unique.uid[1]);

    if (rc != PB_OK)
        return rc;

    memcpy(output, plat_unique.uid_bytes, sizeof(plat_unique.uid_bytes));
    *length = sizeof(plat_unique.uid_bytes);
    return PB_OK;
}

void plat_reset(void)
{
    imx_wdog_reset_now(IMX6UL_WDOG1_BASE);
}

uint32_t plat_get_us_tick(void)
{
    return imx_gpt_get_tick();
}

void plat_wdog_kick(void)
{
    imx_wdog_kick(IMX6UL_WDOG1_BASE);
}

static int plat_mmu_init(void)
{
    /* Configure MMU */
    reset_xlat_tables();

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

    enable_mmu_svc_mon(0);

    return PB_OK;
}

/**
 * The TZASC checks no DDR access until TZASC1_BYP in GPR9 is set. The bit can
 * only be set while no DDR access is in flight, so the board's DCD sets it
 * after the MMDC setup.
 *
 * Out of bypass, region 0 only lets secure accesses through, but OP-TEE reads
 * the DTB through a non-secure mapping before its TZASC driver has run. Open
 * DDR to both worlds in region 1 until OP-TEE replaces it.
 */
static int tzasc_init(void)
{
    int rc;

    /*
     * Only a power-on reset clears the bit, so this misses a DCD without it
     * after a warm reset from an image that set it.
     */
    if ((mmio_read_32(IMX6UL_IOMUXC_GPR9) & IOMUXC_GPR9_TZASC1_BYP) == 0) {
        LOG_ERR("TZASC is bypassed, the DCD must set TZASC1_BYP in GPR9");
        return -PB_ERR_STATE;
    }

    rc = tzc380_init(IMX6UL_TZASC1_BASE);

    if (rc != PB_OK)
        return rc;

    return tzc380_configure_region(1, IMX6UL_DDR_BASE, IMX6UL_DDR_SIZE, TZC380_SP_ALL);
}

/* The ROM log, AN12853 Tables 11 and 13 */
static const struct imx_rom_log_config rom_log_cfg = {
    .pointer = IMX6UL_ROM_LOG_POINTER,
    .ram_start = IMX6UL_OCRAM_BASE,
    .ram_end = IMX6UL_OCRAM_BASE + IMX6UL_OCRAM_SIZE,
    .words = 64,
    .defs = IMX_ROM_LOG_DEFS_V0,
};

/* The HAB API */
static const struct imx_hab_log_config hab_log_cfg = {
    .rvt = HAB_RVT_BASE,
    .entry_size = 4,
};

/* RM Table 5-9, OCOTP_SRK_REVOKE (35.5.58): SRK_REVOKE[2:0] */
static const struct imx_hab_fuses hab_fuses = {
    .sec_config_bank = IMX6UL_FUSE_SEC_CONFIG_BANK,
    .sec_config_word = IMX6UL_FUSE_SEC_CONFIG_WORD,
    .closed = IMX6UL_FUSE_SEC_CONFIG_CLOSED,
    .srk_bank = IMX6UL_FUSE_SRK_BANK,
    .srk_word = IMX6UL_FUSE_SRK_WORD,
    .revoke_bank = IMX6UL_FUSE_REVOKE_BANK,
    .revoke_word = IMX6UL_FUSE_REVOKE_WORD,
    .revocable = IMX6UL_FUSE_REVOCABLE_SRKS,
    .revoke_allowed = hab_has_no_errors,
};

/* The root of trust and the life cycle (platform.md 4.1, 4.2): HABv4's
 * state in the OCOTP. No EOL (F-263) */
const struct rot_config plat_rot = {
    .revoke_key = imx_hab_revoke_key,
    .read_key_status = imx_hab_read_key_status,
};

const struct slc_ops plat_slc = {
    .read_status = imx_hab_slc_read_status,
    .set_configuration = imx_hab_slc_set_configuration,
    .set_configuration_locked = imx_hab_slc_set_configuration_locked,
    .fuse_read = imx_ocotp_fuse_read,
    .fuse_write = imx_ocotp_fuse_write,
};

/*
 * The build's crypto backends (platform.md 2.2, crypto.md 2), each behind
 * its driver's option: the CAAM on job ring 0 with its RNG, and Mbed TLS. A
 * board that needs the software implementation turns the CAAM's option off
 * and Mbed TLS's on in its defconfig.
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
    rc = imx_caam_init(IMX6UL_CAAM_JR0_BASE);
    backends++;

    if (rc != PB_OK) {
        LOG_ERR("CAAM init failed (%i)", rc);
        failed = true;
    } else {
        /* Linux can't instantiate the RNG when it runs in the normal world
         * (platform.md 4.5). Without it Linux has no hardware RNG, which is
         * no reason not to boot */
        rc = imx_caam_rng_init(IMX6UL_CAAM_BASE);

        if (rc != PB_OK)
            LOG_ERR("CAAM RNG init failed (%i)", rc);
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

int plat_early_init(void)
{
    /* The reset reason. Read only: the OS reads the same register */
    srsr = mmio_read_32(IMX6UL_SRC_SRSR);

    /* Both of the ROM's logs, or it is recorded: logged, nothing of the
     * boot's rests on them (platform.md 2.3) */
    imx_rom_log_save(&rom_log_cfg);
    status.rom_log = rom_log_found();

    if (imx_hab_log_init(&hab_log_cfg) != PB_OK)
        status.rom_log = false;

    /* Before the USB driver resets the controller */
    if (imx_rom_log_has_event(IMX_ROM_LOG_V0_SERIAL_DOWNLOAD))
        save_rom_usb_state();

    /* The WDOGs' power-down counters, whether the watchdog is used or not */
    mmio_clrsetbits_32(IMX6UL_CCM_CCGR3, 0, CCM_CCGR3_WDOG1);
    mmio_clrsetbits_32(IMX6UL_CCM_CCGR5, 0, CCM_CCGR5_WDOG2);
    mmio_clrsetbits_32(IMX6UL_CCM_CCGR6, 0, CCM_CCGR6_WDOG3);
    imx_wdog_disable_powerdown(IMX6UL_WDOG1_BASE);
    imx_wdog_disable_powerdown(IMX6UL_WDOG2_BASE);
    imx_wdog_disable_powerdown(IMX6UL_WDOG3_BASE);

    /* Unmask wdog in SRC control reg */
    mmio_clrsetbits_32(IMX6UL_SRC_SCR,
                       SRC_SCR_MASK_WDOG_RST_MASK | SRC_SCR_WARM_RESET_ENABLE,
                       SRC_SCR_MASK_WDOG_RST(10));

    status.wdog_armed = false;
#if CONFIG_ENABLE_WATCHDOG
    /* Enable WDT */
    mmio_clrsetbits_32(IMX6UL_CCM_CCGR3, 0, CCM_CCGR3_WDOG1);
    status.wdog_armed = (imx_wdog_init(IMX6UL_WDOG1_BASE, CONFIG_WATCHDOG_TIMEOUT) == PB_OK);
#endif

    /* Configure systick. The ROM leaves perclk on IPG (66 MHz on jiffy), use the crystal */
    imx_gpt_init_24m(IMX6UL_GPT1_BASE);
    log_time_start();
    return PB_OK;
}

int plat_init(void)
{
    int rc;

    /* Init fusebox */
    imx_ocotp_init(IMX6UL_OCOTP_CTRL_BASE, IMX6UL_FUSE_WORDS_PER_BANK, MHz(66)); /* ipg_clk */
    imx_hab_fuses_init(&hab_fuses);

    plat.si_rev = fuse_read_helper(
        0, IMX6UL_FUSE_SI_REV_WORD, IMX6UL_FUSE_SI_REV_SHIFT, IMX6UL_FUSE_SI_REV_MASK, 0);
    plat.speed_grade = fuse_read_helper(0,
                                        IMX6UL_FUSE_SPEED_GRADE_WORD,
                                        IMX6UL_FUSE_SPEED_GRADE_SHIFT,
                                        IMX6UL_FUSE_SPEED_GRADE_MASK,
                                        1);

    /* Configure ARM core clock
     *
     * Speed grade:
     * 1 - 528 MHz
     * 2 - 696 MHz
     */

    /* Select step clock, so we can change arm PLL */
    mmio_clrsetbits_32(IMX6UL_CCM_CCSR, 0, CCM_CCSR_PLL1_SW_CLK_SEL);

    /* Power down main PLL */
    mmio_write_32(IMX6UL_CCM_ANALOG_PLL_ARM, CCM_ANALOG_PLL_POWERDOWN);

    /* Configure divider and enable:
     * Speedgrade 1: f_CPU = 24 MHz * 88 / 4  = 528 MHz
     * Speedgrade 2: f_CPU = 24 MHz * 116 / 4 = 696 MHz
     */

    if (plat.speed_grade == 1) {
        mmio_write_32(IMX6UL_CCM_ANALOG_PLL_ARM,
                      CCM_ANALOG_PLL_ENABLE | CCM_ANALOG_PLL_ARM_DIV_SELECT(88));
    } else if (plat.speed_grade == 2) {
        mmio_write_32(IMX6UL_CCM_ANALOG_PLL_ARM,
                      CCM_ANALOG_PLL_ENABLE | CCM_ANALOG_PLL_ARM_DIV_SELECT(116));
    } else {
        LOG_WARN("Unknown speed grade (%u), setting core to 528 MHz", plat.speed_grade);
        mmio_write_32(IMX6UL_CCM_ANALOG_PLL_ARM,
                      CCM_ANALOG_PLL_ENABLE | CCM_ANALOG_PLL_ARM_DIV_SELECT(88));
    }

    /* Wait for PLL to lock */
    while (!(mmio_read_32(IMX6UL_CCM_ANALOG_PLL_ARM) & CCM_ANALOG_PLL_ARM_LOCK))
        ;

    /* Select re-connect ARM PLL */
    mmio_clrsetbits_32(IMX6UL_CCM_CCSR, CCM_CCSR_PLL1_SW_CLK_SEL, 0);

    /* Compute some of the core clocks */
    uint32_t cacrr = mmio_read_32(IMX6UL_CCM_CACRR);
    uint32_t cscmr1 = mmio_read_32(IMX6UL_CCM_CSCMR1);
    uint32_t cscdr1 = mmio_read_32(IMX6UL_CCM_CSCDR1);

    /* Core clock */
    unsigned int arm_podf = (cacrr & CCM_CACRR_AR_PODF_MASK) + 1;
    uint32_t pll1_reg = mmio_read_32(IMX6UL_CCM_ANALOG_PLL_ARM);
    plat.core_clk_MHz = (24 * (pll1_reg & CCM_ANALOG_PLL_ARM_DIV_SELECT_MASK) / arm_podf) / 2;

    /* USDHC clocks */
    unsigned int usdhc1_podf =
        ((cscdr1 & CCM_CSCDR1_USDHC1_PODF_MASK) >> CCM_CSCDR1_USDHC1_PODF_SHIFT) + 1;
    unsigned int usdhc2_podf =
        ((cscdr1 & CCM_CSCDR1_USDHC2_PODF_MASK) >> CCM_CSCDR1_USDHC2_PODF_SHIFT) + 1;
    plat.usdhc1_clk_MHz = ((cscmr1 & CCM_CSCMR1_USDHC1_CLK_SEL) ? 352 : 400) / usdhc1_podf;
    plat.usdhc2_clk_MHz = ((cscmr1 & CCM_CSCMR1_USDHC2_CLK_SEL) ? 352 : 400) / usdhc2_podf;

    LOG_INFO("uSDHC1 clk: %u MHz", plat.usdhc1_clk_MHz);
    LOG_INFO("uSDHC2 clk: %u MHz", plat.usdhc2_clk_MHz);

    imx_wdog_kick(IMX6UL_WDOG1_BASE);
    plat_mmu_init();

    /* Not plat_init()'s error: command mode needs no TZASC, and main() would
     * reset (platform.md 2.2). It is recorded, and no boot goes on without
     * it (platform.md 2.3, F-241) */
    rc = tzasc_init();

    if (rc != PB_OK)
        LOG_ERR("TZASC setup failed (%i)", rc);

    status.tz_dram_filter = (rc == PB_OK) ? PLAT_TZ_DRAM_ACTIVE : PLAT_TZ_DRAM_BYPASSED;

    /* Last: until F-247 the board's init() did it, as its first step */
    status.crypto_engine = crypto_init();

    return PB_OK;
}

#ifdef CONFIG_IMX6UL_CM_USB_OTG1
/* Command mode's USB (platform.md 3.10): USB OTG1, with USBPHY1 and the
 * clocks of PLL_USB1 */
int plat_cm_init(void)
{
    /* Enable USB PLL */
    mmio_clrsetbits_32(IMX6UL_CCM_ANALOG_PLL_USB1, 0, CCM_ANALOG_PLL_USB1_EN_USB_CLKS);

    imx_usb2_phy_init(IMX6UL_USBPHY1_BASE);
    return imx_ci_udc_init(IMX6UL_USB1_BASE);
}
#endif

int plat_get_boot_source(struct pb_boot_source *src)
{
    memset(src, 0, sizeof(*src));
    src->kind = PB_BOOT_SOURCE_UNKNOWN;

    /*
     * The ROM logs which of its two jumps it took (NXP AN12853, Table 7).
     * The serial downloader listens on both UART and USB OTG1, and the log
     * doesn't say which one the image came over. The ROM leaves the USB
     * device address set when a host enumerated it, as uuu does: USB. With
     * no address the image can only have come over the UART. A UART
     * download with a USB host connected is reported as USB; either way it
     * is a serial download, which is what the core's stop asks (board.md
     * 5.1).
     */
    if (imx_rom_log_has_event(IMX_ROM_LOG_V0_JUMP_SDP)) {
        src->kind = rom_usb_addressed ? PB_BOOT_SOURCE_USB : PB_BOOT_SOURCE_UART;
        return PB_OK;
    }

#ifdef CONFIG_MMC_CORE
    /*
     * The ROM loads the primary image from the partition in
     * BOOT_PARTITION_ENABLE and has no fallback between the boot partitions
     * (RM 8.5.3.2), so after an SD/eMMC boot that's the one we came from.
     * This assumes the boot device is the eMMC that mmc_core drives. After a
     * failed device init or read the ROM may have tried a recovery device,
     * so that is reported as unknown.
     */
    if (imx_rom_log_has_event(IMX_ROM_LOG_V0_JUMP) &&
        imx_rom_log_has_event(IMX_ROM_LOG_V0_PRIMARY_IMAGE) &&
        imx_rom_log_has_event(IMX_ROM_LOG_V0_PRIMARY_SD_MMC) &&
        !imx_rom_log_has_event(IMX_ROM_LOG_V0_DEVICE_INIT_FAIL) &&
        !imx_rom_log_has_event(IMX_ROM_LOG_V0_DEVICE_READ_FAIL) &&
        mmc_get_boot_part_config(src->part_uu) == PB_OK) {
        src->kind = PB_BOOT_SOURCE_STORAGE;
    }
#endif

    return PB_OK;
}

int plat_lockdown(void)
{
    int rc;

    /*
     * The CSF unlocks SRK revocation for pb's command mode (pb.csf.template),
     * so lock it again before the next stage runs (RM 35.5.6). SCS holds
     * HAB_JDE, which enables JTAG, and the RM (35.5.7) asks for it to be
     * locked before the OS runs. On a closed jiffy HAB has locked field
     * return and SCS already; locking them again costs nothing. The locks
     * hold until the next reset: a watchdog reset clears them too, so pb's
     * command mode can revoke keys after a reboot.
     */
    rc = imx_ocotp_lock(OCOTP_SW_STICKY_SRK_REVOKE_LOCK | OCOTP_SW_STICKY_FIELD_RETURN_LOCK, true);

    if (rc != PB_OK)
        return rc;

    /* Last, and only when the locks are set: a lockdown that fails ends in
     * command mode, where a locked device verifies its token with the CAAM */
    return crypto_stop();
}

/*
 * The boot configuration the ROM used (RM 8.2, Table 8-15 for uSDHC): SRC
 * SBMR1 is BOOT_CFG1..4 from the fuses or the boot pins, the BOOT_CFG fuse
 * word is the shadow at 0x450. Only read.
 */
#define SRC_SBMR1       (IMX6UL_SRC_BASE + 0x04)
#define SRC_SBMR2       (IMX6UL_SRC_BASE + 0x1c)
#define OCOTP_BOOT_CFG0 (IMX6UL_OCOTP_CTRL_BASE + 0x450)

static const char *boot_bus_width_str(uint32_t cfg)
{
    switch (IMX6UL_BOOT_CFG_BUS_WIDTH_OF(cfg)) {
    case IMX6UL_BOOT_BUS_1BIT:
        return "1-bit";
    case IMX6UL_BOOT_BUS_4BIT:
        return "4-bit";
    case IMX6UL_BOOT_BUS_8BIT:
        return "8-bit";
    case IMX6UL_BOOT_BUS_4BIT_DDR:
        return "4-bit DDR";
    case IMX6UL_BOOT_BUS_8BIT_DDR:
        return "8-bit DDR";
    default:
        return "reserved";
    }
}

/* Add a line to the specifics when it fits whole (platform.md 3.11) */
static void put_line(char *extra, size_t size, const char *line)
{
    size_t len = strlen(extra);

    if (len + strlen(line) < size)
        memcpy(&extra[len], line, strlen(line) + 1);
}

/* arm_clk_root: PLL1 = 24 MHz * DIV_SELECT / 2, then ARM_PODF (RM 18.7.1,
 * 18.6.4), when PLL1 isn't bypassed and the core isn't on the step clock */
static uint32_t cpu_hz(void)
{
    uint32_t pll = mmio_read_32(IMX6UL_CCM_ANALOG_PLL_ARM);
    uint32_t podf = (mmio_read_32(IMX6UL_CCM_CACRR) & CCM_CACRR_AR_PODF_MASK) + 1;

    if ((mmio_read_32(IMX6UL_CCM_CCSR) & CCM_CCSR_PLL1_SW_CLK_SEL) ||
        (pll & CCM_ANALOG_PLL_ARM_BYPASS))
        return 0;

    /* At most 24 MHz * 127: 32 bits hold it */
    return 24000000u * (pll & CCM_ANALOG_PLL_ARM_DIV_SELECT_MASK) / (2 * podf);
}

/* mmdc_clk_root (RM 18.6.5, 18.6.6, the clock tree in 18.3): periph2, from
 * PLL2 or one of its PFDs, or from periph2_clk2; then FABRIC_MMDC_PODF. The
 * ROM sets it (396 MHz, PLL2's PFD2, Table 8-43) and jiffy's DCD keeps it */
static uint32_t mmdc_hz(void)
{
    uint32_t cbcdr = mmio_read_32(IMX6UL_CCM_CBCDR);
    uint32_t cbcmr = mmio_read_32(IMX6UL_CCM_CBCMR);
    uint32_t pll2 = (mmio_read_32(IMX6UL_CCM_ANALOG_PLL_SYS) & CCM_ANALOG_PLL_SYS_DIV_SELECT)
                        ? 528000000
                        : 480000000;
    uint32_t pfd = mmio_read_32(IMX6UL_CCM_ANALOG_PFD_528);
    uint32_t frac;
    uint32_t src;

    if (cbcdr & CCM_CBCDR_PERIPH2_CLK_SEL) {
        src = (cbcmr & CCM_CBCMR_PERIPH2_CLK2_SEL) ? 24000000 : 480000000;
        src /= CCM_CBCDR_PERIPH2_CLK2_PODF_OF(cbcdr) + 1;
    } else {
        switch (CCM_CBCMR_PRE_PERIPH2_CLK_SEL_OF(cbcmr)) {
        case 0: /* PLL2 */
            src = pll2;
            break;
        case 1: /* PLL2's PFD2 */
        case 2: /* PLL2's PFD0 */
            frac =
                CCM_ANALOG_PFD_FRAC_OF(pfd, CCM_CBCMR_PRE_PERIPH2_CLK_SEL_OF(cbcmr) == 1 ? 2 : 0);
            if (frac < 12)
                return 0;
            /* In kHz: 528 MHz * 18 doesn't fit in 32 bits */
            src = 528000u * 18 / frac * 1000;
            break;
        default: /* PLL4, the audio PLL: not read */
            return 0;
        }
    }

    return src / (CCM_CBCDR_FABRIC_MMDC_PODF_OF(cbcdr) + 1);
}

int plat_get_info(struct pb_plat_info *info, char *extra, size_t extra_size)
{
    static char rev[8];
    uint32_t digprog = mmio_read_32(IMX6UL_USB_ANALOG_DIGPROG);
    uint32_t sbmr1 = mmio_read_32(SRC_SBMR1);
    char line[96];

    if (extra_size == 0)
        return -PB_ERR_BUF_TOO_SMALL;
    extra[0] = '\0';

    /* 0x64 is the 6UL (RM 55.4.11); the 6ULL and 6ULZ have 0x65, as Linux
     * and U-Boot read it, and share this port */
    switch (DIGPROG_MAJOR_UPPER_OF(digprog)) {
    case 0x64:
        info->soc = "i.MX6UL";
        break;
    case 0x65:
        info->soc = "i.MX6ULL";
        break;
    default:
        info->soc = "unknown";
        break;
    }

    (void)snprintf(rev,
                   sizeof(rev),
                   "%u.%u",
                   (unsigned int)DIGPROG_MAJOR_LOWER_OF(digprog) + 1,
                   (unsigned int)DIGPROG_MINOR_OF(digprog));
    info->rev = rev;
    info->cpu_hz = cpu_hz();
    info->mem_hz = mmdc_hz();

    (void)snprintf(line, sizeof(line), "Speed grade %u\n", (unsigned int)plat.speed_grade);
    put_line(extra, extra_size, line);

    (void)snprintf(line,
                   sizeof(line),
                   "Boot config: SBMR1 0x%08x, SBMR2 0x%08x, fuses 0x450 0x%08x\n",
                   (unsigned int)sbmr1,
                   (unsigned int)mmio_read_32(SRC_SBMR2),
                   (unsigned int)mmio_read_32(OCOTP_BOOT_CFG0));
    put_line(extra, extra_size, line);

    (void)snprintf(line,
                   sizeof(line),
                   "ROM eMMC boot: %s, %s, %s speed, %s boot\n",
                   (sbmr1 & IMX6UL_BOOT_CFG_MMC) ? "MMC" : "SD",
                   boot_bus_width_str(sbmr1),
                   (sbmr1 & IMX6UL_BOOT_CFG_MMC_HIGH_SPEED) ? "high" : "normal",
                   (sbmr1 & IMX6UL_BOOT_CFG_FAST_BOOT) ? "fast" : "normal");
    put_line(extra, extra_size, line);

    return PB_OK;
}
