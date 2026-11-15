/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8M's SPL (D-088, plat-imx.md 2): it brings up DRAM and nothing
 * else, and leaves loading and authenticating pb proper to the ROM
 * (spl_imx8mq.c, spl_imx8mp.c). No console: its lines are in the RAM log,
 * which pb proper keeps and prints.
 */

#include <drivers/timer/imx_gpt.h>
#include <drivers/wdog/imx_wdog.h>
#include <pb/console.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <plat/imx8m/dram.h>
#include <plat/imx8m/imx8m.h>
#include <plat/imx8m/spl.h>

#include "../plat.h"

/* The tick of the training's timeouts and of the log, GPT1 as pb proper's */
uint32_t plat_get_us_tick(void)
{
    return imx_gpt_get_tick();
}

/* A reset through WDOG1, as plat_reset(); its clock is ungated here, the
 * SPL runs before plat_early_init() */
void imx8m_spl_reset(void)
{
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(WDOG_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CCGR_SET(CCGR_WDOG1), CCGR_CLK_ON_MASK);
    imx_wdog_reset_now(IMX8M_WDOG1_BASE);
}

/* Without the board's console the SPL writes the RAM log only */
__attribute__((weak)) void imx8m_spl_console_init(void)
{
}

int imx8m_spl_dram_init(void)
{
    int tzasc_rc;
    int rc;

    /* GPT1_CLK_ROOT is on out of reset, on the 24 MHz reference (CCM_TARGET_ROOT107
     * resets to 0x10000000, RM 5.1.7.10) */
    mmio_write_32(CCM_CCGR_SET(CCGR_GPT1), CCGR_CLK_ON_MASK);
    imx_gpt_init(IMX8M_GPT1_BASE, MHz(24));
    log_time_start();

    /* What plat_init() did before the training until D-088: the GPIO and
     * IOMUX gates (a board reads its DRAM's straps), the SoC's clocks */
    mmio_write_32(CCM_CCGR_SET(CCGR_GPIO1), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_GPIO2), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_GPIO3), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_GPIO4), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_GPIO5), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_IOMUX), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_IOMUX1), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_IOMUX2), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_IOMUX3), CCGR_CLK_ON_MASK);
    imx8m_soc_clock_init();

    /* Out of bypass before DRAM is used (platform.md 4.3). A failure isn't
     * the SPL's: pb proper finds it, records it, and no boot goes on */
    tzasc_rc = imx8m_tzasc_enable();

    rc = imx8m_ddr_init();

    if (rc != PB_OK) {
        LOG_ERR("LPDDR4 init failed (%i)", rc);
        return rc;
    }

    LOG_INFO("LPDDR4 trained");

    if (tzasc_rc == PB_OK && imx8m_tzasc_open_dram() != PB_OK)
        LOG_ERR("TZASC setup failed");

    return PB_OK;
}
