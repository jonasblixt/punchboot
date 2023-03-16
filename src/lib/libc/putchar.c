#include <stdio.h>
#include <console.h>

int putchar(int c)
{
    console_putc(c);
    return c;
}
