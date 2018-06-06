#include <plat.h>
#include <io.h>
#include <tinyprintf.h>
#include "ocotp.h"

#undef OCOTP_DEBUG

static struct ocotp_dev *_dev;

#ifdef OCOTP_DEBUG
static void ocotp_dump_fuse(uint32_t bank, uint32_t row) {
    uint32_t val;
    uint32_t ret;

    ret = ocotp_read(bank, row, &val);
    tfp_printf ("fuse %i %i: %i, 0x%8.8X\n\r", bank, row, ret, val);

}
#endif

void ocotp_init(struct ocotp_dev *dev) {

    _dev = dev;
 
#ifdef OCOTP_DEBUG
    tfp_printf("Initializing ocotp at 0x%8.8X\n\r", dev->base);
  
    ocotp_dump_fuse(0, 5);
    ocotp_dump_fuse(0, 6);

    ocotp_dump_fuse(4, 2);
    ocotp_dump_fuse(4, 3);
    ocotp_dump_fuse(4, 4);

    ocotp_dump_fuse(15, 0);
    ocotp_dump_fuse(15, 1);



    ocotp_dump_fuse(15, 4);
    ocotp_dump_fuse(15, 5);
    ocotp_dump_fuse(15, 6);
    ocotp_dump_fuse(15, 7);


    ocotp_dump_fuse(3, 0);
    ocotp_dump_fuse(3, 1);
    ocotp_dump_fuse(3, 2);
    ocotp_dump_fuse(3, 3);
    ocotp_dump_fuse(3, 4);
    ocotp_dump_fuse(3, 5);
    ocotp_dump_fuse(3, 6);
    ocotp_dump_fuse(3, 7);
#endif

 

}

uint32_t ocotp_write(uint32_t bank, uint32_t row, uint32_t value) {
     uint32_t tmp = 0;

    /* Wait for busy flag */
    while (pb_readl(_dev->base + OCOTP_CTRL) & OCOTP_CTRL_BUSY)
        asm("nop");
    
    tmp = pb_readl(_dev->base + OCOTP_CTRL);
    if (tmp & OCOTP_CTRL_ERROR) {
        pb_writel(tmp & ~OCOTP_CTRL_ERROR, _dev->base + OCOTP_CTRL);
        return PB_ERR;
    }


    pb_writel((5 << 22) | (0x06 << 16) | (1 << 12) | 0x299, 
                                    _dev->base + OCOTP_TIMING);


    /* Wait for busy flag */
    while (pb_readl(_dev->base + OCOTP_CTRL) & OCOTP_CTRL_BUSY)
        asm("nop");
    
    tmp = pb_readl(_dev->base + OCOTP_CTRL);
    if (tmp & OCOTP_CTRL_ERROR) {
        pb_writel(tmp & ~OCOTP_CTRL_ERROR, _dev->base + OCOTP_CTRL);
        return PB_ERR;
    }

    /* Ref manual says this should be 0x3F, but FSL drivers says 0x7F...*/
    tmp = (bank * 8 + row) & 0x7F;

    pb_writel ((OCOTP_CTRL_WR_KEY << 16) | tmp, _dev->base + OCOTP_CTRL);
 
    pb_writel(value, _dev->base + OCOTP_DATA);

    /* Wait for busy flag */
    while (pb_readl(_dev->base + OCOTP_CTRL) & OCOTP_CTRL_BUSY)
        asm("nop");
    
    tmp = pb_readl(_dev->base + OCOTP_CTRL);
    if (tmp & OCOTP_CTRL_ERROR) {
        pb_writel(tmp & ~OCOTP_CTRL_ERROR, _dev->base + OCOTP_CTRL);
        return PB_ERR;
    }



    return PB_OK;
}

uint32_t ocotp_read (uint32_t bank, uint32_t row, uint32_t * value) {
    uint32_t tmp = 0;

    /* Wait for busy flag */
    while (pb_readl(_dev->base + OCOTP_CTRL) & OCOTP_CTRL_BUSY)
        asm("nop");
    
    tmp = pb_readl(_dev->base + OCOTP_CTRL);
    if (tmp & OCOTP_CTRL_ERROR) {
        pb_writel(tmp & ~OCOTP_CTRL_ERROR, _dev->base + OCOTP_CTRL);
        return PB_ERR;
    }

    /* Ref manual says this should be 0x3F, but FSL drivers says 0x7F...*/
    tmp = (bank * 8 + row) & 0x7F;

    pb_writel (tmp, _dev->base + OCOTP_CTRL);
    pb_writel (1, _dev->base + OCOTP_READ_CTRL);

    /* Wait for busy flag */
    while (pb_readl(_dev->base + OCOTP_CTRL) & OCOTP_CTRL_BUSY)
        asm("nop");
 
    *value = pb_readl(_dev->base + OCOTP_READ_FUSE_DATA);
    
    return PB_OK;
}



