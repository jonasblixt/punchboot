/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Host "architecture", used when punchboot is built as a native process
 * for testing. There are no caches or MMU to manage.
 */

#ifndef ARCH_HOST_ARCH_H
#define ARCH_HOST_ARCH_H

#include <stddef.h>
#include <stdint.h>

void arch_jump(void *addr, void *p0, void *p1, void *p2, void *p3) __attribute__((noreturn));
void arch_invalidate_cache_range(uintptr_t start, size_t len);
void arch_clean_cache_range(uintptr_t start, size_t len);

#endif // ARCH_HOST_ARCH_H
