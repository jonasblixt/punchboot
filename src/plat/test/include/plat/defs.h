#ifndef __IMX8M_DEFS_H__
#define __IMX8M_DEFS_H__

#include <pb.h>
#include <io.h>

#define PB_BOOTPART_OFFSET 2

struct pb_platform_setup
{
    __iomem uart_base;
};

#endif
