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

volatile uint32_t _uart_base;

void plat_uart_putc(void *ptr, char c) {
    pb_write(c, 0x09000000); /* Transmit char */
}

void test_uart_init(void) {
    _uart_base = 0x09000000;
}
