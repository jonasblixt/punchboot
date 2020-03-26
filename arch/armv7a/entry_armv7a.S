/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include "armv7a.h"

.global pb_vector_table
.section .vectors, "ax"
.align 5
pb_vector_table:
    b    pb_entry
    b    report_exception    /* Undef */
    b    report_exception    /* SVC call */
    b    report_exception    /* Prefetch abort */
    b    report_exception    /* Data abort */
    b    report_exception    /* Reserved */
    b    report_exception    /* IRQ */
    b    report_exception    /* FIQ */

.section .text
.global pb_entry
pb_entry:
    /* Disable interrupts */
    cpsid    if

    /* Program vector table */
    ldr    r0, =pb_vector_table
    mcr p15, 0, r0, c12, c0, 0
    isb

    /*  Enable I-Cache */
    mrc    p15, 0, r0, c1, c0, 0
    ldr    r1, __sctlr_mmu_clear
    ldr    r2, __sctlr_mmu_set
    and    r0, r0, r1
    orr    r0, r0, r2
    mcr    p15, 0, r0, c1, c0, 0

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

    ldr sp, =_stack_end
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

.section .text
.global report_exception
report_exception:
    b .
