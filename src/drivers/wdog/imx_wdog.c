/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/wdog/imx_wdog.h>
#include <pb/mmio.h>
#include <pb/pb.h>

int imx_wdog_init(uintptr_t base, unsigned int delay_s)
{
    /* The time-out is 0.5 s + WT * 0.5 s, so delay_s is WT = 2 * delay_s - 1.
     * 0 is the shortest, half a second. WT is eight bits: a time-out that
     * doesn't fit is the longest one, 128 s, not one that wrapped into a
     * short one */
    unsigned int wt;

    if (delay_s == 0)
        wt = 0;
    else if (delay_s <= (WDOG_WT_MAX + 1) / 2)
        wt = delay_s * 2 - 1;
    else
        wt = WDOG_WT_MAX;

    uint16_t regval = WDOG_WCR_WT(wt) | WDOG_WCR_WDE | WDOG_WCR_WDT | WDOG_WCR_SRS | WDOG_WCR_WDA;

#ifdef CONFIG_DRIVERS_IMX_WDOG_DEBUG
    regval |= WDOG_WCR_WDBG;
#endif
#ifdef CONFIG_DRIVERS_IMX_WDOG_WAIT
    regval |= WDOG_WCR_WDW;
#endif
    mmio_write_16(base + WDOG_WCR, regval);

    mmio_write_16(base + WDOG_WMCR, 0);

    return imx_wdog_kick(base);
}

void imx_wdog_disable_powerdown(uintptr_t wdog_base)
{
    mmio_write_16(wdog_base + WDOG_WMCR, 0);
}

int imx_wdog_kick(uintptr_t base)
{
    mmio_write_16(base + WDOG_WSR, WDOG_WSR_SEQ1);
    mmio_write_16(base + WDOG_WSR, WDOG_WSR_SEQ2);

    return PB_OK;
}

int imx_wdog_reset_now(uintptr_t base)
{
    /* SRS 0: a software reset. WDA 0: WDOG_B is asserted too (imx_wdog.h) */
    mmio_write_16(base + WDOG_WCR, WDOG_WCR_SRE | WDOG_WCR_WDE);

    /* The reset follows */
    while (true) {
    }
}
