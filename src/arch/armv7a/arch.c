#include <arch/arch_helpers.h>
#include <arch/armv7a/timer.h>
#include <pb/arch.h>
#include <pb/plat.h>

extern char _code_start, _code_end, _data_region_start, _data_region_end, _ro_data_region_start,
    _ro_data_region_end, _zero_region_start, _zero_region_end, _stack_start, _stack_end,
    _no_init_start, _no_init_end, end;

const char *exception_strings[] = { "Undefined", "SVC", "Prefetch", "Abort", "IRQ", "FIQ" };

void arch_init(void)
{
}

void exception(int id)
{
    printf("*** %s (%i) EXCEPTION ***\n\r", exception_strings[id], id);
    printf("IFAR: 0x%08lx, DFAR 0x%08lx\n\r", read_ifar(), read_dfar());
    printf("IFSR: 0x%08lx, DFSR 0x%08lx\n\r", read_ifsr(), read_dfsr());
    printf("SCTLR: 0x%08lx\n\r", read_sctlr());
    plat_reset();
}

void arch_disable_mmu(void)
{
    uintptr_t stack_start = (uintptr_t)&_stack_start;
    size_t stack_size = ((uintptr_t)&_stack_end) - ((uintptr_t)&_stack_start);

    arch_clean_cache_range(stack_start, stack_size);
    arch_invalidate_cache_range(stack_start, stack_size);

    arch_disable_cache();
    disable_mmu_secure();

    unsigned int sctlr;

    sctlr = read_sctlr();
    sctlr &= ~(SCTLR_WXN_BIT | SCTLR_M_BIT);
    write_sctlr(sctlr);

    isb();

    LOG_DBG("MMU Disabled");
}
