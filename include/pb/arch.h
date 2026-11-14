/**
 * Punch BOOT
 *
 * The architecture's part of the handover to the next stage. Every
 * architecture implements these, and a new one must give the same
 * guarantees. A delivery calls arch_disable_mmu(), then arch_jump(): the
 * core for a board without one (src/boot/boot.c), else the board's
 * (boot/handover.h). What it starts, the next stage or TF-A's BL31, starts
 * with:
 *
 *  - the MMU (on ARMv7-M the MPU) and the D-cache off, and everything pb
 *    wrote in memory, not in the cache
 *  - the I-cache invalidated (it may be on)
 *  - interrupts masked: IRQ and FIQ on ARMv7-A, D, A, I and F on ARMv8-A,
 *    PRIMASK on ARMv7-M
 *  - the exception level pb runs at (EL3 on the i.MX8M and i.MX8X), and
 *    p0..p3 in the first four argument registers (r0-r3, x0-x3)
 *
 * What 'addr' is depends on the architecture: the entry point on ARMv7-A
 * and ARMv8-A, the vector table on ARMv7-M (VTOR is set to it, MSP and PSP
 * to its initial stack pointer, then its reset handler runs).
 */

#ifndef INCLUDE_PB_ARCH_H_
#define INCLUDE_PB_ARCH_H_

#include <stddef.h>
#include <stdint.h>

void arch_init(void);

/**
 * The D-cache by range. Drivers don't call these: what a device reads or
 * writes goes through pb/dma.h, which has the rule (drivers.md 4) and is
 * built on them.
 *
 * Each acts on every line that [start, start + len) touches: the start is
 * rounded down to a line and the end up, by the core's own line size. A line
 * the range shares with other data is cleaned or invalidated whole, so an
 * invalidate of such a line discards the neighbour's bytes that were in the
 * cache only: what is invalidated owns its lines (drivers.md 4.2). Each ends
 * with the barrier the architecture requires.
 *
 *  - clean: what the CPU wrote in the range is in memory
 *  - invalidate: the cache's copy is dropped, dirty or not, and the next
 *    read is from memory
 *  - clean and invalidate: the first, then the second, a line at a time
 */
void arch_clean_cache_range(uintptr_t start, size_t len);
void arch_invalidate_cache_range(uintptr_t start, size_t len);
void arch_clean_invalidate_cache_range(uintptr_t start, size_t len);

/**
 * Clean the D-cache, then turn it and the MMU (MPU) off and mask
 * interrupts. Returns with nothing of pb's left only in the cache.
 */
void arch_disable_mmu(void);

/** Invalidate the whole I-cache, after code was written to memory */
void arch_invalidate_icache(void);

/**
 * Masks interrupts, invalidates the I-cache and starts 'addr' with p0..p3 as
 * its arguments. Call arch_disable_mmu() first.
 */
void arch_jump(void *addr, void *p0, void *p1, void *p2, void *p3) __attribute__((noreturn));

#endif // INCLUDE_PB_ARCH_H_
