#include <pb/io.h>
#include <pb/arch.h>
#include <arch/arch_helpers.h>
#include <arch/armv7a/timer.h>
#include <plat/defs.h>

const char *exception_strings[] =
{
    "Undefined",
    "SVC",
    "Prefetch",
    "Abort",
    "IRQ",
    "FIQ"
};

void arch_init(void)
{
#ifndef CONFIG_PLAT_QEMU
    /* Enable generic timers */
    pb_write32(1, SCTR_BASE_ADDRESS + TIMER_CNTCR);
#endif
}

unsigned int arch_get_us_tick(void)
{
    uint32_t tick = (0xFFFFFFFF - read_cntp_tval());
    return (tick >> COUNTER_US_SHIFT);
}

void exception(int id)
{
    printf("*** %s (%i) EXCEPTION ***\n\r", exception_strings[id], id);
    printf("IFAR: 0x%08lx, DFAR 0x%08lx\n\r", read_ifar(), read_dfar());
    printf("IFSR: 0x%08lx, DFSR 0x%08lx\n\r", read_ifsr(), read_dfsr());
    printf("SCTLR: 0x%08lx\n\r", read_sctlr());
}

void arch_disable_mmu(void)
{
    arch_disable_cache();
    disable_mmu_secure();

    unsigned int sctlr;

    sctlr = read_sctlr();
    sctlr &= ~(SCTLR_WXN_BIT | SCTLR_M_BIT);
    write_sctlr(sctlr);

    isb();
    dsb();

    LOG_DBG("MMU Disabled");

    return;
}
