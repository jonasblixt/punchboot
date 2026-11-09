/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * void arch_disable_mmu(void)
 *
 * Hand over the memory system (include/pb/arch.h): interrupts masked, the
 * MPU off, the D-cache cleaned and off, and empty. pb runs with the D-cache
 * on (write-through, src/arch/armv7m/entry.c).
 *
 * As on ARMv8-A (src/arch/armv8a/cache-ops.S): the clean runs with the
 * D-cache on, and nothing writes normal memory until the invalidate after
 * the disable. With the D-cache off a store goes to memory, and a dirty line
 * for the same address written back later would overwrite it. That can't be
 * written in C, which may spill a register to the stack at any point. The
 * registers pushed at the start are in memory after the clean.
 */

.syntax unified
.thumb

#define SCB_CCR    0xe000ed14
#define CCR_DC     (1 << 16)
#define SCB_CCSIDR 0xe000ed80
#define SCB_CSSELR 0xe000ed84
#define MPU_CTRL   0xe000ed94
#define SCB_DCISW  0xe000ef60 /* D-cache invalidate by set/way */
#define SCB_DCCISW 0xe000ef74 /* D-cache clean and invalidate by set/way */

/* Every set and way of the L1 D-cache through the register at r7.
 * r2: log2 of the line size, r3: the way's shift, r5: sets - 1,
 * r6: ways - 1. Uses r0, r1, r4 and r12, no memory. */
.macro dcache_all_sw
    mov     r0, r5
1:
    mov     r1, r6
2:
    lsl     r12, r1, r3
    lsl     r4, r0, r2
    orr     r12, r12, r4
    str     r12, [r7]
    subs    r1, r1, #1
    bpl     2b
    subs    r0, r0, #1
    bpl     1b
    dsb
.endm

.text
.global arch_disable_mmu
.type arch_disable_mmu, %function
.thumb_func
arch_disable_mmu:
    push    {r4-r7, lr}
    cpsid   i

    ldr     r0, =MPU_CTRL
    movs    r1, #0
    str     r1, [r0]
    dsb
    isb

    /* The L1 D-cache's geometry */
    ldr     r0, =SCB_CSSELR
    movs    r1, #0
    str     r1, [r0]
    dsb
    ldr     r0, =SCB_CCSIDR
    ldr     r4, [r0]
    ubfx    r5, r4, #13, #15     /* NumSets: sets - 1 */
    ubfx    r6, r4, #3, #10      /* Associativity: ways - 1 */
    and     r2, r4, #7           /* LineSize: log2(words) - 2 */
    adds    r2, r2, #4
    clz     r3, r6

    /* Off already: nothing of pb's is in it, only drop what is there */
    ldr     r0, =SCB_CCR
    ldr     r1, [r0]
    tst     r1, #CCR_DC
    beq     3f

    ldr     r7, =SCB_DCCISW
    dcache_all_sw

    /* ===== No store to normal memory until the invalidate below ===== */

    ldr     r0, =SCB_CCR
    ldr     r1, [r0]
    bic     r1, r1, #CCR_DC
    str     r1, [r0]
    dsb
    isb
3:
    ldr     r7, =SCB_DCISW
    dcache_all_sw

    /* ===== End of the window ===== */

    isb
    pop     {r4-r7, pc}
.size arch_disable_mmu, . - arch_disable_mmu
.ltorg
