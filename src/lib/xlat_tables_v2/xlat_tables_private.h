/*
 * Copyright (c) 2016-2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef XLAT_TABLES_PRIVATE_H
#define XLAT_TABLES_PRIVATE_H

#include <xlat_tables_arch.h>

/* Alias to retain compatibility with the old #define name */
#define XLAT_BLOCK_LEVEL_MIN	MIN_LVL_BLOCK_DESC

void print_mmap(void);

/* Returns the current Exception Level. The returned EL must be 1 or higher. */
unsigned int xlat_arch_current_el(void);

/*
 * Returns the bit mask that has to be ORed to the rest of a translation table
 * descriptor so that execution of code is prohibited at the given Exception
 * Level.
 */
uint64_t xlat_arch_get_xn_desc(unsigned int el);

void init_xlation_table(uintptr_t base_va, uint64_t *table,
			unsigned int level, uintptr_t *max_va,
			unsigned long long *max_pa);

#endif /* XLAT_TABLES_PRIVATE_H */
