
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
arch_jump:
    mov r5, r0 /* Jump address */

    mov r0, r1
    mov r1, r2
    mov r2, r3
    ldr r3, [sp] /* p3, the fifth argument, is on the stack */

    isb
    dsb

    cpsid if
    mov pc, r5
    b .
