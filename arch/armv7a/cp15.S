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

