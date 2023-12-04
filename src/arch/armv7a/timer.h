#ifndef ARCH_ARMV7A_TIMER_H_
#define ARCH_ARMV7A_TIMER_H_

#include <stdint.h>

#define TIMER_CNTCR  (0)
#define TIMER_CNTSR  (4)
#define TIMER_CNTCV1 (8)
#define TIMER_CNTCV2 (12)

void armv7a_timer_init(void);
uint32_t read_cntp_tval(void);

#endif // ARCH_ARMV7A_TIMER_H_
