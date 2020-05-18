/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <config.h>
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

    /* Disable MMU */
    mrc p15, 0, r1, c1, c0, 0
    bic r1, r1, #0x1
    mcr p15, 0, r1, c1, c0, 0

    /* Enable I$ and D$ */
    mrc     p15, 0, r12, c1, c0, 0
    orr     r12, r12, #(1<<12)
    orr     r12, r12, #(1<<2)
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
    cpsid   i,#0x12       /* irq */
    mov     sp, r1

    cpsid   i,#0x11       /* fiq */
    mov     sp, r1

    cpsid   i,#0x17       /* abort */
    mov     sp, r1

    cpsid   i,#0x1b       /* undefined */
    mov     sp, r1

    cpsid   i,#0x1f       /* system */
    mov     sp, r1

    cpsid   i,#0x13       /* supervisor */
    mov     sp, r1

    bl pb_main
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
