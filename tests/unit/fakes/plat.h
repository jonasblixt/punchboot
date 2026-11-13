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

#include <pb/plat.h>

#define FAKE_US_TICK_DEFAULT 0x12345678

/* A platform where nothing failed: no TrustZone, the watchdog armed, a
 * crypto engine that works, the ROM's logs found */
#define FAKE_PLAT_STATUS_DEFAULT             \
    {                                        \
        .tz_dram_filter = PLAT_TZ_DRAM_NONE, \
        .wdog_armed = true,                  \
        .crypto_engine = true,               \
        .rom_log = true,                     \
    }

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
/* What plat_get_status() answers, and the number of its calls */
extern struct plat_status fake_plat_status;
extern unsigned int fake_plat_status_calls;
/* What plat_get_boot_source() returns, -PB_ERR_NOT_SUPPORTED by default, the
 * kind it answers with PB_OK, and the number of its calls */
extern int fake_boot_source_rc;
extern enum pb_boot_source_kind fake_boot_source;
extern unsigned int fake_boot_source_calls;

#endif // TESTS_UNIT_FAKES_PLAT_H
