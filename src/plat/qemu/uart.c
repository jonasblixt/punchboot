/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "uart.h"
#include <pb/mmio.h>
#include <pb/pb.h>
#include <stdint.h>

void qemu_uart_putc(uintptr_t base, char c)
{
    mmio_write_32(base, c); /* Transmit char */
}
