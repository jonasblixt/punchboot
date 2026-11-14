#include <arch/arch_helpers.h>
#include <arch/armv7a/timer.h>
#include <pb/arch.h>
#include <pb/console.h>
#include <pb/dma.h>
#include <pb/pb.h>
#include <pb/plat.h>

extern char _code_start, _code_end, _data_region_start, _data_region_end, _ro_data_region_start,
    _ro_data_region_end, _zero_region_start, _zero_region_end, _stack_start, _stack_end,
    _no_init_start, _no_init_end, end;

const char *exception_strings[] = { "Undefined", "SVC", "Prefetch", "Abort", "IRQ", "FIQ" };

void arch_init(void)
{
    /* What a device writes is declared and rounded with PB_DMA_ALIGN
     * (drivers.md 4.1): a core with a longer line shares lines between such
     * buffers and their neighbours. CTR's DminLine, which the range
     * functions in cp15.S step by */
    size_t line = SIZE_FROM_LOG2_WORDS((read_ctr() >> CTR_DMINLINE_SHIFT) & CTR_DMINLINE_MASK);

    if (line > PB_DMA_ALIGN)
        LOG_ERR("D-cache line of %zu bytes, PB_DMA_ALIGN is %i", line, PB_DMA_ALIGN);
}

void exception(int id)
{
    /* The exception may have hit in the middle of a RAM log only message */
    console_uart_mute(false);
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

    /* An exception in the sequence would run with half of it done */
    disable_irq();
    disable_fiq();

    arch_clean_cache_range(stack_start, stack_size);
    arch_invalidate_cache_range(stack_start, stack_size);

    /* Clean and disable the D-cache. What runs next reads with the cache
     * off what was written with it on, the boot arguments among them. */
    arch_disable_cache(ARCH_CACHE_FLAG_DCACHE);
    disable_mmu_secure();

    unsigned int sctlr;

    sctlr = read_sctlr();
    sctlr &= ~(SCTLR_WXN_BIT | SCTLR_M_BIT);
    write_sctlr(sctlr);

    isb();

    LOG_DBG("MMU Disabled");
}

void arch_invalidate_icache(void)
{
    dsb();
    __asm__ volatile("mcr p15, 0, %0, c7, c5, 0" : : "r"(0) : "memory"); /* ICIALLU */
    dsb();
    isb();
}
