#ifndef INCLUDE_PB_ARCH_H_
#define INCLUDE_PB_ARCH_H_

#include <stdint.h>

void arch_init(void);
unsigned int arch_get_us_tick(void);
void arch_disable_mmu(void);

#endif  // INCLUDE_PB_ARCH_H_
