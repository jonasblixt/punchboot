/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8M Plus EVK's console, UART2, in the SPL too (plat/imx8m/spl.h)
 */

#include <drivers/pinctrl/imx_iomuxc.h>
#include <drivers/uart/imx_uart.h>
#include <pb/console.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <plat/imx8m/imx8m.h>
#include <plat/imx8m/spl.h>

#include "console.h"

void imx8mpevk_console_init(void)
{
    /* UART2_CLK_ROOT runs from the 24 MHz reference after reset */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(UART2_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CCGR_SET(CCGR_UART2), CCGR_CLK_ON_MASK);

    /* Before plat_init(): the IOMUXC's gate, CCGR27, is on out of reset
     * (0x2, RUN and WAIT) and the ROM leaves it on (RM Table 6-6, F-412) */
    static const struct imx_pin pins[] = {
        IMX8M_PIN(UART2_RXD, MUX_ALT(0), IMX_PAD_KEEP),
        IMX8M_PIN(UART2_TXD, MUX_ALT(0), IMX_PAD_KEEP),
    };

    imx_iomuxc_setup(pins, ARRAY_SIZE(pins));

    imx_uart_init(IMX8M_UART2_BASE, MHz(24), 115200);

    static const struct console_ops ops = {
        .putc = imx_uart_putc,
    };

    console_init(IMX8M_UART2_BASE, &ops);
}

#ifdef PB_SPL
void imx8m_spl_console_init(void)
{
    imx8mpevk_console_init();
}
#endif
