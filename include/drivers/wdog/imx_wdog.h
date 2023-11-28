/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_DRIVERS_IMX_WDOG_H
#define INCLUDE_DRIVERS_IMX_WDOG_H

#include <stdint.h>

int imx_wdog_init(uintptr_t base, unsigned int delay_s);
int imx_wdog_kick(void);
int imx_wdog_reset_now(void);

#endif // INCLUDE_DRIVERS_IMX_WDOG_H
