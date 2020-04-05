#ifndef INCLUDE_PB_CONSOLE_H_
#define INCLUDE_PB_CONSOLE_H_

#include <stdint.h>
#include <stdbool.h>

struct pb_console_driver;

typedef int (*pb_console_call_t) (struct pb_console_driver *drv);

typedef int (*pb_console_io_t) (struct pb_console_driver *drv,
                                char *buf, size_t size);

struct pb_console_plat_driver
{
    pb_console_call_t init;
    pb_console_call_t free;
    void *private;
    size_t size;
};

struct pb_console_driver
{
    const char *name;
    bool ready;
    pb_console_call_t init;
    pb_console_call_t free;
    pb_console_io_t write;
    struct pb_console_plat_driver *platform;
    void *private;
    size_t size;
};

struct pb_console
{
    struct pb_console_driver *driver;
};

int pb_console_init(struct pb_console *console);
int pb_console_free(struct pb_console *console);
int pb_console_start(struct pb_console *console);
int pb_console_add(struct pb_console *console, struct pb_console_driver *drv);

#endif  // INCLUDE_PB_CONSOLE_H_
