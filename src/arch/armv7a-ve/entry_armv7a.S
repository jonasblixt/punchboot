#include "regs.h"

.section .irqvectors
_irqvectors:
    ldr pc, = _reset
    ldr pc, = hang
    ldr pc, = hang
    ldr pc, = hang
    ldr pc, = hang
    ldr pc, = hang
    ldr pc, = hang
    ldr pc, = hang

.section .text
.global _reset
_reset:

  ldr r4,=5
  ldr r5,=#0x020E00FC
  str r4,[r5]

  ldr r4,=#0x4000
  ldr r5,=#0x020A0004
  str r4,[r5]

  ldr r4,=#0x4000
  ldr r5,=#0x020A0000
  str r4,[r5]


    /* Install IRQ Vector's */
    ldr     r0, __irq_vectors_start
    mov     r1, #0x0000
    ldmia   r0!,{r2, r3, r4, r5, r6, r7, r8, r9}
    stmia   r1!,{r2, r3, r4, r5, r6, r7, r8, r9}

    /* Disable interrupts */
	cpsid	if

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

    /*  Enable I-Cache */
	mrc	p15, 0, r0, c1, c0, 0
	ldr	r1, __sctlr_mmu_clear
	ldr	r2, __sctlr_mmu_set
	and	r0, r0, r1
	orr	r0, r0, r2
	mcr	p15, 0, r0, c1, c0, 0

    ldr sp, =_stack_end


    bl pb_main
    b .

__zero_region_start:
    .word _zero_region_start
__zero_region_end:
    .word _zero_region_end
__irq_vectors_start:
    .word _irq_vectors_start
__sctlr_mmu_clear:
	.word ~(SCTLR_A_MASK)
__sctlr_mmu_set:
	.word (SCTLR_I_MASK)
hang:
    b .


.global __aeabi_uldivmod
__aeabi_uldivmod:
    b .
