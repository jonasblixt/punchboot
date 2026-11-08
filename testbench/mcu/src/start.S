/*
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The vector table and the reset handler. The reset handler saves what pb
 * left in the registers before anything changes them, zeroes .bss and
 * calls main(). Every other exception and interrupt goes to
 * unexpected_exception(), which reports it: the image runs without
 * interrupts, so any of them means pb left something enabled.
 */

    .syntax unified
    .thumb

    .section .vectors, "a"
    .global vectors
vectors:
    .word _stack_top
    .word reset_handler
    .rept (16 - 2 + APP_NUM_IRQS)
    .word exception_entry
    .endr

    .text
    .thumb_func
    .global reset_handler
reset_handler:
    /* r0-r3 first, r12 is free to use */
    ldr r12, =entry
    str r0, [r12, #0]
    str r1, [r12, #4]
    str r2, [r12, #8]
    str r3, [r12, #12]
    mrs r0, msp
    str r0, [r12, #16]
    mrs r0, psp
    str r0, [r12, #20]
    mrs r0, control
    str r0, [r12, #24]
    mrs r0, primask
    str r0, [r12, #28]
    mrs r0, faultmask
    str r0, [r12, #32]
    mrs r0, basepri
    str r0, [r12, #36]

    /* .bss; .data is in place, pb loaded the whole image to RAM */
    ldr r0, =_bss_start
    ldr r1, =_bss_end
    movs r2, #0
1:  cmp r0, r1
    bhs 2f
    str r2, [r0], #4
    b 1b
2:  bl main
3:  b 3b

    .thumb_func
exception_entry:
    mrs r0, ipsr
    b unexpected_exception
