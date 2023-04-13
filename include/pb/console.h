#ifndef INCLUDE_CONSOLE_H

#include <inttypes.h>

struct console_ops
{
    void(*putc)(uintptr_t base, char c);
};

void console_putc(char c);
void console_init(uintptr_t base, const struct console_ops *ops);

#endif
