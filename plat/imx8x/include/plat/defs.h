/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_IMX8X_INCLUDE_PLAT_DEFS_H_
#define PLAT_IMX8X_INCLUDE_PLAT_DEFS_H_

#ifndef __ASSEMBLY__
#include <pb/pb.h>
#include <pb/io.h>
#include <plat/imx/usdhc.h>
#include <plat/imx/ehci.h>
#include <plat/imx/lpuart.h>
#include <plat/imx/caam.h>
#include <plat/imx/gpio.h>
#include <plat/imx8qx_pads.h>
#include <plat/imx8_iomux.h>

/* Taken from ATF. */
#include "platform_def.h"

#define PB_BOOTPART_OFFSET 0


#define ESDHC_PAD_CTRL    (PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | \
                         (SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | \
                         (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
                         (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | \
                         (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define ESDHC_CLK_PAD_CTRL    (PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | \
                             (SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | \
                             (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
                             (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | \
                             (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define UART_PAD_CTRL    (PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | \
            (SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | \
            (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
            (SC_PAD_28FDSOI_DSE_DV_LOW << PADRING_DSE_SHIFT) | \
            (SC_PAD_28FDSOI_PS_PD << PADRING_PULL_SHIFT))



#define GPIO_PAD_CTRL   (PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | \
                         (SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | \
                         (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
                         (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | \
                         (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define PLAT_VIRT_ADDR_SPACE_SIZE	(2ull << 32)
#define PLAT_PHY_ADDR_SPACE_SIZE	(2ull << 32)

#define MAX_XLAT_TABLES			32
#define MAX_MMAP_REGIONS		32

#define IMX_CAAM_BASE 0x31430000
#define IMX_EHCI_BASE 0x5b0d0000
#define IMX_GPIO_BASE 0x5D080000
#define IMX_USBDCD_BASE 0x5b100800
#define IMX_GPT_BASE 0x5D140000
#define IMX_GPT_PR 24

#define IMX_LPUART_BAUDRATE 0x402008b

#ifdef CONFIG_CONSOLE_UART0
#   define IMX_LPUART_BASE 0x5A060000
#elif CONFIG_CONSOLE_UART1
#   define IMX_LPUART_BASE 0x5A070000
#elif CONFIG_CONSOLE_UART2
#   define IMX_LPUART_BASE 0x5A080000
#else
#   error "Invalid console uart setting"
#endif
#endif  // __ASSEMBLY__

#define COUNTER_FREQUENCY (8000000)
#define COUNTER_US_SHIFT (3)
#define CACHE_LINE 64

#endif  // PLAT_IMX8X_INCLUDE_PLAT_DEFS_H_
