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
#include <pb/timestamp.h>

static struct pb_timestamp *first = NULL;
static struct pb_timestamp *last = NULL;

int timestamp_begin(struct pb_timestamp *ts)
{
    ts->begin_ts_us = arch_get_us_tick();
    return PB_OK;
}

int timestamp_end(struct pb_timestamp *ts)
{
    ts->end_ts_us = arch_get_us_tick();

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
