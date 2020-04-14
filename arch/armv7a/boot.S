
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

    mov r4, r0 /* Jump address */
    mov r5, r1 /* BL31 image parameters struct */

    mov r0, r1
    ldr r1, =0
    ldr r2, =0
    ldr r3, =0

    mov pc, r4
    b .

