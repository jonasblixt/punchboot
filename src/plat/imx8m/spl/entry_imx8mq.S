/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8MQ's SPL entry, a HABv4 plugin (8MQ RM 6.1.7): the ROM calls it
 * as BOOLEAN plugin(void **start, size_t *bytes, UINT32 *ivt_offset), at EL3,
 * on its own stack and with its MMU and caches as it has them. The plugin
 * returns to it. The ROM's callee-saved registers and its stack pointer are
 * kept in .data (not .bss, which is zeroed here), the SPL runs on its own
 * stack, and the ROM's exception vectors stay.
 */

.section .spl_entry, "ax"
.global spl_plugin_entry
spl_plugin_entry:
    adr     x9, rom_context
    stp     x19, x20, [x9, #0]
    stp     x21, x22, [x9, #16]
    stp     x23, x24, [x9, #32]
    stp     x25, x26, [x9, #48]
    stp     x27, x28, [x9, #64]
    stp     x29, x30, [x9, #80]
    mov     x10, sp
    str     x10, [x9, #96]

    /* The arguments, across the .bss loop */
    mov     x19, x0
    mov     x20, x1
    mov     x21, x2

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

    mov     x0, x19
    mov     x1, x20
    mov     x2, x21
    bl      spl_plugin_main

    /* Back to the ROM with w0 */
    adr     x9, rom_context
    ldr     x10, [x9, #96]
    mov     sp, x10
    ldp     x19, x20, [x9, #0]
    ldp     x21, x22, [x9, #16]
    ldp     x23, x24, [x9, #32]
    ldp     x25, x26, [x9, #48]
    ldp     x27, x28, [x9, #64]
    ldp     x29, x30, [x9, #80]
    ret

.ltorg

.section .data.rom_context, "aw"
.balign 16
rom_context:
    .fill 14, 8, 0
