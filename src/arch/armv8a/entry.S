/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

.macro branch_if_master, xreg1, xreg2, master_label
       mrs     \xreg1, mpidr_el1
       lsr     \xreg2, \xreg1, #32
       lsl     \xreg1, \xreg1, #40
       lsr     \xreg1, \xreg1, #40
       orr     \xreg1, \xreg1, \xreg2
       cbz     \xreg1, \master_label
.endm


.section .text
.global _reset
_reset:

    branch_if_master x0 x1 master_cpu

    wfe
    b .

master_cpu:
	ldr	x0, =_bss_start
	ldr	x1, =_bss_end
clear_loop:
	str	xzr, [x0], #8
	cmp	x0, x1
	b.lo	clear_loop

    ldr x0, =_stack_end
    mov sp, x0

    mov x0, #0
    bl pb_main
    b .

