#include <pb.h>
#include <io.h>
#include <plat.h>


void plat_wdog_init(void)
{
    pb_writel(0, 0x020D8000);
    pb_write((3 << 8) | (1 << 2) | (1 << 3) | (1 << 4), 0x020BC000);

    plat_wdog_kick();
}

void plat_wdog_kick(void)
{
    pb_write(0x5555, 0x020BC002);
    pb_write(0xAAAA, 0x020BC002);
}
