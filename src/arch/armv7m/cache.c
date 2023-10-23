#include <arch/arch.h>

#include <pb/mmio.h>

#define ARMV7M_NVIC_BASE                0xe000e000

#define NVIC_CCSIDR_OFFSET              0x0d80 /* Cache Size ID Register (Cortex-M7) */
#define NVIC_DCIMVAC_OFFSET             0x0f5c /* D-Cache Invalidate by MVA to PoC (Cortex-M7) */
#define NVIC_DCCMVAC_OFFSET             0x0f68 /* D-Cache Clean by MVA to PoC (Cortex-M7) */

#define NVIC_CCSIDR                     (ARMV7M_NVIC_BASE + NVIC_CCSIDR_OFFSET)
#define NVIC_DCIMVAC                    (ARMV7M_NVIC_BASE + NVIC_DCIMVAC_OFFSET)
#define NVIC_DCCMVAC                    (ARMV7M_NVIC_BASE + NVIC_DCCMVAC_OFFSET)

#define NVIC_CCSIDR_LINESIZE_SHIFT      (0)       /* Bits 0-2: Number of words in each cache line */
#define NVIC_CCSIDR_LINESIZE_MASK       (7 << NVIC_CCSIDR_LINESIZE_SHIFT)

#define CCSIDR_LSSHIFT(n) \
   (((n) & NVIC_CCSIDR_LINESIZE_MASK) >> NVIC_CCSIDR_LINESIZE_SHIFT)

void arch_clean_cache_range(uintptr_t start, size_t len)
{
  uintptr_t end = start + len;
  uint32_t ccsidr;
  uint32_t sshift;
  uint32_t ssize;

  /* Get the characteristics of the D-Cache */

  ccsidr = mmio_read_32(NVIC_CCSIDR);
  sshift = CCSIDR_LSSHIFT(ccsidr) + 4;   /* log2(cache-line-size-in-bytes) */

  /* Clean the D-Cache over the range of addresses */

  ssize  = (1 << sshift);
  start &= ~(ssize - 1);
  ARM_DSB();

  do
    {
      /* The below store causes the cache to check its directory and
       * determine if this address is contained in the cache. If so, it
       * clean that cache line. Only the cache way containing the
       * address is invalidated. If the address is not in the cache, then
       * nothing is invalidated.
       */

      mmio_write_32(NVIC_DCCMVAC, start);

      /* Increment the address by the size of one cache line. */

      start += ssize;
    }
  while (start < end);

  ARM_DSB();
  ARM_ISB();
}

void arch_invalidate_cache_range(uintptr_t start, size_t len)
{
  uintptr_t end = start + len;
  uint32_t ccsidr;
  uint32_t sshift;
  uint32_t ssize;

  /* Get the characteristics of the D-Cache */

  ccsidr = mmio_read_32(NVIC_CCSIDR);
  sshift = CCSIDR_LSSHIFT(ccsidr) + 4;   /* log2(cache-line-size-in-bytes) */

  /* Invalidate the D-Cache containing this range of addresses */

  ssize  = (1 << sshift);

  /* Round down the start address to the nearest cache line boundary.
   *
   *   sshift = 5      : Offset to the beginning of the set field
   *   (ssize - 1)  = 0x007f : Mask of the set field
   */

  start &= ~(ssize - 1);
  ARM_DSB();

  do
    {
      /* The below store causes the cache to check its directory and
       * determine if this address is contained in the cache. If so, it
       * invalidate that cache line. Only the cache way containing the
       * address is invalidated. If the address is not in the cache, then
       * nothing is invalidated.
       */

      mmio_write_32(NVIC_DCIMVAC, start);

      /* Increment the address by the size of one cache line. */

      start += ssize;
    }
  while (start < end);

  ARM_DSB();
  ARM_ISB();
}
