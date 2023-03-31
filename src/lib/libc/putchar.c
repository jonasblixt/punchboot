#include <stdio.h>
#include <pb/console.h>

int putchar(int c)
{
    console_putc(c);
    return c;
}
