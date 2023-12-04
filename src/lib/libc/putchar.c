#include <pb/console.h>
#include <stdio.h>

int putchar(int c)
{
    console_putc(c);
    return c;
}
