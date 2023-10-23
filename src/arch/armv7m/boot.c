#include <arch/arch.h>

#define VECTOR_STACK_ADDRESS_OFFSET 0
#define VECTOR_RESET_ADDRESS_OFFSET 4


void arch_jump(void* addr, void* p0, void* p1, void *p2, void *p3)
{
    uintptr_t base_addr = (uintptr_t)addr;

    uint32_t stack_address = *((volatile uint32_t *)(base_addr + VECTOR_STACK_ADDRESS_OFFSET));

    /* TODO: Force reset_handler_address into a register and make
     * sure it does not get overwritten, to avoid the risk of
     * reset_handler_address being on the stack, which we move. */
    uint32_t reset_handler_address =
        *((volatile uint32_t *)(base_addr + VECTOR_RESET_ADDRESS_OFFSET));

    /* Move the stack pointer and branch to the starting address */
    __asm__ volatile("dmb 15       \n\t"
                 "msr msp, %0  \n\t"
                 "msr psp, %0  \n\t"
                 "bx %1        \n\t"
                 :
                 : "r"(stack_address), "r"(reset_handler_address)
                 : "memory");
}
