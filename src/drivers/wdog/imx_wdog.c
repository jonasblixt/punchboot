/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/wdog/imx_wdog.h>
#include <pb/mmio.h>
#include <pb/pb.h>

static uintptr_t base;

int imx_wdog_init(uintptr_t base_, unsigned int delay_s)
{
    base = base_;

    /* Timeout value = 9 * 0.5 + 0.5 = 5 s */
    uint16_t regval = WDOG_WCR_WT(delay_s * 2) | WDOG_WCR_WDE | WDOG_WCR_WDT | WDOG_WCR_SRS |
                      WDOG_WCR_WDA;

#ifdef CONFIG_DRIVERS_IMX_WDOG_DEBUG
    regval |= WDOG_WCR_WDBG;
#endif
#ifdef CONFIG_DRIVERS_IMX_WDOG_WAIT
    regval |= WDOG_WCR_WDW;
#endif
    mmio_write_16(base + WDOG_WCR, regval);

    mmio_write_16(base + WDOG_WMCR, 0);

    return imx_wdog_kick();
}

int imx_wdog_kick(void)
{
    mmio_write_16(base + WDOG_WSR, WDOG_WSR_SEQ1);
    mmio_write_16(base + WDOG_WSR, WDOG_WSR_SEQ2);

    return PB_OK;
}

int imx_wdog_reset_now(void)
{
    /* SRS 0: a software reset */
    mmio_write_16(base + WDOG_WCR, WDOG_WCR_SRE | WDOG_WCR_WDE);

    while (true) {
    };

    return -PB_ERR;
}
