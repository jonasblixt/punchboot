
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
    mov r3, r4

    mcr p15, 0, r0, c7, c5, 0
    mcr p15, 0, r0, c7, c5, 6
    isb
    dsb
    mov pc, r5
    b .

