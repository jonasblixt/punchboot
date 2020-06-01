
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/timing_report.h>

#if TIMING_REPORT >= 1

static uint32_t tr_stamps[TR_NO];

static const char *kind_strings[] =
{
    "TR_POR",
    "TR_BLINIT",
    "TR_LOAD",
    "TR_VERIFY",
    "TR_DT_PATCH",
    "TR_TOTAL"
};


void tr_init(void)
{
}

void tr_stamp_begin(uint32_t kind)
{
    tr_stamps[kind] = arch_get_us_tick();
}

void tr_stamp_end(uint32_t kind)
{
    tr_stamps[kind] = (arch_get_us_tick() - tr_stamps[kind]);
}

void tr_print_result(void)
{
    printf("Boot timing report:\n\r");
    for (uint32_t i = 0; i < TR_NO; i++)
        printf("%s: %u us\n\r", kind_strings[i],
                                    tr_stamps[i]);
}
#else
void tr_init(void) {}
void tr_stamp_begin(uint32_t kind) { UNUSED(kind); }
void tr_stamp_end(uint32_t kind) { UNUSED(kind); }
void tr_print_result(void) {}
#endif
