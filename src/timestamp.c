/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/timestamp.h>
#include <stdio.h>
#include <string.h>

struct timestamp {
    const char *description; /* String representation of the timestamp*/
    unsigned int ts; /* Timestamp in us ticks */
};

static struct timestamp timestamps[CONFIG_NO_OF_TIMESTAMPS];
static int ts_count = 0;

void ts(const char *description)
{
    if (ts_count == CONFIG_NO_OF_TIMESTAMPS)
        return;
    timestamps[ts_count].ts = plat_get_us_tick();
    timestamps[ts_count].description = description;
    ts_count++;
}

void ts_print(void)
{
    unsigned int offset = timestamps[0].ts;

    printf("TS (us)   Description\n\r");
    printf("--------  ------------------- \n\r");
    for (int i = 0; i < ts_count; i++) {
        printf("%08i  %s\n\r", timestamps[i].ts - offset, timestamps[i].description);
    }
}

unsigned int ts_total(void)
{
    return timestamps[ts_count - 1].ts - timestamps[0].ts;
}
