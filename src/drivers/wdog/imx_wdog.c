/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <pb/mmio.h>
#include <drivers/wdog/imx_wdog.h>

#define WDOG_WCR  0x0000
#define WDOG_WSR  0x0002
#define WDOG_WRSR 0x0004
#define WDOG_WICR 0x0006
#define WDOG_WMCR 0x0008

static uintptr_t base;

int imx_wdog_init(uintptr_t base_, unsigned int delay_s)
{
    base = base_;

    /* Timeout value = 9 * 0.5 + 0.5 = 5 s */
    uint16_t regval = ((delay_s * 2) << 8) |
                            (1 << 2) |
                            (1 << 3) |
                            (1 << 4) |
                            (1 << 5);

#ifdef CONFIG_DRIVERS_IMX_WDOG_DEBUG
    regval |= (1 << 1);
#endif
#ifdef CONFIG_DRIVERS_IMX_WDOG_WAIT
    regval |= (1 << 7);
#endif
    mmio_write_16(base + WDOG_WCR, regval);

    mmio_write_16(base + WDOG_WMCR, 0);

    return imx_wdog_kick();
}

int imx_wdog_kick(void)
{
    mmio_write_16(base + WDOG_WSR, 0x5555);
    mmio_write_16(base + WDOG_WSR, 0xAAAA);

    return PB_OK;
}

int imx_wdog_reset_now(void)
{
    mmio_write_16(base + WDOG_WCR, ((1 << 6) | (1 << 2)));

    while (true)
        {};

    return -PB_ERR;
}
