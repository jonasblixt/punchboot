
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_TEST_UART_H_
#define PLAT_TEST_UART_H_

#include <stdint.h>

void qemu_uart_putc(uintptr_t base, char c);

#endif  // PLAT_TEST_UART_H_
