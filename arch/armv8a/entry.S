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
tmp2                    .req x10
wtmp2                   .req w10
idx                     .req x11
idx_shift               .req x12
page_table              .req x13
new_page_table          .req x14
phys_offset             .req x15

cpuid                   .req x19
page_table0             .req x20
page_table1             .req x21
mmu_initial_mapping     .req x22
vaddr                   .req x23
paddr                   .req x24
mapping_size            .req x25
size                    .req x26
attr                    .req x27
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

    /* enable caches so atomics and spinlocks work */
    mrs     tmp, sctlr_el1
    orr     tmp, tmp, #(1<<12) /* Enable icache */
    orr     tmp, tmp, #(1<<2)  /* Enable dcache/ucache */
    orr     tmp, tmp, #(1<<3)  /* Enable Stack Alignment Check EL1 */
    orr     tmp, tmp, #(1<<4)  /* Enable Stack Alignment Check EL0 */
    bic     tmp, tmp, #(1<<1)  /* Disable Alignment Checking for EL1 EL0 */
    msr     sctlr_el1, tmp

    /* set up the mmu according to mmu_initial_mappings */

    /* load the base of the translation table and clear the table */
    adrp    page_table1, arm64_kernel_translation_table
    add     page_table1, page_table1, #:lo12:arm64_kernel_translation_table

    /* Prepare tt_trampoline page table */
    /* Calculate pagetable physical addresses */
    adrp    page_table0, tt_trampoline
    add     page_table0, page_table0, #:lo12:tt_trampoline

    mov_imm	x0, (SCTLR_RESET_VAL & ~(SCTLR_EE_BIT | SCTLR_WXN_BIT \
            | SCTLR_SA_BIT | SCTLR_A_BIT | SCTLR_DSSBS_BIT))
    msr	sctlr_el3, x0
    isb

    mov     tmp, #0




    /* Invalidate TLB */
    tlbi    vmalle1
    dsb     sy
    isb

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
