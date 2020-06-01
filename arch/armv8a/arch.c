#include <pb/arch.h>
#include <arch/armv8a/timer.h>
#include <plat/defs.h>

unsigned int arch_get_us_tick(void)
{
    uint32_t tick = (0xFFFFFFFF - read_cntps_tval_el1());
    return (tick >> COUNTER_US_SHIFT);
}
