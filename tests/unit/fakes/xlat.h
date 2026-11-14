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

#include <pb/mmu.h>
#include <stddef.h>
#include <stdint.h>

/* The number of times the MMU was enabled, and the length of the register
 * log (regs.h) when it last was. A test sets the first to 0 */
extern unsigned int fake_mmu_enables;
extern size_t fake_mmu_enabled_at;
/* The arch's enable that ran last: "el3" (ARMv8-A) or "svc_mon" (ARMv7-A),
 * NULL before any */
extern const char *fake_mmu_enabled_by;

/* The regions the port added since its last reset_xlat_tables(), in the
 * order it added them, mmap_add()'s included */
#define FAKE_MMAP_MAX 32

struct fake_mmap_region {
    uintptr_t base;
    size_t size;
    unsigned int attr;
};

extern struct fake_mmap_region fake_mmap[FAKE_MMAP_MAX];
extern size_t fake_mmap_len;

/* The link script's symbols for pb's sections, in the test's memory */
#define FAKE_PB_MEMORY_SIZE 0xc000
extern char _pb_start[], _code_start[], _code_end[], _data_region_start[], _data_region_end[];
extern char _ro_data_region_start[], _ro_data_region_end[], _stack_start[], _stack_end[];
extern char _zero_region_start[], _pb_end[], _buffers_start[], _buffers_end[];

/* The region at base of that size, NULL when none was added */
const struct fake_mmap_region *fake_mmap_find(uintptr_t base, size_t size);

/* The number of regions that addr is in */
size_t fake_mmap_covering(uintptr_t addr);

/* platform.md 5.2's layer 4 and layer 1: the rest of pb and the buffers are
 * two regions, RW and execute never, with nothing over the gap between
 * them, and the board's DRAM is one, the same */
void fake_mmu_check_pb_map(uintptr_t dram_base, uintptr_t dram_end);

/* The regions added are those of 'expected', each once, in any order:
 * xlat_tables sorts them, so the order they are added in doesn't change the
 * tables, as long as no two have the same base and size, which it checks
 * too (platform.md 5.2) */
void fake_mmap_check_equal(const struct fake_mmap_region *expected, size_t length);

/* pb's six regions of layer 4 (platform.md 5.2), as src/mmu.c adds them, in
 * out[]: the number of entries */
size_t fake_mmap_pb_regions(struct fake_mmap_region *out);

/* The board's mmap (board.md 2.8) in fakes/board.c, which the test fills;
 * empty (zeroes, PB_MAP_END) as it starts */
#define FAKE_BOARD_MMAP_MAX 8
extern struct pb_mmap_region fake_board_mmap[FAKE_BOARD_MMAP_MAX];

/* xlat_tables declares it for the AArch64 compiler only */
void enable_mmu_el3(unsigned int flags);

#endif // TESTS_UNIT_FAKES_XLAT_H
