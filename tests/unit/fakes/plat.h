/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Controls for the fake platform API.
 */

#ifndef TESTS_UNIT_FAKES_PLAT_H
#define TESTS_UNIT_FAKES_PLAT_H

#define FAKE_US_TICK_DEFAULT 0x12345678

/* Value returned by the next plat_get_us_tick() */
extern unsigned int fake_us_tick;
/* Added to fake_us_tick on every plat_get_us_tick() call, 0 stops the clock */
extern unsigned int fake_us_tick_step;
/* Number of plat_wdog_kick() calls */
extern unsigned int fake_wdog_kicks;
/* fake_us_tick at the last kick, and the longest stretch between two kicks.
 * A test that measures the stretch sets both first. */
extern unsigned int fake_wdog_kick_tick;
extern unsigned int fake_wdog_max_gap_us;
/* Number of plat_lockdown() calls, and what it returns */
extern unsigned int fake_lockdown_calls;
extern int fake_lockdown_rc;
/* fake_wdog_kicks when plat_lockdown() was last called */
extern unsigned int fake_lockdown_wdog_kicks;

#endif // TESTS_UNIT_FAKES_PLAT_H
