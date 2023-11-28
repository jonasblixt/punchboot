/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_DRIVERS_USB_IMX_USBDCD_H
#define INCLUDE_DRIVERS_USB_IMX_USBDCD_H

#include <drivers/usb/usbd.h>
#include <stdint.h>

int imx_usbdcd_init(uintptr_t base);
int imx_usbdcd_start(void);
enum usb_charger_type imx_usbdcd_poll_result(void);

#endif // INCLUDE_DRIVERS_USB_IMX_USBDCD_H
