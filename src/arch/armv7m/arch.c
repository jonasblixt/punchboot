#include <arch/arch.h>
#include <pb/arch.h>
#include <pb/dma.h>
#include <pb/pb.h>

void arch_init(void)
{
    /* What a device writes is declared and rounded with PB_DMA_ALIGN
     * (drivers.md 4.1): a core with a longer line shares lines between such
     * buffers and their neighbours */
    size_t line = armv7m_dcache_line_size();

    if (line > PB_DMA_ALIGN)
        LOG_ERR("D-cache line of %zu bytes, PB_DMA_ALIGN is %i", line, PB_DMA_ALIGN);
}
