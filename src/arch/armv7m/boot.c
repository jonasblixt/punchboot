#include <arch/arch.h>
#include <pb/arch.h>
#include <pb/mmio.h>

#define VECTOR_STACK_ADDRESS_OFFSET 0
#define VECTOR_RESET_ADDRESS_OFFSET 4

/*
 * Start the image whose vector table is at 'addr' (include/pb/arch.h):
 * PRIMASK set, the I-cache invalidated, VTOR points to that table, MSP and
 * PSP are its initial stack pointer, r0-r3 are p0-p3 and execution continues
 * at its reset handler.
 */
void arch_jump(void *addr, void *p0, void *p1, void *p2, void *p3)
{
    __asm__ volatile("cpsid i" : : : "memory");
    arch_invalidate_icache();

    uintptr_t base_addr = (uintptr_t)addr;
    uint32_t stack_address = *((volatile uint32_t *)(base_addr + VECTOR_STACK_ADDRESS_OFFSET));
    uint32_t reset_handler_address =
        *((volatile uint32_t *)(base_addr + VECTOR_RESET_ADDRESS_OFFSET));

    /* The image's exceptions go to its own handlers from its first
     * instruction on */
    mmio_write_32(ARMV7M_SCB_VTOR, (uint32_t)base_addr);

    /* The arguments in r0-r3, so the stack pointer and the reset handler
     * are in other registers and nothing is read from the old stack after
     * MSP moves */
    register uintptr_t r0 __asm__("r0") = (uintptr_t)p0;
    register uintptr_t r1 __asm__("r1") = (uintptr_t)p1;
    register uintptr_t r2 __asm__("r2") = (uintptr_t)p2;
    register uintptr_t r3 __asm__("r3") = (uintptr_t)p3;

    __asm__ volatile(
        "dsb          \n\t"
        "isb          \n\t"
        "msr msp, %4  \n\t"
        "msr psp, %4  \n\t"
        "bx %5        \n\t"
        :
        : "r"(r0), "r"(r1), "r"(r2), "r"(r3), "r"(stack_address), "r"(reset_handler_address)
        : "memory");
    __builtin_unreachable();
}
