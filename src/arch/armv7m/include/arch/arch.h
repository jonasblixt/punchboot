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

void arch_jump(void *addr, void *p0, void *p1, void *p2, void *p3);

#endif
