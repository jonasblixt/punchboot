#include <pb/arch.h>
#include <arch/armv8a/timer.h>
#include <plat/defs.h>

void arch_init(void)
{
}

unsigned int arch_get_us_tick(void)
{
    uint32_t tick = (0xFFFFFFFF - read_cntp_tval_el0());
    return (tick >> COUNTER_US_SHIFT);
}
