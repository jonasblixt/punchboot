#ifndef INCLUDE_ARCH_ARMV7M_ARCH_H
#define INCLUDE_ARCH_ARMV7M_ARCH_H

#define ULONG_MAX 0xFFFFFFFFUL

#include <stddef.h>
#include <stdint.h>

#define arm_isb(n) __asm__ __volatile__("isb " #n : : : "memory")
#define arm_dsb(n) __asm__ __volatile__("dsb " #n : : : "memory")
#define arm_dmb(n) __asm__ __volatile__("dmb " #n : : : "memory")

#define ARM_DSB()  arm_dsb(15)
#define ARM_ISB()  arm_isb(15)
#define ARM_DMB()  arm_dmb(15)

void arch_clean_cache_range(uintptr_t start, size_t len);
void arch_invalidate_cache_range(uintptr_t start, size_t len);

/* The system control block */
#define ARMV7M_SCB_VTOR     0xe000ed08 /* Vector table offset */
#define ARMV7M_SCB_CCR      0xe000ed14 /* Configuration and control */
#define ARMV7M_CCR_DC       (1 << 16) /* D-cache enable */
#define ARMV7M_CCR_IC       (1 << 17) /* I-cache enable */
/* Cortex-M7 L1 cache control */
#define ARMV7M_CACR         0xe000ef9c
#define ARMV7M_CACR_FORCEWT (1 << 2) /* The D-cache is write-through */

#endif
