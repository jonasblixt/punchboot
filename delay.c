
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

void pb_delay_ms(uint32_t delay)
{
    uint32_t ts = arch_get_us_tick();

    while ( (arch_get_us_tick()-ts) < (delay*1000))
        __asm__ volatile ("nop");
}

