/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <config.h>
#include <arch/arch.h>
#include "armv7a.h"

.global pb_vector_table
.section .vectors, "ax"
.align 5
pb_vector_table:
    b    pb_entry
    b    undef_exception    /* Undef */
    b    svc_exception    /* SVC call */
    b    prefetch_exception    /* Prefetch abort */
    b    abort_exception    /* Data abort */
    b    .                  /* Reserved */
    b    irq_exception    /* IRQ */
    b    fiq_exception    /* FIQ */

.section .text
.global pb_entry
pb_entry:
    /* A Cortex-A7 (MIDR 0x41xFC07x) needs ACTLR.SMP set before the caches and
     * the MMU are enabled and before any cache or TLB maintenance: with it
     * clear the caches are off whatever SCTLR.C says, and loads to cacheable
     * memory aren't cached (Cortex-A7 MPCore TRM, DDI0464F, 2.4 and 4.3.31,
     * F-135). What the ROM left is kept for arch_init()'s log. pb runs in
     * the Secure state, where ACTLR is writable. Uses r0, r1 */
    mrc     p15, 0, r0, c0, c0, 0
    ldr     r1, =0xff00fff0
    and     r0, r0, r1
    ldr     r1, =0x4100c070
    cmp     r0, r1
    bne     1f
    mrc     p15, 0, r0, c1, c0, 1
    ldr     r1, =armv7a_rom_actlr
    str     r0, [r1]
    orr     r0, r0, #(1 << 6)
    mcr     p15, 0, r0, c1, c0, 1
    isb
1:

    /* Disable MMU */
    mrc p15, 0, r1, c1, c0, 0
    bic r1, r1, #SCTLR_M_BIT
    mcr p15, 0, r1, c1, c0, 0

    /* Empty the data and unified caches first, by set/way to the level of
     * coherency (F-207). Their contents are UNKNOWN until invalidated (Arm
     * ARM), and the 6UL's ROM used them and only turned them off before it
     * left (RM 8.4.4): a valid line it left would answer for memory written
     * since. With the MMU off pb's data accesses are Strongly-ordered and
     * allocate nothing, so nothing of pb's is in them until the MMU is on
     * (plat_mmu_init()). Uses r0-r6 and r9-r11, no stack */
    bl      invalidate_cache_v7

    /* Enable I$ and D$ */
    mrc     p15, 0, r12, c1, c0, 0
    orr     r12, r12, #SCTLR_I_BIT
    orr     r12, r12, #SCTLR_C_BIT
    mcr     p15, 0, r12, c1, c0, 0

    /* Program vector table */
    ldr    r0, =pb_vector_table
    mcr p15, 0, r0, c12, c0, 0
    isb

    /* Clear BSS and HEAP */
    ldr    r1, __zero_region_start
    ldr    r2, __zero_region_end
    mov    r7, #0x0
    mov    r8, #0x0
    mov    r9, #0x0
    mov    r10, #0x0
_zeroing_loop:
    cmp    r1, r2
    bge    _zeroing_done
    stmia    r1!, {r7 - r10}
    b    _zeroing_loop
_zeroing_done:

    ldr r1, =_stack_end
    cpsid   i,#MODE32_irq
    mov     sp, r1

    cpsid   i,#MODE32_fiq
    mov     sp, r1

    cpsid   i,#MODE32_abt
    mov     sp, r1

    cpsid   i,#MODE32_und
    mov     sp, r1

    cpsid   i,#MODE32_sys
    mov     sp, r1

    cpsid   i,#MODE32_svc
    mov     sp, r1

    bl main
    b .


__zero_region_start:
    .word _zero_region_start
__zero_region_end:
    .word _zero_region_end
__sctlr_mmu_clear:
    .word ~(SCTLR_A_MASK)
__sctlr_mmu_set:
    .word (SCTLR_I_MASK)
hang:
    b .

undef_exception:
    mov r0, #0
    bl exception
    b .

svc_exception:
    mov r0, #1
    bl exception
    b .

prefetch_exception:
    mov r0, #2
    bl exception
    b .

abort_exception:
    mov r0, #3
    bl exception
    b .

irq_exception:
    mov r0, #4
    bl exception
    b .

fiq_exception:
    mov r0, #5
    bl exception
    b .
