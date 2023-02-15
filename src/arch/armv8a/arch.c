#include <stdio.h>
#include <pb/arch.h>
#include <pb/io.h>
#include <arch/armv8a/timer.h>
#include <arch/arch_helpers.h>
#include <plat/defs.h>

extern char _code_start, _code_end,
            _data_region_start, _data_region_end,
            _ro_data_region_start, _ro_data_region_end,
            _zero_region_start, _zero_region_end,
            _stack_start, _stack_end,
            _big_buffer_start, _big_buffer_end, end;

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

void arch_disable_mmu(void)
{
    uintptr_t stack_start = (uintptr_t) &_stack_start;
    size_t stack_size = ((uintptr_t) &_stack_end) -
                      ((uintptr_t) &_stack_start);

    arch_clean_cache_range(stack_start, stack_size);
    arch_invalidate_cache_range(stack_start, stack_size);

    LOG_DBG("Disabling MMU");
    disable_mmu_el3();
    LOG_DBG("Done");
}
