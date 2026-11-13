/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/delay.h>
#include <pb/pb.h>
#include <pb/plat.h>

int pb_timeout_init_us(struct pb_timeout *timeout, uint32_t timeout_us)
{
    timeout->ts_start = plat_get_us_tick();

    /* A refused timeout has expired: a caller that ignores the result doesn't wait */
    if ((timeout_us == 0) || (timeout_us == UINT32_MAX)) {
        timeout->timeout_us = 0;
        return -PB_ERR;
    }

    timeout->timeout_us = timeout_us;
    return PB_OK;
}

bool pb_timeout_has_expired(struct pb_timeout *timeout)
{
    /* A difference, never an end tick: it holds across the tick's wrap
     * (platform.md 3.2) */
    uint32_t elapsed = (uint32_t)(plat_get_us_tick() - timeout->ts_start);

    return elapsed >= timeout->timeout_us;
}

void pb_delay_us(unsigned int us)
{
    struct pb_timeout to;

    if (pb_timeout_init_us(&to, us) == PB_OK) {
        while (pb_timeout_has_expired(&to) == false) {
            __asm__ volatile("nop");
        }
    }
}

void pb_delay_ms(uint32_t delay)
{
    pb_delay_us(delay * 1000);
}
