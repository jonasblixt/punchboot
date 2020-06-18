/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/vm.h>
#include <arch/mmu.h>
#include <arch/armv8a/include/arch/arch.h>
#include <arch/armv8a/asm_macros.S>

tmp                     .req x9

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

    /* Enable caches */
    mrs     tmp, sctlr_el3
    orr     tmp, tmp, #(1<<12) /* Enable icache */
    orr     tmp, tmp, #(1<<2)  /* Enable dcache/ucache */
    orr     tmp, tmp, #(1<<3)  /* Enable Stack Alignment Check EL3 */
    bic     tmp, tmp, #(1<<1)  /* Disable Alignment Checking for EL3 */
    msr     sctlr_el3, tmp

    /* Zero out BSS */
	ldr	x0, =__zero_region_start
	ldr	x1, =__zero_region_end
clear_loop:
	str	xzr, [x0], #8
	cmp	x0, x1
	b.lo	clear_loop

    ldr x0, =_stack_end
    mov sp, x0

    mov x0, #0
    bl pb_main
    b .

__zero_region_start:
    .word _zero_region_start
__zero_region_end:
    .word _zero_region_end
