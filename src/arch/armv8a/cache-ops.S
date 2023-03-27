#include <arch/arch.h>
#include <platform_defs.h>

.text

/* Copied from Little Kernel */

.macro cache_range_op, cache op
    add     x2, x0, x1                  // calculate the end address
    bic     x3, x0, #(CACHE_LINE-1)     // align the start with a cache line
.Lcache_range_op_loop\@:
    \cache  \op, x3
    add     x3, x3, #CACHE_LINE
    cmp     x3, x2
    blo     .Lcache_range_op_loop\@
    dsb     sy
.endm

    /* void arch_flush_cache_range(addr_t start, size_t len); */
func (arch_clean_cache_range)
    cache_range_op dc cvac         // clean cache to PoC by MVA
    ret

    /* void arch_flush_invalidate_cache_range(addr_t start, size_t len); */
func (arch_clean_invalidate_cache_range)
    cache_range_op dc civac        // clean & invalidate dcache to PoC by MVA
    ret

    /* void arch_invalidate_cache_range(addr_t start, size_t len); */
func (arch_invalidate_cache_range)
    cache_range_op dc ivac         // invalidate dcache to PoC by MVA
    ret

    /* void arch_sync_cache_range(addr_t start, size_t len); */
func (arch_sync_cache_range)
    cache_range_op dc cvau         // clean dcache to PoU by MVA
    cache_range_op ic ivau         // invalidate icache to PoU by MVA
    ret
