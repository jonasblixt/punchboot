#include <pb/pb.h>
#include <pb/plat.h>

int putchar(int c)
{
    plat_console_putchar(c);
    return c;
}
