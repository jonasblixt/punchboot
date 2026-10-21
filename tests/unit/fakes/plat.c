/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Minimal platform API for unit tests.
 */

#include <pb/plat.h>

unsigned int plat_get_us_tick(void)
{
    /* Deterministic, so tests that depend on it (e.g. the GPT disk GUID
     * generator) are reproducible. */
    return 0x12345678;
}

void plat_wdog_kick(void)
{
}
