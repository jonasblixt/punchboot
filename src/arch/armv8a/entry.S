/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <arch/arch.h>
#include <arch/armv8a/asm_macros.S>

.macro branch_if_master, xreg1, xreg2, master_label
       mrs     \xreg1, mpidr_el1
       lsr     \xreg2, \xreg1, #32
       lsl     \xreg1, \xreg1, #40
       lsr     \xreg1, \xreg1, #40
       orr     \xreg1, \xreg1, \xreg2
       cbz     \xreg1, \master_label
.endm


.section .vectors
.global _reset
_reset:
    branch_if_master x0 x1 master_cpu

    wfe
    b .

master_cpu:

    /* Enable caches */
    mrs     x9, sctlr_el3
    orr     x9, x9, #(1<<12) /* Enable icache */
    orr     x9, x9, #(1<<2)  /* Enable dcache/ucache */
    orr     x9, x9, #(1<<3)  /* Enable Stack Alignment Check EL3 */
    bic     x9, x9, #(1<<1)  /* Disable Alignment Checking for EL3 */
    msr     sctlr_el3, x9

    mov_imm    x0, (SCTLR_RESET_VAL & ~(SCTLR_EE_BIT | SCTLR_WXN_BIT \
            | SCTLR_SA_BIT | SCTLR_A_BIT | SCTLR_DSSBS_BIT))
    msr    sctlr_el3, x0
    isb

.L__do_bss:
    ldr     x9, =_zero_region_start
    ldr     x10, =_zero_region_end
    sub     x10, x10, x9
    cbz     x10, .L__bss_loop_done
.L__bss_loop:
    sub     x10, x10, #8
    str     xzr, [x9], #8
    cbnz    x10, .L__bss_loop
.L__bss_loop_done:


    ldr x0, =_stack_end
    mov sp, x0

    ldr x0, = Vector_table_el3
    msr VBAR_EL3, x0

    mov x0, #0

    bl main
    b .

__zero_region_start:
    .word _zero_region_start
__zero_region_end:
    .word _zero_region_end

.balign 0x800
Vector_table_el3:
curr_el_sp0_sync:
mov x0, #0
bl exception
b .
.balign 0x80
curr_el_sp0_irq:
mov x0, #1
bl exception
b .
.balign 0x80
curr_el_sp0_fiq:
mov x0, #2
bl exception
b .
.balign 0x80
curr_el_sp0_serror:
mov x0, #3
bl exception
b .
.balign 0x80
curr_el_spx_sync:
bl exception_sync
b .
.balign 0x80
curr_el_spx_irq:
mov x0, #5
bl exception
b .
.balign 0x80
curr_el_spx_fiq:
mov x0, #6
bl exception
b .
.balign 0x80
curr_el_spx_serror:
mov x0, #7
bl exception
b .
.balign 0x80
lower_el_aarch64_sync:
mov x0, #8
bl exception
b .
.balign 0x80
lower_el_aarch64_irq:
mov x0, #9
bl exception
b .
.balign 0x80
lower_el_aarch64_fiq:
mov x0, #10
bl exception
b .
.balign 0x80
lower_el_aarch64_serror:
mov x0, #11
bl exception
b .
.balign 0x80
lower_el_aarch32_sync:
mov x0, #12
bl exception
b .
.balign 0x80
lower_el_aarch32_irq:
mov x0, #13
bl exception
b .
.balign 0x80
lower_el_aarch32_fiq:
mov x0, #14
bl exception
b .
.balign 0x80
lower_el_aarch32_serror:
mov x0, #15
bl exception
b .


