/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * What a port's MMU set up needs to link on the host: xlat_tables' functions,
 * which build nothing and note the regions, the arch's enable, which is
 * counted, and the link script's symbols for pb's sections.
 */

#include "xlat.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>
#include <pb/utils_def.h>
#include <xlat_tables.h>

#include "regs.h"

unsigned int fake_mmu_enables;
size_t fake_mmu_enabled_at;
const char *fake_mmu_enabled_by;
struct fake_mmap_region fake_mmap[FAKE_MMAP_MAX];
size_t fake_mmap_len;

/* The link script's: pb's sections in the order src/link.lds has them, a
 * page each, in memory of the test's: the code, data, rodata, the stack,
 * .bss and the RAM log; then a gap, as between the memory pb runs in and
 * its buffers on a board; then the buffers. None of it is at an address
 * the SoC or the tests map */
char fake_pb_memory[FAKE_PB_MEMORY_SIZE] __attribute__((aligned(4096))) = { 1 };

#define FAKE_PB_SYM(name, offset) ".globl " #name "\n.set " #name ", fake_pb_memory + " #offset "\n"

__asm__(FAKE_PB_SYM(_pb_start, 0x0000));
__asm__(FAKE_PB_SYM(_code_start, 0x0000));
__asm__(FAKE_PB_SYM(_code_end, 0x1000));
__asm__(FAKE_PB_SYM(_data_region_start, 0x1000));
__asm__(FAKE_PB_SYM(_data_region_end, 0x2000));
__asm__(FAKE_PB_SYM(_ro_data_region_start, 0x2000));
__asm__(FAKE_PB_SYM(_ro_data_region_end, 0x3000));
__asm__(FAKE_PB_SYM(_stack_start, 0x3000));
__asm__(FAKE_PB_SYM(_stack_end, 0x4000));
__asm__(FAKE_PB_SYM(_zero_region_start, 0x4000));
__asm__(FAKE_PB_SYM(_pb_end, 0x6000));
__asm__(FAKE_PB_SYM(_buffers_start, 0x9000));
__asm__(FAKE_PB_SYM(_buffers_end, 0xb000));

void reset_xlat_tables(void)
{
    fake_mmap_len = 0;
}

void mmap_add_region(unsigned long long base_pa, uintptr_t base_va, size_t size, unsigned int attr)
{
    /* Flat, as every region of pb's */
    assert_true(base_pa == base_va);
    assert_true(fake_mmap_len < FAKE_MMAP_MAX);
    fake_mmap[fake_mmap_len].base = base_va;
    fake_mmap[fake_mmap_len].size = size;
    fake_mmap[fake_mmap_len].attr = attr;
    fake_mmap_len++;
}

void mmap_add(const mmap_region_t *mm)
{
    for (; mm->size != 0 || mm->attr != 0; mm++)
        mmap_add_region(mm->base_pa, mm->base_va, mm->size, mm->attr);
}

void init_xlat_tables(void)
{
}

const struct fake_mmap_region *fake_mmap_find(uintptr_t base, size_t size)
{
    for (size_t n = 0; n < fake_mmap_len; n++) {
        if (fake_mmap[n].base == base && fake_mmap[n].size == size)
            return &fake_mmap[n];
    }

    return NULL;
}

size_t fake_mmap_covering(uintptr_t addr)
{
    size_t count = 0;

    for (size_t n = 0; n < fake_mmap_len; n++) {
        if (addr >= fake_mmap[n].base && addr - fake_mmap[n].base < fake_mmap[n].size)
            count++;
    }

    return count;
}

void fake_mmu_check_pb_map(uintptr_t dram_base, uintptr_t dram_end)
{
    const unsigned int rw_nox = MT_RW | MT_MEMORY | MT_EXECUTE_NEVER;
    const uintptr_t pb_rw = (uintptr_t)_zero_region_start;
    const uintptr_t pb_end = (uintptr_t)_pb_end;
    const uintptr_t buffers = (uintptr_t)_buffers_start;
    const uintptr_t buffers_end = (uintptr_t)_buffers_end;
    const struct fake_mmap_region *r;

    /* .bss and the RAM log: the rest of pb, RW, execute never */
    r = fake_mmap_find(pb_rw, pb_end - pb_rw);
    assert_non_null(r);
    assert_int_equal(r->attr, rw_nox);

    /* The buffers, the same, a region of their own */
    r = fake_mmap_find(buffers, buffers_end - buffers);
    assert_non_null(r);
    assert_int_equal(r->attr, rw_nox);

    /* Nothing maps what is between them (F-158) */
    for (uintptr_t a = pb_end; a < buffers; a += 0x1000)
        assert_int_equal(fake_mmap_covering(a), 0);

    /* The board's DRAM, where images are loaded: the same */
    r = fake_mmap_find(dram_base, dram_end - dram_base);
    assert_non_null(r);
    assert_int_equal(r->attr, rw_nox);
}

void fake_mmap_check_equal(const struct fake_mmap_region *expected, size_t length)
{
    assert_int_equal(fake_mmap_len, length);

    for (size_t n = 0; n < length; n++) {
        const struct fake_mmap_region *r = fake_mmap_find(expected[n].base, expected[n].size);

        if (r == NULL || r->attr != expected[n].attr) {
            print_error("Region %zu (0x%lx, 0x%zx, attr 0x%x) not in the map\n",
                        n,
                        (unsigned long)expected[n].base,
                        expected[n].size,
                        expected[n].attr);
            fail();
        }

        /* No other region with the same base and size */
        for (size_t m = 0; m < fake_mmap_len; m++) {
            if (&fake_mmap[m] != r && fake_mmap[m].base == r->base && fake_mmap[m].size == r->size)
                fail_msg("Two regions at 0x%lx of 0x%zx", (unsigned long)r->base, r->size);
        }
    }
}

size_t fake_mmap_pb_regions(struct fake_mmap_region *out)
{
    const unsigned int rw_nox = MT_RW | MT_MEMORY | MT_EXECUTE_NEVER;
    size_t n = 0;

#define FAKE_PB_REGION(start, end, attr_)                                                \
    out[n++] = (struct fake_mmap_region){ .base = (uintptr_t)(start),                    \
                                          .size = (uintptr_t)(end) - (uintptr_t)(start), \
                                          .attr = (attr_) }
    FAKE_PB_REGION(_code_start, _code_end, MT_RO | MT_MEMORY | MT_EXECUTE);
    FAKE_PB_REGION(_data_region_start, _data_region_end, rw_nox);
    FAKE_PB_REGION(
        _ro_data_region_start, _ro_data_region_end, MT_RO | MT_MEMORY | MT_EXECUTE_NEVER);
    FAKE_PB_REGION(_stack_start, _stack_end, rw_nox);
    FAKE_PB_REGION(_zero_region_start, _pb_end, rw_nox);
    FAKE_PB_REGION(_buffers_start, _buffers_end, rw_nox);
#undef FAKE_PB_REGION

    return n;
}

static void mmu_enabled(const char *by)
{
    fake_mmu_enables++;
    fake_mmu_enabled_at = fake_reg_log_len;
    fake_mmu_enabled_by = by;
}

void enable_mmu_svc_mon(unsigned int flags)
{
    (void)flags;
    mmu_enabled("svc_mon");
}

void enable_mmu_el3(unsigned int flags)
{
    (void)flags;
    mmu_enabled("el3");
}
