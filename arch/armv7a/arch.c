#include <pb/io.h>
#include <arch/armv7a/timer.h>
#include <plat/defs.h>

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
