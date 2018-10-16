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
#include <tinyprintf.h>

void plat_reset(void) {
    pb_writel(0, 0x020D8000);
    pb_write((1 << 2) | (1 << 3) | (1 << 4), 0x020BC000);
 
    LOG_INFO("Reset...");   
    while (1)
        asm("nop");
}
