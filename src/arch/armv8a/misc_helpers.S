/*
 * Copyright (c) 2013-2019, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch/arch.h>
#include <arch/armv8a/asm_macros.S>

	.globl	disable_mmu_el3
	.globl	disable_mmu_icache_el3
	.globl	dcsw_op_all

/* ---------------------------------------------------------------------------
 * Clean and/or invalidate the entire data/unified cache hierarchy by set/way,
 * from L1 down to the point specified by the caller (LoC or LoUIS). The
 * operation type (DCISW / DCCISW / DCCSW) is passed in x0.
 * ---------------------------------------------------------------------------
 */

	/*
	 * This macro can be used for implementing various data cache operations
	 * `by set/way`. x0 holds the operation type (as defined in arch.h), and
	 * the macro derives x3 (last cache level to operate on) and x9
	 * (clidr_el1) before branching into the main loop.
	 */
	.macro	dcsw_op shift, fw, ls
	mrs	x9, clidr_el1
	ubfx	x3, x9, \shift, \fw
	lsl	x3, x3, \ls
	mov	x10, xzr
	b	do_dcsw_op
	.endm

func do_dcsw_op
	cbz	x3, exit
	adr	x14, dcsw_loop_table
	add	x14, x14, x0, lsl #5
	mov	x0, x9
	mov	w8, #1
loop1:
	add	x2, x10, x10, lsr #1
	lsr	x1, x0, x2
	and	x1, x1, #7
	cmp	x1, #2
	b.lo	level_done

	msr	csselr_el1, x10
	isb
	mrs	x1, ccsidr_el1
	and	x2, x1, #7
	add	x2, x2, #4
	ubfx	x4, x1, #3, #10
	clz	w5, w4
	lsl	w9, w4, w5
	lsl	w16, w8, w5
	orr	w9, w10, w9
	ubfx	w6, w1, #13, #15
	lsl	w17, w8, w2
	dsb	sy
	br	x14

	.macro	dcsw_loop _op
loop2_\_op:
	lsl	w7, w6, w2

loop3_\_op:
	orr	w11, w9, w7
	dc	\_op, x11
	subs	w7, w7, w17
	b.hs	loop3_\_op

	subs	x9, x9, x16
	b.hs	loop2_\_op

	b	level_done
	.endm

level_done:
	add	x10, x10, #2
	cmp	x3, x10
	b.hi	loop1
	msr	csselr_el1, xzr
	dsb	sy
	isb
exit:
	ret
endfunc do_dcsw_op

dcsw_loop_table:
	dcsw_loop isw
	dcsw_loop cisw
	dcsw_loop csw

func dcsw_op_all
	dcsw_op #LOC_SHIFT, #CLIDR_FIELD_WIDTH, #LEVEL_SHIFT
endfunc dcsw_op_all

/* ---------------------------------------------------------------------------
 * Disable the MMU at EL3
 * ---------------------------------------------------------------------------
 */

func disable_mmu_el3
	mov	x1, #(SCTLR_M_BIT | SCTLR_C_BIT)
do_disable_mmu_el3:
	mrs	x0, sctlr_el3
	bic	x0, x0, x1
	msr	sctlr_el3, x0
	isb	/* ensure MMU is off */
	dsb	sy
	ret
endfunc disable_mmu_el3


func disable_mmu_icache_el3
	mov	x1, #(SCTLR_M_BIT | SCTLR_C_BIT | SCTLR_I_BIT)
	b	do_disable_mmu_el3
endfunc disable_mmu_icache_el3
