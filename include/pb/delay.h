/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_DELAY_H_
#define INCLUDE_PB_DELAY_H_

#include <stdint.h>

struct pb_timeout {
    uint32_t ts_start;
    uint32_t ts_end;
    bool rollover;
};

int pb_timeout_init_us(struct pb_timeout *timeout, uint32_t timeout_us);
bool pb_timeout_has_expired(struct pb_timeout *timeout);
void pb_delay_ms(uint32_t delay);
void pb_delay_us(uint32_t delay);

#endif  // INCLUDE_PB_DELAY_H_
