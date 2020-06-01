#include <arch/armv7a/timer.h>
#include <plat/defs.h>

unsigned int arch_get_us_tick(void)
{
    uint32_t tick = (0xFFFFFFFF - read_cntp_tval());
    return (tick >> COUNTER_US_SHIFT);
}
