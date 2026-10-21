/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <arch/arch.h>
#include <pb/arch.h>
#include <pb/pb.h>
#include <stdio.h>
#include <stdlib.h>

void arch_init(void)
{
}

void arch_disable_mmu(void)
{
}

void arch_invalidate_cache_range(uintptr_t start, size_t len)
{
    (void)start;
    (void)len;
}

void arch_clean_cache_range(uintptr_t start, size_t len)
{
    (void)start;
    (void)len;
}

void arch_jump(void *addr, void *p0, void *p1, void *p2, void *p3)
{
    (void)p1;
    (void)p2;
    (void)p3;

    /* There is nothing to jump to on the host. Boards are expected to report
     * the boot from their 'late_boot' callback before this happens. */
    LOG_ERR("Jump to %p (%p) is not supported on the host", addr, p0);
    exit(EXIT_FAILURE);
}
