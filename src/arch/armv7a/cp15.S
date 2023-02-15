#include <pb/asm.h>
#include <plat/defs.h>
#define CTR_DMINLINE_SHIFT	(16)
#define CTR_DMINLINE_WIDTH	(4)
#define WORD_SIZE   4
#define DCCMVAC		p15, 0, c7, c10, 1
#define DCIMVAC		p15, 0, c7, c6, 1
#define CTR		p15, 0, c0, c0, 1

.section .text
.macro ldcopr reg, coproc, opc1, CRn, CRm, opc2
mrc	\coproc, \opc1, \reg, \CRn, \CRm, \opc2
.endm

.macro stcopr reg, coproc, opc1, CRn, CRm, opc2
mcr	\coproc, \opc1, \reg, \CRn, \CRm, \opc2
.endm

/* Cache line size helpers */
.macro	dcache_line_size  reg, tmp
ldcopr	\tmp, CTR
ubfx	\tmp, \tmp, #CTR_DMINLINE_SHIFT, #CTR_DMINLINE_WIDTH
mov	\reg, #WORD_SIZE
lsl	\reg, \reg, \tmp
.endm

/* Copied from arm-trusted-firmware */
.macro do_dcache_maintenance_by_mva op, coproc, opc1, CRn, CRm, opc2
	/* Exit early if size is zero */
	cmp	r1, #0
	beq	exit_loop_\op
	dcache_line_size r2, r3
	add	r1, r0, r1
	sub	r3, r2, #1
	bic	r0, r0, r3
loop_\op:
	stcopr	r0, \coproc, \opc1, \CRn, \CRm, \opc2
	add	r0, r0, r2
	cmp	r0, r1
	blo	loop_\op
	dsb	sy
exit_loop_\op:
	bx	lr
.endm

func(armv7a_cp15_read_id_mmfr0)
    mrc p15, 0, r0, c0, c1, 4
    bx lr

func(armv7a_cp15_read_cpuid)
    mrc     p15, 0, r0, c0, c0, 5  /* Read MPIDR */
    ubfx    r0, r0, #0, #8        /* Read the bottom 8 bits */
    bx lr

func(armv7a_cp15_read_sctlr)
    mrc p15, 0, r0, c1, c0, 0   /* Read SCTLR (System Control Register) */
    bx lr

func (armv7a_cp15_disable_mmu)
    /* Disable I$ and D$ */
    mrc     p15, 0, r12, c1, c0, 0
    bic     r12, #(1<<12)
    bic     r12, #(1<<2)
    mcr     p15, 0, r12, c1, c0, 0

    mrc p15, 0, r12, c1, c0, 0
    bic r12, r12, #0x1
    mcr p15, 0, r12, c1, c0, 0

    bx lr

func (arch_invalidate_cache_range)
	do_dcache_maintenance_by_mva imvac, DCIMVAC
    bx      lr

    /* void arch_flush_cache_range(addr_t start, size_t len); */
func (arch_clean_cache_range)
	do_dcache_maintenance_by_mva cmvac, DCCMVAC
    bx lr

func (arch_disable_cache)
    stmfd   sp!, {r4-r11, lr}

    mov     r7, r0                      // save flags

    mrs     r8, cpsr                    // save the old interrupt state
    cpsid   iaf                         // interrupts disabled

.Ldcache_disable:
    tst     r7, #2
    beq     .Licache_disable
    mrc     p15, 0, r0, c1, c0, 0       // cr1
    tst     r0, #(1<<2)                 // is the dcache already disabled?
    beq     .Ldcache_already_disabled

    bic     r0, #(1<<2)
    mcr     p15, 0, r0, c1, c0, 0       // disable dcache

    // flush and invalidate the dcache
    // NOTE: trashes a bunch of registers, can't be spilling stuff to the stack
    bl      flush_invalidate_cache_v7

    b       .Ldcache_disable_L2

.Ldcache_already_disabled:
    // make sure all of the caches are invalidated
    // NOTE: trashes a bunch of registers, can't be spilling stuff to the stack
    bl      invalidate_cache_v7

.Ldcache_disable_L2:
    // disable the L2, if present
    mrc     p15, 0, r0, c1, c0, 1       // aux cr1
    bic     r0, #(1<<1)
    mcr     p15, 0, r0, c1, c0, 1       // disable L2 dcache

.Licache_disable:
    tst     r7, #1
    beq     .Ldone_disable

    mrc     p15, 0, r0, c1, c0, 0       // cr1
    bic     r0, #(1<<12)
    mcr     p15, 0, r0, c1, c0, 0       // disable icache

.Ldone_disable:
    // make sure the icache is always invalidated
    mov     r0, #0
    mcr     p15, 0, r0, c7, c5, 0       // invalidate icache to PoU

    msr     cpsr, r8
    ldmfd   sp!, {r4-r11, pc}


