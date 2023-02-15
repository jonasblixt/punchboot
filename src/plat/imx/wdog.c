
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <pb/io.h>
#include <pb/plat.h>
#include <plat/imx/wdog.h>

static __iomem base;
static unsigned int wdog_delay_s;

int imx_wdog_init(__iomem base_addr, unsigned int delay)
{
    base = base_addr;
    wdog_delay_s = delay;

    /* Timeout value = 9 * 0.5 + 0.5 = 5 s */
    pb_write16(( (delay * 2) << 8) | (1 << 2) |
                                (1 << 3) |
                                (1 << 4) |
                                (1 << 5),
                base + WDOG_WCR);

    pb_write16(0, base + WDOG_WMCR);

    return imx_wdog_kick();
}

int imx_wdog_kick(void)
{
    pb_write16(0x5555, base + WDOG_WSR);
    pb_write16(0xAAAA, base + WDOG_WSR);

    return PB_OK;
}

int imx_wdog_reset_now(void)
{
    pb_write16(((1 << 6) | (1 << 2)), base + WDOG_WCR);

    while (true)
        __asm__("nop");

    return -PB_ERR;
}
