/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/fuse/imx_ocotp.h>
#include <drivers/mmc/mmc_core.h>
#include <drivers/timer/imx_gpt.h>
#include <drivers/tzc/tzc380.h>
#include <drivers/wdog/imx_wdog.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <plat/imx6ul/imx6ul.h>
#include <plat/imx6ul/rom_log.h>
#include <string.h>
#include <uuid.h>
#include <xlat_tables.h>

#define SRSR_WARM     BIT(16)
#define SRSR_TEMP     BIT(8)
#define SRSR_WDOG3    BIT(7)
#define SRSR_JTAG_SW  BIT(6)
#define SRSR_JTAG_RST BIT(5)
#define SRSR_WDOG     BIT(4)
#define SRSR_USER     BIT(3)
#define SRSR_CSU      BIT(2)
#define SRSR_POR      BIT(0)

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

enum imx6ul_boot_reason {
    BR_UNKNOWN,
    BR_POR,
    BR_CSU,
    BR_USER,
    BR_WDOG,
    BR_JTAG_RST,
    BR_JTAG_SW,
    BR_WDOG3,
    BR_TEMP,
    BR_WARM,
    BR_END,
};

static const char *br_str[BR_END] = {
    "Unknown", "POR", "CSU", "User", "wdog", "jtag_rst", "jtag_sw", "wdog3", "temp", "warm",
};

