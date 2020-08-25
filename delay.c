
/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <pb/arch.h>

void pb_delay_ms(unsigned int ms)
{
    uint32_t ts = arch_get_us_tick();

    while ( (arch_get_us_tick()-ts) < (ms*1000))
        __asm__ volatile ("nop");
}

void pb_delay_us(unsigned int us)
{
    uint32_t ts = arch_get_us_tick();

    while ( (arch_get_us_tick()-ts) < us)
        __asm__ volatile ("nop");
}

