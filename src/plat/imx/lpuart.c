#include <pb.h>
#include <io.h>
#include <plat/imx/lpuart.h>


uint32_t lpuart_init(struct lpuart_device *dev)
{
    uint32_t tmp;

	tmp = pb_read32(dev->base + CTRL);
	tmp &= ~(CTRL_TE | CTRL_RE);
	pb_write32(tmp,dev->base + CTRL);

	pb_write32(0,dev->base + MODIR);
	pb_write32( ~(FIFO_TXFE | FIFO_RXFE), dev->base + FIFO);

	pb_write32(0,dev->base + MATCH);

    pb_write32(dev->baudrate, dev->base + BAUD);


	tmp = pb_read32(dev->base + CTRL);
	tmp &= ~(LPUART_CTRL_PE_MASK | LPUART_CTRL_PT_MASK | LPUART_CTRL_M_MASK);
	pb_write32(tmp,dev->base + CTRL);

	pb_write32( CTRL_RE | CTRL_TE, dev->base + CTRL);

    return PB_OK;
}

uint32_t lpuart_putc(struct lpuart_device *dev,
                     char c)
{

    while (!(pb_read32(dev->base + STAT) & (1 << 22)))
        __asm__("nop");
    pb_write32(c, dev->base + DATA);
    return PB_OK;
}


