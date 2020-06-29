#include <stdio.h>
#include <pb/arch.h>
#include <pb/io.h>
#include <arch/armv8a/timer.h>
#include <arch/arch_helpers.h>
#include <plat/defs.h>

void arch_init(void)
{
/* Timer stuff is already enabled for imx8x */
#ifndef CONFIG_PLAT_IMX8X
    /* Enable generic timers */
    pb_write32(1, SCTR_BASE_ADDRESS + TIMER_CNTCR);
#endif

}

unsigned int arch_get_us_tick(void)
{
    uint32_t tick = (0xFFFFFFFF - read_cntp_tval_el0());
    return (tick >> COUNTER_US_SHIFT);
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
    printf("FAR: 0x%08lx\n\r", read_far_el3());
    printf("SCTLR: 0x%08lx\n\r", read_sctlr_el3());
}

void arch_disable_mmu(void)
{
    disable_mmu_el3();
}
