#include <stdio.h>
#include <console.h>
#include <config.h>

static const struct console_ops *ops;
static uintptr_t base;

void console_init(uintptr_t base_, const struct console_ops *ops_)
{
    ops = ops_;
    base = base_;

#ifdef CONFIG_PRINT_BOOT_BANNER
    printf("\n\rPB " PB_VERSION ", %s (%i)\n\r", plat_boot_reason_str(),
                                                 plat_boot_reason());
#endif
}

void console_putc(char c)
{
    if (ops->putc) {
        ops->putc(base, c);
    }
}
