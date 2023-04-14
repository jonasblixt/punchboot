/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_DRIVERS_USB_IMX_EHCI_H
#define INCLUDE_DRIVERS_USB_IMX_EHCI_H

#include <stdint.h>

int imx_ehci_init(uintptr_t base);

#endif  // INCLUDE_DRIVERS_USB_IMX_EHCI_H
