/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Minimal platform API for unit tests.
 */

#include "plat.h"
#include <pb/plat.h>

/* Deterministic, so tests that depend on it (e.g. the GPT disk GUID
 * generator) are reproducible. */
unsigned int fake_us_tick = FAKE_US_TICK_DEFAULT;
unsigned int fake_us_tick_step;
unsigned int fake_wdog_kicks;
unsigned int fake_wdog_kick_tick;
unsigned int fake_wdog_max_gap_us;
unsigned int fake_lockdown_calls;
unsigned int fake_lockdown_wdog_kicks;
int fake_lockdown_rc;
struct plat_status fake_plat_status = FAKE_PLAT_STATUS_DEFAULT;
unsigned int fake_plat_status_calls;

uint32_t plat_get_us_tick(void)
{
    uint32_t tick = fake_us_tick;

    fake_us_tick += fake_us_tick_step;
    return tick;
}

void plat_wdog_kick(void)
{
    if (fake_us_tick - fake_wdog_kick_tick > fake_wdog_max_gap_us)
        fake_wdog_max_gap_us = fake_us_tick - fake_wdog_kick_tick;
    fake_wdog_kick_tick = fake_us_tick;
    fake_wdog_kicks++;
}

void plat_get_status(struct plat_status *status)
{
    fake_plat_status_calls++;
    *status = fake_plat_status;
}

int plat_lockdown(void)
{
    fake_lockdown_calls++;
    fake_lockdown_wdog_kicks = fake_wdog_kicks;
    return fake_lockdown_rc;
}
