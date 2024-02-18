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
#include <plat/imx8m/imx8m.h>
#include <uuid.h>
#include <xlat_tables.h>

#include "umctl2.h"

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

static struct imx8m_platform plat;

const char *platform_ns_uuid = "\x32\x92\xd7\xd2\x28\x25\x41\x00\x90\xc3\x96\x8f\x29\x60\xc9\xf2";

static const mmap_region_t imx_mmap[] = {
    /* UMCTL2 state */
    MAP_REGION_FLAT(0x40000000, (128 * 1024), MT_MEMORY | MT_RW),
    /* DDRC */
    MAP_REGION_FLAT(0x3c000000, (22 * 1024 * 1024), MT_DEVICE | MT_RW),
    /* Boot ROM API*/
    MAP_REGION_FLAT(0x00000000, (128 * 1024), MT_MEMORY | MT_RO | MT_EXECUTE),
    /* Periph (AIPS) */
    MAP_REGION_FLAT(0x30000000, (16 * 1024 * 1024), MT_DEVICE | MT_RW),
    /* OCRAM */
    MAP_REGION_FLAT(0x00900000, (128 * 1024), MT_MEMORY | MT_RW),
    /* GVP */
    MAP_REGION_FLAT(0x32000000, 0x20000, MT_DEVICE | MT_RW),
    /* USB */
    MAP_REGION_FLAT(0x38100000, (2 * 1024 * 1024), MT_DEVICE | MT_RW),
    { 0 }
};

int plat_boot_reason(void)
{
    return -PB_ERR_NOT_IMPLEMENTED;
}

const char *plat_boot_reason_str(void)
{
    return "";
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

    imx_ocotp_read(0, 1, &plat_unique.uid[0]);
    imx_ocotp_read(0, 2, &plat_unique.uid[1]);
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

    enable_mmu_el3(0);

    return PB_OK;
}

int plat_init(void)
{
#if CONFIG_ENABLE_WATCHDOG
    /* Enable and ungate WDOG1 clock */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(WDOG_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CCGR_SET(CCGR_WDOG1), CCGR_CLK_ON_MASK);
    imx_wdog_init(IMX8M_WDOG1_BASE, CONFIG_WATCHDOG_TIMEOUT);
#endif

    /* Init debug console */
    board_console_init(&plat);

    /* Init fusebox */
    imx_ocotp_init(IMX8M_OCOTP_BASE, 4);

    /* Configure systick */
    mmio_write_32(CCM_CCGR_SET(CCGR_GPT1), CCGR_CLK_ON_MASK);
    imx_gpt_init(IMX8M_GPT1_BASE, MHz(24));

    /* Read SOC variant and version */
    // private.soc_ver_var = mmio_read_32(CCM_ANALOG_DIGPROG);

    /* Ungate GPIO clocks */
    mmio_write_32(CCM_CCGR_SET(CCGR_GPIO1), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_GPIO2), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_GPIO3), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_GPIO4), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_GPIO5), CCGR_CLK_ON_MASK);

    /* Ungate IOMUX clocks */
    mmio_write_32(CCM_CCGR_SET(CCGR_IOMUX), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_IOMUX1), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_IOMUX2), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_IOMUX3), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_IOMUX4), CCGR_CLK_ON_MASK);

    /* Configure main clocks */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(ARM_A53_CLK_ROOT), CLK_ROOT_ON);

    /* Configure PLL's */
    /* bypass the clock */
    mmio_clrsetbits_32(ARM_PLL_CFG0, 0, FRAC_PLL_BYPASS_MASK);

    /* Set CPU core clock to 1 GHz */
    mmio_write_32(ARM_PLL_CFG1, FRAC_PLL_INT_DIV_CTL_VAL(49));

    mmio_write_32(ARM_PLL_CFG0,
                  (FRAC_PLL_CLKE_MASK | FRAC_PLL_REFCLK_SEL_OSC_25M | FRAC_PLL_LOCK_SEL_MASK |
                   FRAC_PLL_NEWDIV_VAL_MASK | FRAC_PLL_REFCLK_DIV_VAL(4) |
                   FRAC_PLL_OUTPUT_DIV_VAL(0) | FRAC_PLL_BYPASS_MASK));

    /* unbypass the clock */
    mmio_clrsetbits_32(ARM_PLL_CFG0, FRAC_PLL_BYPASS_MASK, 0);

    while (!(mmio_read_32(ARM_PLL_CFG0) & FRAC_PLL_LOCK_MASK))
        ;

    mmio_clrsetbits_32(ARM_PLL_CFG0, FRAC_PLL_NEWDIV_VAL_MASK, 0);

    mmio_clrsetbits_32(SYS_PLL1_CFG0,
                       0,
                       SSCG_PLL_CLKE_MASK | SSCG_PLL_DIV2_CLKE_MASK | SSCG_PLL_DIV3_CLKE_MASK |
                           SSCG_PLL_DIV4_CLKE_MASK | SSCG_PLL_DIV5_CLKE_MASK |
                           SSCG_PLL_DIV6_CLKE_MASK | SSCG_PLL_DIV8_CLKE_MASK |
                           SSCG_PLL_DIV10_CLKE_MASK | SSCG_PLL_DIV20_CLKE_MASK);

    mmio_clrsetbits_32(SYS_PLL2_CFG0,
                       0,
                       SSCG_PLL_CLKE_MASK | SSCG_PLL_DIV2_CLKE_MASK | SSCG_PLL_DIV3_CLKE_MASK |
                           SSCG_PLL_DIV4_CLKE_MASK | SSCG_PLL_DIV5_CLKE_MASK |
                           SSCG_PLL_DIV6_CLKE_MASK | SSCG_PLL_DIV8_CLKE_MASK |
                           SSCG_PLL_DIV10_CLKE_MASK | SSCG_PLL_DIV20_CLKE_MASK);

    umctl2_init();

    LOG_DBG("LPDDR4 training complete");

    imx_wdog_kick();
    plat_mmu_init();

    LOG_DBG("Plat init done");
    return PB_OK;
}

int plat_board_init(void)
{
    return board_init(&plat);
}
