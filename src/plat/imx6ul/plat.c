/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/fuse/imx_ocotp.h>
#include <drivers/timer/imx_gpt.h>
#include <drivers/wdog/imx_wdog.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <plat/imx6ul/imx6ul.h>
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

int plat_init(void)
{
    /* Unmask wdog in SRC control reg */
    mmio_clrsetbits_32(IMX6UL_SRC_SCR,
                       SRC_SCR_MASK_WDOG_RST_MASK | SRC_SCR_WARM_RESET_ENABLE,
                       SRC_SCR_MASK_WDOG_RST(10));

#if CONFIG_ENABLE_WATCHDOG
    /* Enable WDT */
    mmio_clrsetbits_32(IMX6UL_CCM_CCGR3, 0, CCM_CCGR3_WDOG1);
    imx_wdog_init(IMX6UL_WDOG1_BASE, CONFIG_WATCHDOG_TIMEOUT);
#endif

    /* Configure systick */
    imx_gpt_init(IMX6UL_GPT1_BASE, MHz(24));

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

    return PB_OK;
}

int plat_board_init(void)
{
    return board_init(&plat);
}
