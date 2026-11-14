/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * OP-TEE's entry without TF-A (boot.md 5.7, OP-TEE's entry_a32.S and
 * boot_save_args()): r0 the pageable part (0: none), r1 and r2 relayed to
 * the normal world, lr the normal world's entry.
 */

.section .text

.globl boot_optee_enter
/* void boot_optee_enter(uintptr_t entry, uintptr_t ns_entry, uintptr_t r1,
 *                       uintptr_t r2)
 * Masks IRQ and FIQ, invalidates the I-cache and enters OP-TEE at 'entry'
 * with r0 = 0, r1, r2, r3 = 0 and lr = 'ns_entry' (include/boot/optee.h) */
boot_optee_enter:
    cpsid if
    mov r5, r0 /* OP-TEE's entry */
    mov lr, r1 /* The normal world's */
    mov r1, r2
    mov r2, r3
    mov r0, #0
    mov r3, #0

    dsb
    mov r6, #0
    mcr p15, 0, r6, c7, c5, 0 /* ICIALLU */
    dsb
    isb

    bx r5
    b .
