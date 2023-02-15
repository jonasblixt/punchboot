#ifndef ARCH_ARMV7A_CP15_H_
#define ARCH_ARMV7A_CP15_H_

#include <stdint.h>

uint32_t armv7a_cp15_read_id_mmfr0(void);
uint32_t armv7a_cp15_read_cpuid(void);
uint32_t armv7a_cp15_read_sctlr(void);
void armv7a_cp15_disable_mmu(void);

#endif  // ARCH_ARMV7A_CP15_H_
