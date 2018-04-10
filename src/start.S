#include "pb.h"


	.section .expvect, "ax", %progbits
	.globl _start


_start:
    add	r0, pc, #-0x8
	cpsid	if

    ldr	r1, __code_start
	sub	r6, r0, r1		/* r1 -> Load Start - Exec Start */
	/*
	 * r6 -> offset between binary execution and load addresses
	 * We need to ensure that when we jump to reset code, we are executing
	 * from intended execution address. If necessary do relocation from
	 * load memory to execution memory.
	 */
	ldr	r1, __reloc_region_start	/* r1 -> execution address of reloc_region_start */
	ldr	r2, __reloc_region_end
	sub	r2, r2, r1		/* r2 -> reloc_region size */
	add	r0, r1, r6		/* r0 -> load address of reloc_region start */
	bl	_reloc_region

	/*
	 * Manually zero out the zero region (bss + heap)
	 */
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



    /*
	 * Enable I-Cache
	 */
	mrc	p15, 0, r0, c1, c0, 0
	ldr	r1, __sctlr_mmu_clear
	ldr	r2, __sctlr_mmu_set
	and	r0, r0, r1
	orr	r0, r0, r2
	mcr	p15, 0, r0, c1, c0, 0

_jump_to_exec:
	ldr	pc, __reset

__code_start:
	.word _code_start
__reloc_region_start:
	.word _reloc_region_start
__reloc_region_end:
	.word _reloc_region_end
__zero_region_start:
	.word _zero_region_start
__zero_region_end:
	.word _zero_region_end
__heap_start:
	.word _heap_start
__heap_end:
	.word _heap_end
__sctlr_mmu_clear:
	.word ~(SCTLR_A_MASK)
__sctlr_mmu_set:
	.word (SCTLR_I_MASK)

	/* 
	 * Copies data from source to destination taking care of even
	 * overlapping regions
	 * Arguments:
	 *  r0 -> source address
	 *  r1 -> destination address
	 *  r2 -> byte count
	 * Unmodified gprs: r4, r5, r6, r11, r12
	 */
_reloc_region:
	mov	r3, #0
	cmp	r0, r1
	beq	_reloc_done
	blt	_rev_copy
_fwd_loop:
	cmp	r3, r2
	bge	_reloc_done
	ldmia	r0!, {r7 - r10}
	stmia	r1!, {r7 - r10}
	add	r3, r3, #16
	b	_fwd_loop
_rev_copy:
	add	r0, r0, r2
	add	r1, r1, r2
_rev_loop:
	cmp	r3, r2
	bge	_reloc_done
	ldmdb	r0!, {r7 - r10}
	stmdb	r1!, {r7 - r10}
	add	r3, r3, #16
	b	_rev_loop
_reloc_done:
	bx	lr


	.section .expvect, "ax", %progbits
	.align 5		/* Required for VBAR */
	.globl _start_vect
_start_vect:	
	ldr	pc, __reset
	ldr	pc, __undefined_instruction
	ldr	pc, __software_interrupt
	ldr	pc, __prefetch_abort
	ldr	pc, __data_abort
	ldr	pc, __not_used
	ldr	pc, __irq
	ldr	pc, __fiq
__reset:
	.word _reset
__undefined_instruction:
	.word _undefined_instruction
__software_interrupt:
	.word _software_interrupt
__prefetch_abort:
	.word _prefetch_abort
__data_abort:
	.word _data_abort
__not_used:
	.word _not_used
__irq:
	.word _irq
__fiq:
	.word _fiq
	.global _end_vect
_end_vect:

__svc_stack_end:
	.word _svc_stack_end
__und_stack_end:
	.word _und_stack_end
__abt_stack_end:
	.word _abt_stack_end
__irq_stack_end:
	.word _irq_stack_end
__fiq_stack_end:
	.word _fiq_stack_end
__usr_stack_end:
	.word _usr_stack_end

_reset:
	/* Clear a register for temporary usage */
	mov	r8, #0
	/* Disable IRQ & FIQ */
	cpsid if
  ldr r4,=5
  ldr r5,=#0x020E00FC
  str r4,[r5]

  ldr r4,=#0x4000
  ldr r5,=#0x020A0004
  str r4,[r5]

  ldr r4,=#0x4000
  ldr r5,=#0x020A0000
  str r4,[r5]

  @ ------ Configure UART1 PAD's ----------------------------------------------
  ldr r4,=0
  ldr r5,=#0x020E0094
  str r4,[r5]

  ldr r4,=0
  ldr r5,=#0x020E0098
  str r4,[r5]


  ldr r4,=UART_PAD_CTRL
  ldr r5,=#0x020E0320
  str r4,[r5]

  ldr r4,=UART_PAD_CTRL
  ldr r5,=#0x020E0324
  str r4,[r5]

  @ ------- END OF UART1 PAD CONFIG -------------------------------------------


  ldr r4,=0
  ldr r5,=UART_PHYS+UCR1
  str r4,[r5]

  ldr r4,=0
  ldr r5,=UART_PHYS+UCR2
  str r4,[r5]

uart_reset_wait:
  ldr r5,=UART_PHYS+UCR2
  ldr r4,[r5]
  and r9,r4,#(UCR2_SRST) @ wait until SRST bit is set
  cmp r9,#0
  beq uart_reset_wait

  ldr r4,=#0x0704
  ldr r5,=UART_PHYS+UCR3
  str r4,[r5]

  ldr r4,=#0x8000
  ldr r5,=UART_PHYS+UCR4
  str r4,[r5]

  ldr r4,=#0x002b
  ldr r5,=UART_PHYS+UESC
  str r4,[r5]

  ldr r4,=0
  ldr r5,=UART_PHYS+UTIM
  str r4,[r5]


  ldr r4,=0
  ldr r5,=UART_PHYS+UTS
  str r4,[r5]

  ldr r4,=(4 << 7)
  ldr r5,=UART_PHYS+UFCR
  str r4,[r5]


  ldr r4,=#0x0f
  ldr r5,=UART_PHYS+UBIR
  str r4,[r5]


  ldr r4,=80000000 / (2 * 115200)
  ldr r5,=UART_PHYS+UBMR
  str r4,[r5]

  ldr r4,=(UCR2_WS | UCR2_IRTS | UCR2_RXEN | UCR2_TXEN | UCR2_SRST)
  ldr r5,=UART_PHYS+UCR2
  str r4,[r5]


  ldr r4,=UCR1_UARTEN
  ldr r5,=UART_PHYS+UCR1
  str r4,[r5]

  @ ---- END OF UART CONFIG ---------------------------------------------------

  ldr r4,=#0x4000
  ldr r5,=#0x020A0000
  str r4,[r5]

  ldr r4,='A'
  ldr r5,=UART_PHYS+UTXD
  str r4,[r5]

  ldr sp, =_svc_stack_end
  bl punch_boot_init



  hang:
    b .


_undefined_instruction: b .
_software_interrupt: b .
_prefetch_abort: b .
_data_abort: b .
_not_used: b .
_irq: b .
_fiq: b .







