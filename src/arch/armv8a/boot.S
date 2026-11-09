/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <arch/armv8a/include/arch/arch.h>
#include <arch/armv8a/asm_macros.S>

.section .text

.global arch_jump
arch_jump:
    mov x8, x0 /* Jump address */

    mov x0, x1
    mov x1, x2
    mov x2, x3
    mov x3, x4

    ic iallu  /* Invalidate the whole I$ */
    dsb sy    /* Ensure completion of cache op */

    br x8
    b .