static const mmap_region_t imx_mmap[] = {
    /* Boot ROM API*/
    MAP_REGION_FLAT(0x00000000, (128 * 1024), MT_MEMORY | MT_RO | MT_EXECUTE),
    /* Needed for HAB*/
    MAP_REGION_FLAT(0x00900000, (256 * 1024), MT_MEMORY | MT_RW),
    /* AIPS-1 */
    MAP_REGION_FLAT(0x02000000, (1024 * 1024), MT_DEVICE | MT_RW),
    /* AIPS-2 */
    MAP_REGION_FLAT(0x02100000, (1024 * 1024), MT_DEVICE | MT_RW),
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

int plat_boot_reason(void)
{
    /* Read the SRC reset status register */
    uint32_t srsr = mmio_read_32(IMX6UL_SRC_SRSR);

    if (srsr & SRSR_WARM)
        return BR_WARM;
    else if (srsr & SRSR_TEMP)
        return BR_TEMP;
    else if (srsr & SRSR_WDOG3)
        return BR_WDOG3;
    else if (srsr & SRSR_JTAG_SW)
        return BR_JTAG_SW;
    else if (srsr & SRSR_JTAG_RST)
        return BR_JTAG_RST;
    else if (srsr & SRSR_WDOG)
        return BR_WDOG;
    else if (srsr & SRSR_USER)
        return BR_USER;
    else if (srsr & SRSR_CSU)
        return BR_CSU;
    else if (srsr & SRSR_POR)
        return BR_POR;
    else
        return BR_UNKNOWN;
}

const char *plat_boot_reason_str(void)
{
    return br_str[plat_boot_reason()];
}

int plat_get_unique_id(uint8_t *output, size_t *length)
{
    union {
        uint32_t uid[2];
        uint8_t uid_bytes[8];
    } plat_unique;

    if (*length < sizeof(plat_unique.uid_bytes))
        return -PB_ERR_BUF_TOO_SMALL;
    *length = sizeof(plat_unique.uid_bytes);

    imx_ocotp_read(IMX6UL_FUSE_UNIQUE_BANK, IMX6UL_FUSE_UNIQUE1_WORD, &plat_unique.uid[0]);
    imx_ocotp_read(IMX6UL_FUSE_UNIQUE_BANK, IMX6UL_FUSE_UNIQUE2_WORD, &plat_unique.uid[1]);
    memcpy(output, plat_unique.uid_bytes, sizeof(plat_unique.uid_bytes));
    return PB_OK;
}

void plat_reset(void)
{
    imx_wdog_reset_now();
}

uint32_t plat_get_us_tick(void)
{
    return imx_gpt_get_tick();
}

void plat_wdog_kick(void)
{
    imx_wdog_kick();
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

int plat_init(void)
{
    int rc;

    imx6ul_rom_log_save();
    /* Before the USB driver resets the controller */
    if (imx6ul_rom_log_has_event(IMX6UL_ROM_LOG_SERIAL_DOWNLOAD))
        save_rom_usb_state();

    /* Unmask wdog in SRC control reg */
    mmio_clrsetbits_32(IMX6UL_SRC_SCR,
                       SRC_SCR_MASK_WDOG_RST_MASK | SRC_SCR_WARM_RESET_ENABLE,
                       SRC_SCR_MASK_WDOG_RST(10));

#if CONFIG_ENABLE_WATCHDOG
    /* Enable WDT */
    mmio_clrsetbits_32(IMX6UL_CCM_CCGR3, 0, CCM_CCGR3_WDOG1);
    imx_wdog_init(IMX6UL_WDOG1_BASE, CONFIG_WATCHDOG_TIMEOUT);
#endif

    /* Configure systick. The ROM leaves perclk on IPG (66 MHz on jiffy), use the crystal */
    imx_gpt_init_24m(IMX6UL_GPT1_BASE);

    board_console_init(&plat);

    /* Init fusebox */
    imx_ocotp_init(IMX6UL_OCOTP_CTRL_BASE, 8);

    plat.si_rev = fuse_read_helper(0, 3, 16, 0x0f, 0);
    plat.speed_grade = fuse_read_helper(0, 4, 16, 0x03, 1);

    LOG_INFO("IMX6UL: si_rev=%u, speed_grade=%u", plat.si_rev, plat.speed_grade);

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

    LOG_INFO("CPU clk: %u MHz", plat.core_clk_MHz);
    LOG_INFO("uSDHC1 clk: %u MHz", plat.usdhc1_clk_MHz);
    LOG_INFO("uSDHC2 clk: %u MHz", plat.usdhc2_clk_MHz);

    imx_wdog_kick();
    plat_mmu_init();

    /* Not fatal: without plat_board_init() command mode can't be used to recover */
    rc = tzasc_init();

    if (rc != PB_OK)
        LOG_ERR("TZASC setup failed (%i)", rc);

    return PB_OK;
}

int plat_board_init(void)
{
    return board_init(&plat);
}

int plat_get_boot_source(struct pb_boot_source *src)
{
    memset(src, 0, sizeof(*src));
    src->kind = PB_BOOT_SOURCE_UNKNOWN;

    /*
     * The ROM logs which of its two jumps it took (NXP AN12853, Table 7).
     * The serial downloader listens on both UART and USB OTG1, and the log
     * doesn't say which one the image came over. The ROM leaves the USB
     * device address set when a host enumerated it, as uuu does. A UART
     * download with a USB host connected would also be reported as USB.
     */
    if (imx6ul_rom_log_has_event(IMX6UL_ROM_LOG_JUMP_SDP)) {
        if (rom_usb_addressed)
            src->kind = PB_BOOT_SOURCE_USB;
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
    if (imx6ul_rom_log_has_event(IMX6UL_ROM_LOG_JUMP) &&
        imx6ul_rom_log_has_event(IMX6UL_ROM_LOG_PRIMARY_IMAGE) &&
        imx6ul_rom_log_has_event(IMX6UL_ROM_LOG_PRIMARY_SD_MMC) &&
        !imx6ul_rom_log_has_event(IMX6UL_ROM_LOG_DEVICE_INIT_FAIL) &&
        !imx6ul_rom_log_has_event(IMX6UL_ROM_LOG_DEVICE_READ_FAIL) &&
        mmc_get_boot_part_config(src->part_uu) == PB_OK) {
        src->kind = PB_BOOT_SOURCE_STORAGE;
    }
#endif

    return PB_OK;
}
