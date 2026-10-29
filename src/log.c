/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The start of every log line: the time since log_time_start(), the level
 * and the function.
 */

#include <pb/console.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <stdbool.h>
#include <stdio.h>

static bool log_time_running;
static unsigned int log_time_base;

void log_time_start(void)
{
    log_time_base = plat_get_us_tick();
    log_time_running = true;
}

void log_prefix(const char *level, const char *func)
{
    unsigned int us = log_time_running ? plat_get_us_tick() - log_time_base : 0;

    /* No width: the firmware printf only pads with zeros */
    printf("[%u.%03u] %s %s: ", us / 1000, us % 1000, level, func);
}
