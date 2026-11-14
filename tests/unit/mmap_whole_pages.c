/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * platform.md 5.2: a board region's base and size are whole pages, which
 * pb/mmu.h's macros check in the build. Compiled only: as it is, it builds;
 * with BAD_BASE, BAD_SIZE or EMPTY it must not (CMakeLists.txt).
 */

#include <pb/mmu.h>

#if defined(BAD_BASE)
#define BASE 0x08000800
#else
#define BASE 0x08000000
#endif

#if defined(BAD_SIZE)
#define SIZE 0x00010800
#elif defined(EMPTY)
#define SIZE 0
#else
#define SIZE 0x00010000
#endif

const struct pb_mmap_region board_mmap[] = {
    PB_MAP_DEVICE(BASE, SIZE),
    PB_MAP_MEMORY_RO(0x60000000, 0x01000000),
    PB_MAP_MEMORY_RW(0x70000000, 0x1000),
    PB_MAP_END,
};
