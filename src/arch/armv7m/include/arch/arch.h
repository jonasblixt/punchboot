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

/* Every line of the L1 D-cache invalidated, before pb turns it on (mmu.S) */
void armv7m_dcache_invalidate_all(void);

/* The system control block */
#define ARMV7M_SCB_VTOR            0xe000ed08 /* Vector table offset */
#define ARMV7M_SCB_CCR             0xe000ed14 /* Configuration and control */
#define ARMV7M_CCR_DC              (1 << 16) /* D-cache enable */
#define ARMV7M_CCR_IC              (1 << 17) /* I-cache enable */
/* Cortex-M7 L1 cache control */
#define ARMV7M_CACR                0xe000ef9c
/* The MPU (ARMv7-M ARM B3.5) */
#define ARMV7M_MPU_CTRL            0xe000ed94
#define ARMV7M_MPU_CTRL_ENABLE     (1 << 0)
#define ARMV7M_MPU_CTRL_PRIVDEFENA (1 << 2) /* The default map for what no region has */
#define ARMV7M_MPU_RNR             0xe000ed98
#define ARMV7M_MPU_RBAR            0xe000ed9c
#define ARMV7M_MPU_RASR            0xe000eda0
#define ARMV7M_MPU_RASR_ENABLE     (1 << 0)
#define ARMV7M_MPU_RASR_SIZE(log2) (((log2) - 1) << 1) /* 2^(SIZE + 1) bytes */
#define ARMV7M_MPU_RASR_B          (1 << 16)
#define ARMV7M_MPU_RASR_C          (1 << 17)
#define ARMV7M_MPU_RASR_TEX(x)     ((x) << 19)
#define ARMV7M_MPU_RASR_AP_RO      (6 << 24) /* Read only, privileged and not */
#define ARMV7M_MPU_RASR_XN         (1 << 28)
#define ARMV7M_CACR_FORCEWT        (1 << 2) /* The D-cache is write-through */

#endif
