#ifndef __ARCH_H__
#define __ARCH_H__

#include <stdint.h>

void arch_jump(uint32_t pc, uint32_t arg0,
                            uint32_t arg1,
                            uint32_t arg2,
                            uint32_t arg3);

#endif
