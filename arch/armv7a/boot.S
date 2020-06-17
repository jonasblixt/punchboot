
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
    mov r6, r1
    mov r7, r2
    mov r8, r3
    mov r9, r4

    cpsid    if

    bl armv7a_cp15_disable_mmu
    bl arch_disable_cache

    mov r0, r6
    mov r1, r7
    mov r2, r8
    mov r3, r9

    isb
    dsb
    mov pc, r5
    b .

