/**
 * Punch BOOT
 *
 * The architecture's part of the handover to the next stage. Every
 * architecture implements these, and a new one must give the same
 * guarantees. A boot driver calls arch_disable_mmu(), then arch_jump(); the
 * next stage starts with:
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

#include <stdint.h>

void arch_init(void);

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
