/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef DRIVERS_IMX_LPUART_H
#define DRIVERS_IMX_LPUART_H

#include <inttypes.h>

int imx_lpuart_init(uintptr_t base, unsigned int input_clock_Hz, unsigned int baudrate);

void imx_lpuart_putc(uintptr_t base, char c);

#endif
