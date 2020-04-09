
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

#include <pb/pb.h>
#include <pb/console.h>

struct qemu_uart_device
{
    __iomem base;
};

int qemu_uart_setup(struct pb_console_driver *drv);

#endif  // PLAT_TEST_UART_H_
