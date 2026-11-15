/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_DRIVERS_USB_IMX_USB2_PHY_H
#define INCLUDE_DRIVERS_USB_IMX_USB2_PHY_H

#include <stdint.h>

/* A USBPHY with its own PLL, USBPHY_PLL_SIC (the i.MX8DXL): out of reset,
 * its PLL at 480 MHz, powered */
int imx_usb2_phy_init(uintptr_t phy_base);

/* A USBPHY clocked from the CCM's PLL_USB1, which the caller runs (the
 * i.MX6UL, F-139: its USBPHY has no PLL_SIC, the map ends at VERSION): out
 * of reset, powered */
int imx_usb2_phy_init_ccm_pll(uintptr_t phy_base);

#endif // INCLUDE_DRIVERS_USB_IMX_USB2_PHY_H
