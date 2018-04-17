#include <plat.h>
#include <io.h>


void plat_reset(void) {
    pb_writel(0, REG(0x020D8000,0));
    pb_write((1 << 2) | (1 << 3) | (1 << 4), REG(0x020BC000,0));
 
    while (1);
}
