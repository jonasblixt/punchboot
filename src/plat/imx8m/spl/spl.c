/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8M's SPL (D-088, plat-imx.md 2): it brings up DRAM and leaves
 * loading and authenticating pb proper to the ROM (spl_imx8mq.c,
 * spl_imx8mp.c). The i.MX8MQ's also brings up USB1 when the ROM is in its
 * serial downloader, which can't load pb proper (D-089). No console: its
 * lines are in the RAM log, which pb proper keeps and prints.
 */

#include <drivers/timer/imx_gpt.h>
#include <drivers/usb/dwc3_udc.h>
#include <drivers/usb/imx8m_phy.h>
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

#ifdef IMX8M_SPL_FASTBOOT
/* USB1 for the SPL's fastboot (D-089), as plat_cm_init() for command mode:
 * its clocks, its PHY and the controller's driver. The ROM's serial
 * downloader ran on the port: the PHY and the controller are reset */
int imx8m_spl_usb_init(void)
{
    int rc = imx8m_soc_usb1_power_up();

    if (rc == PB_OK)
        rc = imx8m_usb_phy_init(IMX8M_USB1_PHY_BASE);

    if (rc == PB_OK)
        rc = imx_dwc3_udc_init(IMX8M_USB1_BASE);

    return rc;
}
#endif

#ifdef CONFIG_ENABLE_WATCHDOG
/* WDOG1 armed while the SPL serves a host and calls the ROM's HAB (D-089):
 * what hangs there resets, and the ROM is in its serial downloader again.
 * pb proper arms it again, with its own time (plat_init()) */
void imx8m_spl_wdog_start(unsigned int timeout_s)
{
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(WDOG_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CCGR_SET(CCGR_WDOG1), CCGR_CLK_ON_MASK);
    (void)imx_wdog_init(IMX8M_WDOG1_BASE, timeout_s);
}

void imx8m_spl_wdog_kick(void)
{
    (void)imx_wdog_kick(IMX8M_WDOG1_BASE);
}
#else
void imx8m_spl_wdog_start(unsigned int timeout_s)
{
    (void)timeout_s;
}

void imx8m_spl_wdog_kick(void)
{
}
#endif

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
    rc = imx8m_soc_clock_init();

    if (rc != PB_OK)
        return rc;

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
