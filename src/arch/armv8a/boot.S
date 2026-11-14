/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <arch/armv8a/include/arch/arch.h>
#include <arch/armv8a/asm_macros.S>

.section .text

/* void arch_jump(void *addr, void *p0, void *p1, void *p2, void *p3)
 * Masks D, A, I and F, invalidates the I-cache and jumps to addr with p0-p3
 * in x0-x3 (include/pb/arch.h) */
.global arch_jump
arch_jump:
    msr daifset, #0xf
    mov x8, x0 /* Jump address */

    mov x0, x1
    mov x1, x2
    mov x2, x3
    mov x3, x4

    ic iallu  /* Invalidate the whole I$ */
    dsb sy    /* Ensure completion of cache op */
    isb

    br x8
    b .