// flush & invalidate cache routine, trashes r0-r6, r9-r11
flush_invalidate_cache_v7:
    /* from ARMv7 manual, B2-17 */
    dmb
    MRC     p15, 1, R0, c0, c0, 1       // Read CLIDR
    ANDS    R3, R0, #0x7000000
    MOV     R3, R3, LSR #23             // Cache level value (naturally aligned)
    BEQ     .Lfinished
    MOV     R10, #0
.Loop1:
    ADD     R2, R10, R10, LSR #1        // Work out 3xcachelevel
    MOV     R1, R0, LSR R2              // bottom 3 bits are the Cache type for this level
    AND     R1, R1, #7                  // get those 3 bits alone
    CMP     R1, #2
    BLT     .Lskip                      // no cache or only instruction cache at this level
    MCR     p15, 2, R10, c0, c0, 0      // write the Cache Size selection register
    isb                                 // ISB to sync the change to the CacheSizeID reg
    MRC     p15, 1, R1, c0, c0, 0       // reads current Cache Size ID register
    AND     R2, R1, #0x7                // extract the line length field
    ADD     R2, R2, #4                  // add 4 for the line length offset (log2 16 bytes)
    LDR     R4, =0x3FF
    ANDS    R4, R4, R1, LSR #3          // R4 is the max number on the way size (right aligned)
    CLZ     R5, R4                      // R5 is the bit position of the way size increment
    LDR     R6, =0x00007FFF
    ANDS    R6, R6, R1, LSR #13         // R6 is the max number of the index size (right aligned)
.Loop2:
    MOV     R9, R4                      // R9 working copy of the max way size (right aligned)
.Loop3:
    ORR     R11, R10, R9, LSL R5        // factor in the way number and cache number into R11
    ORR     R11, R11, R6, LSL R2        // factor in the index number
    MCR     p15, 0, R11, c7, c14, 2     // clean & invalidate by set/way
    SUBS    R9, R9, #1                  // decrement the way number
    BGE     .Loop3
    SUBS    R6, R6, #1                  // decrement the index
    BGE     .Loop2
.Lskip:
    ADD     R10, R10, #2                    // increment the cache number
    CMP     R3, R10
    BGT     .Loop1

.Lfinished:
    mov     r10, #0
    mcr     p15, 2, r10, c0, c0, 0      // select cache level 0
    dsb
    isb

    bx      lr

// invalidate cache routine, trashes r0-r6, r9-r11
invalidate_cache_v7:
    /* from ARMv7 manual, B2-17 */
    dmb
    MRC     p15, 1, R0, c0, c0, 1       // Read CLIDR
    ANDS    R3, R0, #0x7000000
    MOV     R3, R3, LSR #23             // Cache level value (naturally aligned)
    BEQ     .Lfinished_invalidate
    MOV     R10, #0
.Loop1_invalidate:
    ADD     R2, R10, R10, LSR #1        // Work out 3xcachelevel
    MOV     R1, R0, LSR R2              // bottom 3 bits are the Cache type for this level
    AND     R1, R1, #7                  // get those 3 bits alone
    CMP     R1, #2
    BLT     .Lskip_invalidate           // no cache or only instruction cache at this level
    MCR     p15, 2, R10, c0, c0, 0      // write the Cache Size selection register
    isb                                 // ISB to sync the change to the CacheSizeID reg
    MRC     p15, 1, R1, c0, c0, 0       // reads current Cache Size ID register
    AND     R2, R1, #0x7                // extract the line length field
    ADD     R2, R2, #4                  // add 4 for the line length offset (log2 16 bytes)
    LDR     R4, =0x3FF
    ANDS    R4, R4, R1, LSR #3          // R4 is the max number on the way size (right aligned)
    CLZ     R5, R4                      // R5 is the bit position of the way size increment
    LDR     R6, =0x00007FFF
    ANDS    R6, R6, R1, LSR #13         // R6 is the max number of the index size (right aligned)
.Loop2_invalidate:
    MOV     R9, R4                      // R9 working copy of the max way size (right aligned)
.Loop3_invalidate:
    ORR     R11, R10, R9, LSL R5        // factor in the way number and cache number into R11
    ORR     R11, R11, R6, LSL R2        // factor in the index number
    MCR     p15, 0, R11, c7, c6, 2      // invalidate by set/way
    SUBS    R9, R9, #1                  // decrement the way number
    BGE     .Loop3_invalidate
    SUBS    R6, R6, #1                  // decrement the index
    BGE     .Loop2_invalidate
.Lskip_invalidate:
    ADD     R10, R10, #2                // increment the cache number
    CMP     R3, R10
    BGT     .Loop1_invalidate

.Lfinished_invalidate:
    dsb
    mov     r10, #0
    mcr     p15, 2, r10, c0, c0, 0      // select cache level 0
    isb

    bx      lr
