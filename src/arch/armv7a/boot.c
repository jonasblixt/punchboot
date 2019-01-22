#include <pb.h>
#include <arch.h>


void arch_jump(uint32_t pc, uint32_t arg0,
                            uint32_t arg1,
                            uint32_t arg2,
                            uint32_t arg3)
{

    volatile uint32_t _pc = pc;

    volatile uint32_t _arg0 = arg0;
    volatile uint32_t _arg1 = arg1;
    volatile uint32_t _arg2 = arg2;
    volatile uint32_t _arg3 = arg3;

    asm volatile(   "mov r0, %0" "\n\r"
                    "mov r1, %1" "\n\r"
                    "mov r2, %2" "\n\r"
                    "mov r3, %3" "\n\r"
                    "mov pc, %4" "\n\r"
                    :
                    : "r" (_arg0),
                      "r" (_arg1),
                      "r" (_arg2),
                      "r" (_arg3),
                      "r" (_pc));

    while(1)
        asm volatile ("wfi");
}
