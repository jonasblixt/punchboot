/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/timestamp.h>

struct timestamp
{
    const char *description;         /* String representation of the timestamp*/
    unsigned int nested_count;       /* If a new timestamp is started before
                                        the last one is finished, this varialbe
                                        will be > 0 to indicate a nested measurement */
    uint32_t ts_start_us;            /* Start time in us ticks */
    uint32_t ts_end_us;              /* End time in us ticks */
};

static struct timestamp ts[PB_TIMESTAMP_NO_OF_TS];
static unsigned int ts_count = 0;
static unsigned int ts_nested_count = 0;

/* ts_active holds a list of concurrently active timers */
static unsigned int ts_active[PB_TIMESTAMP_NO_OF_TS];
static unsigned int ts_active_count = 0;

int pb_timestamp_begin(const char *description)
{
    if (ts_count >= PB_TIMESTAMP_NO_OF_TS)
        return -PB_ERR_MEM;

    ts[ts_count].ts_start_us = plat_get_us_tick();
    ts[ts_count].description = description;
    ts[ts_count].nested_count = ts_nested_count;

    ts_active[ts_active_count++] = ts_count;
    ts_nested_count++;
    ts_count++;

    return PB_OK;
}

int pb_timestamp_end(void)
{
    if (ts_active_count == 0)
        return -PB_ERR;
    ts_active_count--;
    unsigned int index = ts_active[ts_active_count];

    if (index >= PB_TIMESTAMP_NO_OF_TS)
        return -PB_ERR;

    ts[index].ts_end_us = plat_get_us_tick();
    ts_nested_count--;

    return PB_OK;
}

void pb_timestamp_print(void)
{
    if (ts_active_count != 0)
        LOG_WARN("Warning: There are still timers running, report will be incorrect");
    printf("--- Timing report begin --- \n\r");
    for (unsigned int i = 0; i < ts_count; i++) {
        for (unsigned int n = 0; n < ts[i].nested_count; n++)
            printf("    ");
        printf("%s %u us\n\r", ts[i].description,
                               ts[i].ts_end_us - ts[i].ts_start_us);
    }
    printf("--- Timing report end ---\n\r");
}

int pb_timestamp_total(void)
{
    if (ts_count == 0)
        return -PB_ERR;
    if (ts_active_count != 0)
        return -PB_ERR;

    return ts[ts_count - 1].ts_end_us - ts[0].ts_start_us;
}
