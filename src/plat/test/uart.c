/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <plat.h>
#include <io.h>
#include "uart.h"

volatile uint32_t _uart_base;

void plat_uart_putc(void *ptr, char c) {
    UNUSED(ptr);
    pb_write32(c, 0x09000000); /* Transmit char */
}

void test_uart_init(void) {
    _uart_base = 0x09000000;
}
