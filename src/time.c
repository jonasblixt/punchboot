/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/time.h>
#include <string.h>

static struct pb_timestamp *first = NULL;
static struct pb_timestamp *last = NULL;

void timestamp_init(void)
{
    first = NULL;
    last = NULL;
}

int timestamp_begin(struct pb_timestamp *ts)
{
    ts->next = NULL;
    ts->begin_ts_us = plat_get_us_tick();
    return PB_OK;
}

int timestamp_end(struct pb_timestamp *ts)
{
    ts->end_ts_us = plat_get_us_tick();

    if (!first)
    {
        first = ts;
        last = ts;
    }
    else
    {
        last->next = ts;
        last = ts;
    }

    return PB_OK;
}

const char *timestamp_description(struct pb_timestamp *ts)
{
    return ts->description;
}

struct pb_timestamp * timestamp_get_first(void)
{
    return first;
}

int timestamp_read_us(struct pb_timestamp *ts)
{
    return (ts->end_ts_us - ts->begin_ts_us);
}

void pb_delay_ms(uint32_t delay)
{
    pb_delay_us(delay * 1000);
}

void pb_delay_us(unsigned int us)
{
    struct pb_timeout to;

    if (pb_timeout_init_us(&to, us) == PB_OK) {
        while (pb_timeout_has_expired(&to) == false) {
            __asm__ volatile ("nop");
        }
    }
}

int pb_timeout_init_us(struct pb_timeout *timeout, uint32_t timeout_us)
{
    if ((timeout_us == 0) || (timeout_us == 0xffffffff))
        return -PB_ERR;

    memset(timeout, 0, sizeof(*timeout));
    timeout->ts_start = plat_get_us_tick();

    if ((0xffffffff - timeout->ts_start) <= timeout_us) {
        timeout->rollover = true;
        timeout->ts_end = timeout_us - (0xffffffff - timeout->ts_start);
    } else {
        timeout->ts_end = timeout->ts_start + timeout_us;
    }

    return PB_OK;
}

bool pb_timeout_has_expired(struct pb_timeout *timeout)
{
    uint32_t ts = plat_get_us_tick();

    if (timeout->rollover) {
        if ((ts >= timeout->ts_end) && (ts < timeout->ts_start))
            return true;
        else
            return false;
    } else {
        if (ts >= timeout->ts_end)
            return true;
        else
            return false;
    }
}
