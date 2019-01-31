#include <pb.h>
#include <io.h>
#include <plat/imx/lpuart.h>


uint32_t lpuart_init(struct lpuart_device *dev)
{
    pb_write32(dev->baudrate, dev->base + BAUD);

    pb_setbit32((1 << 19) | (1 << 18), dev->base + CTRL);

    pb_write32('A', dev->base + DATA);
    return PB_OK;
}

uint32_t lpuart_putc(struct lpuart_device *dev,
                     char c)
{
    return PB_OK;
}


