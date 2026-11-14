#include <arch/arch.h>
#include <pb/arch.h>

#include <pb/mmio.h>

#define ARMV7M_NVIC_BASE           0xe000e000

#define NVIC_CCSIDR_OFFSET         0x0d80 /* Cache Size ID Register (Cortex-M7) */
#define NVIC_DCIMVAC_OFFSET        0x0f5c /* D-Cache Invalidate by MVA to PoC (Cortex-M7) */
#define NVIC_DCCMVAC_OFFSET        0x0f68 /* D-Cache Clean by MVA to PoC (Cortex-M7) */
#define NVIC_DCCIMVAC_OFFSET       0x0f70 /* D-Cache Clean and Invalidate by MVA to PoC */
#define NVIC_ICIALLU_OFFSET        0x0f50 /* I-Cache Invalidate All to PoU (Cortex-M7) */

#define NVIC_CCSIDR                (ARMV7M_NVIC_BASE + NVIC_CCSIDR_OFFSET)
#define NVIC_DCIMVAC               (ARMV7M_NVIC_BASE + NVIC_DCIMVAC_OFFSET)
#define NVIC_DCCMVAC               (ARMV7M_NVIC_BASE + NVIC_DCCMVAC_OFFSET)
#define NVIC_DCCIMVAC              (ARMV7M_NVIC_BASE + NVIC_DCCIMVAC_OFFSET)
#define NVIC_ICIALLU               (ARMV7M_NVIC_BASE + NVIC_ICIALLU_OFFSET)

#define NVIC_CCSIDR_LINESIZE_SHIFT (0) /* Bits 0-2: Number of words in each cache line */
#define NVIC_CCSIDR_LINESIZE_MASK  (7 << NVIC_CCSIDR_LINESIZE_SHIFT)

#define CCSIDR_LSSHIFT(n)          (((n) & NVIC_CCSIDR_LINESIZE_MASK) >> NVIC_CCSIDR_LINESIZE_SHIFT)

/*
 * Writes the address of every line that [start, start + len) touches to
 * 'reg', one of the cache maintenance registers that take an address: the
 * cache looks the address up, and a line that holds it is cleaned,
 * invalidated or both. A line that isn't in the cache is left alone. The
 * start is rounded down to a line, by the D-cache's own line size. With a
 * length of 0 the line at 'start' is written, as before.
 */
static void dcache_range_op(uint32_t reg, uintptr_t start, size_t len)
{
    uintptr_t end = start + len;
    uint32_t ccsidr;
    uint32_t sshift;
    uint32_t ssize;

    /* Get the characteristics of the D-Cache */

    ccsidr = mmio_read_32(NVIC_CCSIDR);
    sshift = CCSIDR_LSSHIFT(ccsidr) + 4; /* log2(cache-line-size-in-bytes) */

    ssize = (1 << sshift);
    start &= ~(ssize - 1);
    ARM_DSB();

    do {
        mmio_write_32(reg, start);

        /* Increment the address by the size of one cache line. */

        start += ssize;
    } while (start < end);

    ARM_DSB();
    ARM_ISB();
}

void arch_clean_cache_range(uintptr_t start, size_t len)
{
    dcache_range_op(NVIC_DCCMVAC, start, len);
}

void arch_invalidate_cache_range(uintptr_t start, size_t len)
{
    dcache_range_op(NVIC_DCIMVAC, start, len);
}

void arch_clean_invalidate_cache_range(uintptr_t start, size_t len)
{
    dcache_range_op(NVIC_DCCIMVAC, start, len);
}

void arch_invalidate_icache(void)
{
    ARM_DSB();
    mmio_write_32(NVIC_ICIALLU, 0);
    ARM_DSB();
    ARM_ISB();
}
