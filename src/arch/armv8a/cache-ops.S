/**
 * Punch BOOT
 *
 * armv8a cache maintenance.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * With the data cache disabled (SCTLR.C == 0) the cache is neither allocated
 * into nor looked up, but its contents are *not* discarded. So while it is off:
 *
 *   - a load returns what is in memory, even when a dirty line holds newer
 *     data, and
 *   - a store goes to memory, where the later write back of a dirty line for
 *     the same address overwrites it again.
 *
 * The stack is normal cacheable memory, which is why arch_disable_mmu() below
 * cleans the cache while it is still enabled and only ever invalidates after
 * that, and makes no memory access at all in between. Keep that property if you
 * change it: it cannot be expressed in C, where the compiler is free to spill a
 * register in the middle of the sequence.
 */

#include <arch/arch.h>
#include <arch/armv8a/asm_macros.S>

.text

/* The range operations are derived from Little Kernel. The line size comes from
 * CTR_EL0 rather than from a per platform constant, both so that it is right on
 * every core and because a stride larger than the real line size would skip
 * lines. */

.macro cache_range_op, cache op
    cbz     x1, .Lcache_range_op_done\@ // Nothing to do for an empty range
    dcache_line_size x4, x5
    add     x2, x0, x1                  // calculate the end address
    sub     x5, x4, #1
    bic     x3, x0, x5                  // align the start with a cache line
.Lcache_range_op_loop\@:
    \cache  \op, x3
    add     x3, x3, x4
    cmp     x3, x2
    blo     .Lcache_range_op_loop\@
    dsb     sy
.Lcache_range_op_done\@:
.endm

    /* void arch_clean_cache_range(uintptr_t start, size_t len); */
func (arch_clean_cache_range)
    cache_range_op dc cvac         // clean cache to PoC by MVA
    ret

    /* void arch_clean_invalidate_cache_range(uintptr_t start, size_t len); */
func (arch_clean_invalidate_cache_range)
    cache_range_op dc civac        // clean & invalidate dcache to PoC by MVA
    ret

    /* void arch_invalidate_cache_range(uintptr_t start, size_t len);
     *
     * The range must be cache line aligned and a multiple of the line size, see
     * the note on the declaration in include/pb/arch.h. A line is invalidated
     * as a whole, so for a partially covered line this discards dirty bytes
     * outside the range as well. */
func (arch_invalidate_cache_range)
    cache_range_op dc ivac         // invalidate dcache to PoC by MVA
    ret

/* ---------------------------------------------------------------------------
 * void arch_disable_mmu(void)
 *
 * Hand the memory system over to the next boot stage: write back everything we
 * have produced, then turn the MMU and both caches off, leaving the caches
 * empty.
 *
 * The ordering is load bearing, see the comment at the top of this file. The
 * clean runs with the data cache still enabled, the only cache operation after
 * the disable is a pure invalidate, and nothing in between touches memory.
 *
 * Returns with all exceptions masked; one taken inside that window would push a
 * frame with the data cache off.
 * ---------------------------------------------------------------------------
 */
func (arch_disable_mmu)
    msr     daifset, #0xf
    /* dcsw_op_all only uses x0-x17 and x18 is a temporary in the AArch64
     * procedure call standard, so holding the return address there keeps this
     * routine off the stack entirely. Same idiom as TF-A's CPU power down
     * hooks. */
    mov     x18, x30

    mov     x0, #DCCISW
    bl      dcsw_op_all                 // clean & invalidate to the PoC

    /* ===== No access to normal memory until the invalidate below ===== */

    /* Clears SCTLR.M, SCTLR.C and SCTLR.I */
    bl      disable_mmu_icache_el3

    /* Instruction fetch allocates into the unified cache levels that the clean
     * above swept, so clean lines have reappeared since. Drop them, or the next
     * stage could hit one when it enables its own cache after rewriting the
     * memory behind it. Invalidate only: a clean here would write lines back
     * over anything stored since the disable. */
    mov     x0, #DCISW
    bl      dcsw_op_all

    /* ===== End of the window ===== */

    /* The next stage runs code we have just written through the data cache, and
     * may enable its MMU without invalidating first. */
    ic      iallu
    tlbi    alle3
    dsb     sy
    isb

    mov     x30, x18
    ret
