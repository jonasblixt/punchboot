/**
 * Punch BOOT
 *
 * Copyright (C) 2020, 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The pico8ml's console, UART1, in both stages (D-088)
 */

#include <drivers/pinctrl/imx_iomuxc.h>
#include <drivers/uart/imx_uart.h>
#include <pb/console.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <plat/imx8m/imx8m.h>
#include <plat/imx8m/spl.h>

#include "console.h"

void pico8ml_console_init(void)
{
    /* Enable UART1 clock */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(UART1_CLK_ROOT), CLK_ROOT_ON);
    /* Ungate UART1 clock */
    mmio_write_32(CCM_CCGR_SET(CCGR_UART1), CCGR_CLK_ON_MASK);

    static const struct imx_pin pins[] = {
        IMX8M_PIN(UART1_RXD, MUX_ALT(0), PAD_CTL_DSE_40_OHM),
        IMX8M_PIN(UART1_TXD, MUX_ALT(0), PAD_CTL_DSE_40_OHM),
    };

    imx_iomuxc_setup(pins, ARRAY_SIZE(pins));

    imx_uart_init(IMX8M_UART1_BASE, MHz(25), 115200);

    static const struct console_ops ops = {
        .putc = imx_uart_putc,
    };

    console_init(IMX8M_UART1_BASE, &ops);
}

#ifdef PB_SPL
void imx8m_spl_console_init(void)
{
    pico8ml_console_init();
}
#endif
