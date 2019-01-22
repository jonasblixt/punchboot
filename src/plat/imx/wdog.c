#include <pb.h>
#include <io.h>
#include <plat.h>
#include <plat/imx/wdog.h>

static struct imx_wdog_device *_dev = NULL;

uint32_t imx_wdog_init(struct imx_wdog_device *dev, uint32_t delay)
{
    if ((dev == NULL) || !delay)
        return PB_ERR;

    _dev = dev;

    /* Timeout value = 9 * 0.5 + 0.5 = 5 s */
    pb_write16((delay*2 << 8) | (1 << 2) | 
                                (1 << 3) | 
                                (1 << 4) |
                                (1 << 5), 
                _dev->base + WDOG_WCR);

    pb_write16(0, _dev->base + WDOG_WMCR);

    return imx_wdog_kick();
}

uint32_t imx_wdog_kick(void)
{
    if (_dev == NULL)
        return PB_ERR;

    pb_write16(0x5555, _dev->base + WDOG_WSR);
    pb_write16(0xAAAA, _dev->base + WDOG_WSR);

    return PB_OK;
}

uint32_t imx_wdog_reset_now(void)
{
    pb_write16((1<<6)|(1<<2), _dev->base + WDOG_WCR);

    while(1)
        asm("nop");
}
