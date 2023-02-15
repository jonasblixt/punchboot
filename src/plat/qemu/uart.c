/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <pb/plat.h>
#include <pb/io.h>
#include <pb/arch.h>
#include "uart.h"

int qemu_uart_write(struct qemu_uart_device *dev, char *buf, size_t size)
{

    arch_clean_cache_range((uintptr_t) buf, size);

    for (unsigned int i = 0; i < size; i++)
        pb_write32(buf[i], dev->base); /* Transmit char */

    return PB_OK;
}

int qemu_uart_init(struct qemu_uart_device *dev)
{
    return PB_OK;
}

