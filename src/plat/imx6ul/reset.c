#include <plat.h>
#include <io.h>


void plat_reset(void) {
    pb_writel(0, 0x020D8000);
    pb_write((1 << 2) | (1 << 3) | (1 << 4), 0x020BC000);
 
    while (1);
}
