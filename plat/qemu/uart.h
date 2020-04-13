
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
#include <pb/io.h>

struct qemu_uart_device
{
    __iomem base;
};

int qemu_uart_write(struct qemu_uart_device *dev, char *buf, size_t size);
int qemu_uart_init(struct qemu_uart_device *dev);

#endif  // PLAT_TEST_UART_H_
