
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

.section .text

.globl arch_jump_linux_dt
arch_jump_linux_dt:

    mov r4, r0 /* Jump address */
    mov r5, r1 /* Device tree address */

    mov r0, #0
    mov r1, #0xFFFFFFFF
    mov r2, r5
    mov r3, #0

    mov pc, r4
    b .


.globl arch_jump_atf
arch_jump_atf:

    mov r4, r0 /* Jump address */
    mov r5, r1 /* BL31 image parameters struct */

    mov r0, r1
    ldr r1, =0
    ldr r2, =0
    ldr r3, =0

    mov pc, r4
    b .

