/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * What a port's MMU set up needs to link on the host: xlat_tables' functions,
 * which build nothing, the arch's enable, which is counted, and the link
 * script's symbols for pb's sections.
 */

#include "xlat.h"

#include <stddef.h>
#include <stdint.h>
#include <xlat_tables.h>

#include "regs.h"

unsigned int fake_mmu_enables;
size_t fake_mmu_enabled_at;

/* The link script's: pb's sections */
char _code_start[1], _code_end[1];
char _data_region_start[1], _data_region_end[1];
char _ro_data_region_start[1], _ro_data_region_end[1];
char _stack_start[1], _stack_end[1];
char _zero_region_start[1], _no_init_end[1];

void reset_xlat_tables(void)
{
}

void mmap_add_region(unsigned long long base_pa, uintptr_t base_va, size_t size, unsigned int attr)
{
    (void)base_pa;
    (void)base_va;
    (void)size;
    (void)attr;
}

void mmap_add(const mmap_region_t *mm)
{
    (void)mm;
}

void init_xlat_tables(void)
{
}

static void mmu_enabled(void)
{
    fake_mmu_enables++;
    fake_mmu_enabled_at = fake_reg_log_len;
}

void enable_mmu_svc_mon(unsigned int flags)
{
    (void)flags;
    mmu_enabled();
}

void enable_mmu_el3(unsigned int flags)
{
    (void)flags;
    mmu_enabled();
}
