/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8M Plus's SPL entry (D-088): the v2 ROM jumps to it at EL3 and
 * it never returns. As pb's own entry (src/arch/armv8a/entry.S): the MMU
 * and the D-cache off, the caches emptied, then .bss, the stack, and
 * spl_main().
 */

#include <arch/arch.h>
#include <arch/armv8a/asm_macros.S>

.section .spl_entry, "ax"
.global spl_entry
spl_entry:
    mov_imm x0, ((SCTLR_RESET_VAL | SCTLR_I_BIT) & ~(SCTLR_EE_BIT | SCTLR_WXN_BIT \
            | SCTLR_SA_BIT | SCTLR_A_BIT | SCTLR_DSSBS_BIT))
    msr     sctlr_el3, x0
    isb

    /* The ROM ran with the D-cache on during its authentication (F-207):
     * no line of it may answer for memory written from here. Uses x0-x17 */
    mov     x0, #DCISW
    bl      dcsw_op_all

    ldr     x9, =_spl_bss_start
    ldr     x10, =_spl_bss_end
.L_bss:
    cmp     x9, x10
    b.hs    .L_bss_done
    str     xzr, [x9], #8
    b       .L_bss
.L_bss_done:

    ldr     x9, =_spl_stack_end
    mov     sp, x9

    bl      spl_main
    b       .

.ltorg
