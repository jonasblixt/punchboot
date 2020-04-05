#include <pb/pb.h>
#include <pb/console.h>

int pb_console_init(struct pb_console *console)
{
    return PB_OK;
}

int pb_console_free(struct pb_console *console)
{
    return PB_OK;
}

int pb_console_start(struct pb_console *console)
{
    struct pb_console_driver *drv = console->driver;
    int rc;

    drv->ready = false;

    if (drv->platform)
    {
        rc = drv->platform->init(drv);

        if (rc != PB_OK)
            return rc;   
    }

    rc = drv->init(drv);

    return rc;
}

int pb_console_add(struct pb_console *console, struct pb_console_driver *drv)
{
    console->driver = drv;
    return PB_OK;
}
