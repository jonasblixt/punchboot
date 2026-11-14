/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_DELAY_H_
#define INCLUDE_PB_DELAY_H_

#include <stdbool.h>
#include <stdint.h>

/* A timeout is the tick at its start and its length: it has expired when
 * plat_get_us_tick() - ts_start >= timeout_us, which holds across the tick's
 * wrap (platform.md 3.2). A poll sees it until the tick has gone round once
 * since the start, 71.6 minutes. */
struct pb_timeout {
    uint32_t ts_start;
    uint32_t timeout_us;
};

/* 0 and UINT32_MAX are refused, -PB_ERR, and the timeout has then expired */
int pb_timeout_init_us(struct pb_timeout *timeout, uint32_t timeout_us);
bool pb_timeout_has_expired(struct pb_timeout *timeout);
void pb_delay_ms(uint32_t delay);
void pb_delay_us(uint32_t delay);

#endif // INCLUDE_PB_DELAY_H_
