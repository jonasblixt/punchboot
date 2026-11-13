/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * i.MX8M Plus clocks. The ARM PLL is left as the ROM set it. SYSTEM_PLL1
 * (800 MHz) and SYSTEM_PLL2 (1000 MHz) feed the DRAM clocks and most clock
 * roots; all their divided outputs are enabled after reset (RM,
 * CCM_ANALOG_SYS_PLLn_GEN_CTRL). And USB1's clocks and power, for command
 * mode.
 */

#include <pb/delay.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <plat/imx8m/clock_imx8mp.h>
#include <plat/imx8m/imx8m.h>

#include "plat.h"

#ifdef CONFIG_IMX8M_CM_USB1
/* USB1 PHY glue (RM 11.2.5.2.5) */
#define USB1_PHY_CTRL0          (IMX8M_USB1_PHY_BASE + 0x0)
#define PHY_CTRL0_FSEL_MASK     (0x3f << 5)
#define PHY_CTRL0_FSEL_24MHZ    (0x2a << 5)
/* Reserved in the RM, reset 0b11: the alternate PHY clock. Cleared, the PHY
 * runs on its own PLL (ALT_CLK_EN, ALT_CLK_SEL in NXP's Linux driver) */
#define USB1_PHY_CTRL6          (IMX8M_USB1_PHY_BASE + 0x18)
#define PHY_CTRL6_ALT_CLK       (0x3)
#define GPC_POWER_UP_TIMEOUT_us 1000

static int wait_set(uintptr_t reg, uint32_t mask, uint32_t value)
{
    struct pb_timeout to;

    pb_timeout_init_us(&to, GPC_POWER_UP_TIMEOUT_us);

    while ((mmio_read_32(reg) & mask) != value) {
        if (pb_timeout_has_expired(&to))
            return -PB_ERR_TIMEOUT;
    }

    return PB_OK;
}

/*
 * The ROM powers HSIOMIX and the USB1 PHY and sets their clocks only for a
 * serial download boot (F-115). Power them up here, it does nothing when
 * they are on already.
 */
int imx8m_soc_usb1_power_up(void)
{
    int rc;

    /* The bus clocks first: the NoC handshake needs them. The ROM sets up
     * the USB reference clock roots but enables them only for USB (RM
     * Table 6-4) */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(IMX8MP_HSIO_AXI_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(IMX8MP_USB_CORE_REF_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(IMX8MP_USB_PHY_REF_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CCGR_SET(IMX8MP_CCGR_HSIO), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(IMX8MP_CCGR_USB), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(IMX8MP_CCGR_USB_PHY), CCGR_CLK_ON_MASK);

    mmio_clrsetbits_32(
        IMX8MP_GPC_PGC_CPU_A53_MAPPING, 0, GPC_A53_MAPPING_HSIOMIX | GPC_A53_MAPPING_USB1_PHY);
    mmio_clrsetbits_32(IMX8MP_GPC_PU_PGC_SW_PUP_REQ, 0, GPC_PU_HSIOMIX | GPC_PU_USB1_PHY);

    /* The request bits clear when the domains are up */
    rc = wait_set(IMX8MP_GPC_PU_PGC_SW_PUP_REQ, GPC_PU_HSIOMIX | GPC_PU_USB1_PHY, 0);

    if (rc != PB_OK) {
        LOG_ERR("HSIOMIX/USB1 PHY power up timed out");
        return rc;
    }

    /*
     * Open the NoC to HSIOMIX. The ADB acknowledges only once the BLK_CTRL
     * clock is on as well (USB_CLOCK_MODULE_EN), Linux doesn't wait for it.
     * The PHY reference clock is the 24 MHz oscillator.
     */
    mmio_clrsetbits_32(IMX8MP_GPC_PU_PWRHSK, 0, GPC_PWRHSK_NOC2HSIO_PWRDNREQN);
    mmio_clrsetbits_32(IMX8MP_HSIO_BLK_CTRL_GPR_REG0,
                       HSIO_GPR_REG0_USB_PHY_REF_CLK_SEL,
                       HSIO_GPR_REG0_USB_CLOCK_MODULE_EN);
    rc = wait_set(
        IMX8MP_GPC_PU_PWRHSK, GPC_PWRHSK_NOC2HSIO_PWRDNACKN, GPC_PWRHSK_NOC2HSIO_PWRDNACKN);

    if (rc != PB_OK) {
        LOG_WARN("No HSIOMIX NoC handshake ack (PWRHSK 0x%08x)",
                 mmio_read_32(IMX8MP_GPC_PU_PWRHSK));
    }

    mmio_clrsetbits_32(USB1_PHY_CTRL0, PHY_CTRL0_FSEL_MASK, PHY_CTRL0_FSEL_24MHZ);
    mmio_clrsetbits_32(USB1_PHY_CTRL6, PHY_CTRL6_ALT_CLK, 0);

    return PB_OK;
}
#endif

/* Start a PLL14xx with its reset dividers, unless it is running already */
static void pll14xx_start(uintptr_t gen_ctrl)
{
    if (mmio_read_32(gen_ctrl) & PLL14XX_LOCK)
        return;

    mmio_clrsetbits_32(gen_ctrl, 0, PLL14XX_RST);

    while (!(mmio_read_32(gen_ctrl) & PLL14XX_LOCK))
        ;

    mmio_clrsetbits_32(gen_ctrl, PLL14XX_BYPASS, 0);
}

void imx8m_soc_clock_init(void)
{
    pll14xx_start(IMX8MP_SYS_PLL1_GEN_CTRL);
    pll14xx_start(IMX8MP_SYS_PLL2_GEN_CTRL);
}
