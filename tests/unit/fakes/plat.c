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

unsigned int plat_get_us_tick(void)
{
    unsigned int tick = fake_us_tick;

    fake_us_tick += fake_us_tick_step;
    return tick;
}

void plat_wdog_kick(void)
{
    fake_wdog_kicks++;
}
