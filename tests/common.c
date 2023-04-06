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
#include <pb/console.h>
#include <plat/qemu/gcov.h>
#include <plat/qemu/uart.h>
#include <plat/qemu/semihosting.h>
#include <pb/assert.h>
#include "test.h"

void main(void)
{
    int rc;

    static const struct console_ops ops = {
        .putc = qemu_uart_putc,
    };

    console_init(0x09000000, &ops);

    gcov_init();
    test_main();
    rc = gcov_store_output();
    if (rc < 0) {
        LOG_ERR("GCOV error %i", rc);
    }
    semihosting_sys_exit(rc);
}
