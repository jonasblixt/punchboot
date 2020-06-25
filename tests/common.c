/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <pb/board.h>
#include <pb/plat.h>
#include <plat/qemu/pl061.h>
#include <plat/qemu/gcov.h>
#include <plat/qemu/uart.h>
#include <plat/qemu/semihosting.h>
#include <pb/assert.h>
#include "test.h"

static struct qemu_uart_device console_uart =
{
    .base = 0x09000000,
};

int plat_console_putchar(char c)
{
    qemu_uart_write(&console_uart, (char *) &c, 1);
    return PB_OK;
}
/*
void __assert(const char *file, unsigned int line,
              const char *assertion)
{
    printf("Assert failed %s:%i (%s)\n\r", file, line, assertion);

    gcov_final();
    semihosting_sys_exit(1);

    while (1) {}
}*/

int plat_get_uuid(char *out)
{
    return PB_OK;
}

void pb_main(void)
{
    qemu_uart_init(&console_uart);
    gcov_init();
    test_main();
    gcov_final();
    semihosting_sys_exit(0);
}
