/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_TIME_H_
#define INCLUDE_PB_TIME_H_

#include <stdint.h>

#define TIMESTAMP(__name__) { \
                                .begin_ts_us = 0, \
                                .end_ts_us = 0, \
                                .description = __name__, \
                                .next = NULL, \
                            };

#define timestamp_foreach(__ts__, __name__) \
        for (struct pb_timestamp *__name__ = __ts__; __name__; __name__ = __name__->next)

struct pb_timestamp
{
    uint32_t begin_ts_us;
    uint32_t end_ts_us;
    const char *description;
    struct pb_timestamp *next;
};

struct pb_timeout {
    uint32_t ts_start;
    uint32_t ts_end;
    bool rollover;
};

void timestamp_init(void);
int timestamp_begin(struct pb_timestamp *ts);
int timestamp_end(struct pb_timestamp *ts);
const char *timestamp_description(struct pb_timestamp *ts);
int timestamp_read_us(struct pb_timestamp *ts);
struct pb_timestamp * timestamp_get_first(void);

void pb_delay_ms(uint32_t delay);
void pb_delay_us(uint32_t delay);

int pb_timeout_init_us(struct pb_timeout *timeout, uint32_t timeout_us);
bool pb_timeout_has_expired(struct pb_timeout *timeout);

#endif  // INCLUDE_PB_TIMESTAMP_H_
