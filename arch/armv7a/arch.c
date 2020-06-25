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
    printf("IFAR: 0x%08x, DFAR 0x%08x\n\r", read_ifar(), read_dfar());
    printf("IFSR: 0x%08x, DFSR 0x%08x\n\r", read_ifsr(), read_dfsr());
    printf("SCTLR: 0x%08x\n\r", read_sctlr());
}

void arch_disable_mmu(void)
{

    /**
     * Disable cache
     * Outer cache disable
     * Invalidate d cache all
     * i cache disable
     * Invalidate i cache all
     *
     */

    arch_disable_cache();
    disable_mmu_secure();
    LOG_DBG("MMU Disabled %x", armv7a_cp15_read_sctlr());
}
