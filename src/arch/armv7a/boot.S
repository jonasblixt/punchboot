
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


.section .text

.globl arch_jump
/* void arch_jump(void *addr, void *p0, void *p1, void *p2, void *p3)
 * Masks IRQ and FIQ, invalidates the I-cache and jumps to addr with p0-p3 in
 * r0-r3 (include/pb/arch.h) */
arch_jump:
    cpsid if
    mov r5, r0 /* Jump address */

    mov r0, r1
    mov r1, r2
    mov r2, r3
    ldr r3, [sp] /* p3, the fifth argument, is on the stack */

    dsb
    mov r6, #0
    mcr p15, 0, r6, c7, c5, 0 /* ICIALLU */
    dsb
    isb

    mov pc, r5
    b .
