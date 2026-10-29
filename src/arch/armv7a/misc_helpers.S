/*
 * Copyright (c) 2016-2019, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch/arch.h>
#include <arch/armv7a/asm_macros.S>

	.globl	disable_mmu_secure

/* ---------------------------------------------------------------------------
 * Disable the MMU in Secure State
 * ---------------------------------------------------------------------------
 */

func disable_mmu_secure
	mov	r1, #(SCTLR_M_BIT | SCTLR_C_BIT)
do_disable_mmu:
#if ERRATA_A9_794073
	stcopr	r0, BPIALL
	dsb
#endif
	ldcopr	r0, SCTLR
	bic	r0, r0, r1
	stcopr	r0, SCTLR
	isb				// ensure MMU is off
	dsb	sy
	bx	lr
endfunc disable_mmu_secure
