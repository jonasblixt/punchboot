/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include "armv7a.h"

.section .text
.global _reset
_reset:
    /* Disable interrupts */
	cpsid	if

    /*  Enable I-Cache */
	mrc	p15, 0, r0, c1, c0, 0
	ldr	r1, __sctlr_mmu_clear
	ldr	r2, __sctlr_mmu_set
	and	r0, r0, r1
	orr	r0, r0, r2
	mcr	p15, 0, r0, c1, c0, 0

    /* Clear BSS and HEAP */
	ldr	r1, __zero_region_start
	ldr	r2, __zero_region_end
	mov	r7, #0x0
	mov	r8, #0x0
	mov	r9, #0x0
	mov	r10, #0x0
_zeroing_loop:
	cmp	r1, r2
	bge	_zeroing_done
	stmia	r1!, {r7 - r10}
	b	_zeroing_loop
_zeroing_done:

    ldr sp, =_stack_end

    bl pb_main
    b .

__zero_region_start:
    .word _zero_region_start
__zero_region_end:
    .word _zero_region_end
__sctlr_mmu_clear:
	.word ~(SCTLR_A_MASK)
__sctlr_mmu_set:
	.word (SCTLR_I_MASK)
hang:
    b .


