#include <arch/arch_helpers.h>
#include <arch/armv8a/timer.h>
#include <pb/arch.h>
#include <pb/pb.h>

void arch_init(void)
{
}

/* Generic exception handler */
void exception(int index)
{
    printf("*** UNHANDLED EXCEPTION ***\n\r");
    printf("Index %i\n\r", index);
}

void exception_sync(void)
{
    printf("*** SYNCHRONOUS EXCEPTION ***\n\r");
    printf("ESR: 0x%08lx\n\r", read_esr_el3());
    printf("ELR: 0x%08lx\n\r", read_elr_el3());
    printf("FAR: 0x%08lx\n\r", read_far_el3());
    printf("SCTLR: 0x%08lx\n\r", read_sctlr_el3());
}

/* arch_disable_mmu() is implemented in cache-ops.S. It has to be assembly: the
 * ordering it depends on cannot be expressed in C, see the comment there. */
