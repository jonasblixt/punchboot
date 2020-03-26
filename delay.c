
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <plat.h>


void plat_delay_ms(uint32_t delay)
{
    uint32_t ts = plat_get_us_tick();

    while ( (plat_get_us_tick()-ts) < (delay*1000))
        __asm__ volatile ("nop");
}

