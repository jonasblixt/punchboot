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

#define PB_BOOTPART_OFFSET 0

#define PADRING_IFMUX_EN_SHIFT		31
#define PADRING_IFMUX_EN_MASK		(1U << PADRING_IFMUX_EN_SHIFT)
#define PADRING_GP_EN_SHIFT		30
#define PADRING_GP_EN_MASK		(1U << PADRING_GP_EN_SHIFT)
#define PADRING_IFMUX_SHIFT		27
#define PADRING_IFMUX_MASK		(0x7U << PADRING_IFMUX_SHIFT)
#define PADRING_CONFIG_SHIFT		25
#define PADRING_CONFIG_MASK		(0x3U << PADRING_CONFIG_SHIFT)
#define PADRING_LPCONFIG_SHIFT		23
#define PADRING_LPCONFIG_MASK		(0x3U << PADRING_LPCONFIG_SHIFT)
#define PADRING_PULL_SHIFT		5
#define PADRING_PULL_MASK		(0x3U << PADRING_PULL_SHIFT)
#define PADRING_DSE_SHIFT		0
#define PADRING_DSE_MASK		(0x7U << PADRING_DSE_SHIFT)

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

#define IMX8X_PAD_MUX(val) ((val << PADRING_IFMUX_SHIFT) & PADRING_IFMUX_MASK)

#define PLAT_VIRT_ADDR_SPACE_SIZE    (2ull << 32)
#define PLAT_PHY_ADDR_SPACE_SIZE    (2ull << 32)

#define MAX_XLAT_TABLES            32
#define MAX_MMAP_REGIONS        32

#define IMX_CAAM_BASE 0x31430000
#define IMX_EHCI_BASE 0x5b0d0000
#define IMX_GPIO_BASE 0x5D080000
#define IMX_USBDCD_BASE 0x5b100800
#define IMX_GPT_BASE 0x5D140000
#define SC_IPC_BASE  0x5d1b0000
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
