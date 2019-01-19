#ifndef __IMX_WDOG_H__
#define __IMX_WDOG_H__

#include <pb.h>
#include <io.h>

#define WDOG_WCR  0x0000
#define WDOG_WSR  0x0002
#define WDOG_WRSR 0x0004
#define WDOG_WICR 0x0006
#define WDOG_WMCR 0x0008


struct imx_wdog_device
{
    __iomem base;
    uint32_t delay;
};

uint32_t imx_wdog_init(struct imx_wdog_device *dev, uint32_t delay);
uint32_t imx_wdog_kick(void);
uint32_t imx_wdog_reset_now(void);

#endif
