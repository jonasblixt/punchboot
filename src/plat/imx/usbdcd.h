/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_IMX_USBDCD_H_
#define PLAT_IMX_USBDCD_H_

#include <stdint.h>
#include <pb/usb.h>

#define IMX_USBDCD_CONTROL (0x00)
#define IMX_USBDCD_CLOCK   (0x04)
#define IMX_USBDCD_STATUS  (0x08)
#define IMX_USBDCD_SIGNAL_OVERRIDE (0x0c)
#define IMX_USBDCD_TIMER0  (0x10)
#define IMX_USBDCD_TIMER1  (0x14)
#define IMX_USBDCD_TIMER2_BC11 (0x18)
#define IMX_USBDCD_TIMER2_BC12 (0x18)

struct imx_usbdcd
{
    uintptr_t base;
};

int imx_usbdcd_init(struct imx_usbdcd *dev, uintptr_t base);
int imx_usbdcd_start(struct imx_usbdcd *dev);
int imx_usbdcd_poll_result(struct imx_usbdcd *dev,
                            enum usb_charger_type *result);

#endif  // PLAT_IMX_USBDCD_H_
