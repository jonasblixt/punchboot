/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX6UL's HAB header with its ROM vector table read from the test:
 * the real header reads the table at 0x100, which a host process can't map.
 * The HAB_RVT_* macros expand at their use, so they read fake_hab_rvt_read().
 */

#ifndef TESTS_UNIT_FAKES_IMX6UL_HAB_H
#define TESTS_UNIT_FAKES_IMX6UL_HAB_H

#define hab_rvt_read pb_rom_hab_rvt_read
#include_next <plat/imx6ul/hab.h>
#undef hab_rvt_read

uint32_t fake_hab_rvt_read(uintptr_t offset);
#define hab_rvt_read fake_hab_rvt_read

#endif
