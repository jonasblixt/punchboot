/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <config.h>
#include "armv7a.h"
#include <arch/mmu.h>
#include <pb/vm.h>

.global pb_vector_table
.section .vectors, "ax"
.align 5
pb_vector_table:
    b    pb_entry
    b    report_exception    /* Undef */
    b    report_exception    /* SVC call */
    b    report_exception    /* Prefetch abort */
    b    report_exception    /* Data abort */
    b    report_exception    /* Reserved */
    b    report_exception    /* IRQ */
    b    report_exception    /* FIQ */

.section .text
.global pb_entry
pb_entry:

    /* Disable MMU */
    mrc p15, 0, r1, c1, c0, 0
    bic r1, r1, #0x1
    mcr p15, 0, r1, c1, c0, 0

    /* Enable I$ and D$ */
    mrc     p15, 0, r12, c1, c0, 0
    orr     r12, r12, #(1<<12)
    orr     r12, r12, #(1<<2)
    mcr     p15, 0, r12, c1, c0, 0

    /* Program vector table */
    ldr    r0, =pb_vector_table
    mcr p15, 0, r0, c12, c0, 0
    isb

    /* Clear BSS and HEAP */
    ldr    r1, __zero_region_start
    ldr    r2, __zero_region_end
    mov    r7, #0x0
    mov    r8, #0x0
    mov    r9, #0x0
    mov    r10, #0x0
_zeroing_loop:
    cmp    r1, r2
    bge    _zeroing_done
    stmia    r1!, {r7 - r10}
    b    _zeroing_loop
_zeroing_done:

    ldr sp, =_stack_end

.Lsetup_mmu:

    /* set up the mmu according to mmu_initial_mappings */

    /* load the base of the translation table and clear the table */
    ldr     r4, =initial_translation_table
    add     r4, r4, r11
        /* r4 = physical address of translation table */

    mov     r5, #0
    mov     r6, #0

    /* walk through all the entries in the translation table, setting them up */
0:
    str     r5, [r4, r6, lsl #2]
    add     r6, #1
    cmp     r6, #4096
    bne     0b

    /* load the address of the mmu_initial_mappings table and start processing */
    ldr     r5, =mmu_initial_mappings
    add     r5, r5, r11
        /* r5 = physical address of mmu initial mapping table */

.Linitial_mapping_loop:
    ldmia   r5!, { r6-r10 }
        /* r6 = phys, r7 = virt, r8 = size, r9 = flags, r10 = name */

    /* round size up to 1MB alignment */
    ubfx        r10, r6, #0, #20
    add     r8, r8, r10
    add     r8, r8, #(1 << 20)
    sub     r8, r8, #1

    /* mask all the addresses and sizes to 1MB boundaries */
    lsr     r6, #20  /* r6 = physical address / 1MB */
    lsr     r7, #20  /* r7 = virtual address / 1MB */
    lsr     r8, #20  /* r8 = size in 1MB chunks */

    /* if size == 0, end of list */
    cmp     r8, #0
    beq     .Linitial_mapping_done

    /* set up the flags */
    ldr     r10, =MMU_KERNEL_L1_PTE_FLAGS
    teq     r9, #MMU_INITIAL_MAPPING_FLAG_UNCACHED
    ldreq   r10, =MMU_INITIAL_MAP_STRONGLY_ORDERED
    beq     0f
    teq     r9, #MMU_INITIAL_MAPPING_FLAG_DEVICE
    ldreq   r10, =MMU_INITIAL_MAP_DEVICE
        /* r10 = mmu entry flags */

0:
    orr     r12, r10, r6, lsl #20
        /* r12 = phys addr | flags */

    /* store into appropriate translation table entry */
    str     r12, [r4, r7, lsl #2]

    /* loop until we're done */
    add     r6, #1
    add     r7, #1
    subs    r8, #1
    bne     0b

    b       .Linitial_mapping_loop

.Linitial_mapping_done:
    bl      .Lmmu_setup

    bl pb_main
    b .

.Lmmu_setup:
    /* Invalidate TLB. The value in r0 is ignored */
    mcr     p15, 0, r0, c8, c7, 0
    dsb     sy
    isb

    /* Write 0 to TTBCR */
    mov     r12, #0
    mcr     p15, 0, r12, c2, c0, 2
    isb

    /* Set cacheable attributes on translation walk */
    orr     r12, r4, #MMU_TTBRx_FLAGS

    /* Write ttbr with phys addr of the translation table */
    mcr     p15, 0, r12, c2, c0, 0
    isb

    /* Write DACR */
    mov     r12, #0x1
    mcr     p15, 0, r12, c3, c0, 0
    isb

    /* Read SCTLR into r12 */
    mrc     p15, 0, r12, c1, c0, 0

    /* Disable TRE/AFE */
    bic     r12, #(1<<29 | 1<<28)

    /* Turn on the MMU */
    orr     r12, #0x1

    /* Write back SCTLR */
    mcr     p15, 0, r12, c1, c0, 0
    isb

    /* Jump to virtual code address */
    ldr     pc, =1f
1:
    /* Invalidate TLB. The value in r0 is ignored */
    mcr     p15, 0, r0, c8, c7, 0
    dsb     sy
    isb

    /* assume lr was in physical memory, adjust it before returning */
    sub     lr, r11
    bx      lr

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

.section .text
.global report_exception
report_exception:
    b .
