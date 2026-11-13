/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Controls for the fake MMU set up (xlat.c), for the ports' tests.
 */

#ifndef TESTS_UNIT_FAKES_XLAT_H
#define TESTS_UNIT_FAKES_XLAT_H

#include <stddef.h>

/* The number of times the MMU was enabled, and the length of the register
 * log (regs.h) when it last was. A test sets the first to 0 */
extern unsigned int fake_mmu_enables;
extern size_t fake_mmu_enabled_at;

/* xlat_tables declares it for the AArch64 compiler only */
void enable_mmu_el3(unsigned int flags);

#endif // TESTS_UNIT_FAKES_XLAT_H
