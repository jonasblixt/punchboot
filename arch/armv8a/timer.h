#ifndef ARCH_ARMV8A_TIMER_H_
#define ARCH_ARMV8A_TIMER_H_

#include <stdint.h>

uint32_t read_cntps_tval_el1(void);
uint32_t read_cntfrq_el0(void);

#endif  // ARCH_ARMV8A_TIMER_H_
