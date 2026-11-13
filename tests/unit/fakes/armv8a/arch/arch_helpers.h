/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * What the host's tests of ARMv8-A code get for arch/arch_helpers.h: the
 * system registers are functions of the test's. The fields' names are the
 * arch's own, from src/arch/armv8a/include/arch/arch.h.
 */

#ifndef TESTS_UNIT_FAKES_ARMV8A_ARCH_HELPERS_H
#define TESTS_UNIT_FAKES_ARMV8A_ARCH_HELPERS_H

#include <stdint.h>

/* ID_AA64PFR0_EL1: which exception levels the CPU has */
uint64_t read_id_aa64pfr0_el1(void);

/* CNTFRQ_EL0: the system counter's frequency */
void write_cntfrq_el0(uint64_t value);

#endif // TESTS_UNIT_FAKES_ARMV8A_ARCH_HELPERS_H
