
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <plat.h>
#include <io.h>
#include <plat/imx/ocotp.h>

#undef OCOTP_DEBUG

static struct ocotp_dev *_dev;

#ifdef OCOTP_DEBUG
static void ocotp_dump_fuse(uint32_t bank, uint32_t row) {
    uint32_t val;
    uint32_t ret;

    ret = ocotp_read(bank, row, &val);
    printf ("fuse %i %i: %i, 0x%x\n\r", bank, row, ret, val);

}
#endif

uint32_t ocotp_init(struct ocotp_dev *dev) {

    _dev = dev;

    if (dev->words_per_bank < 4)
    {
        LOG_ERR("Invalid words_per_bank setting");
        return PB_ERR;
    }

#ifdef OCOTP_DEBUG
    LOG_DBG("Initializing ocotp at 0x%x", dev->base);
 

    ocotp_dump_fuse(0, 1);
    ocotp_dump_fuse(0, 2);
    ocotp_dump_fuse(0, 3);

    ocotp_dump_fuse(1, 0);
    ocotp_dump_fuse(1, 1);
    ocotp_dump_fuse(1, 2);

    ocotp_dump_fuse(9, 0);
    ocotp_dump_fuse(9, 1);
    ocotp_dump_fuse(9, 2);

#endif

 
    return PB_OK;
}

uint32_t ocotp_write(uint32_t bank, uint32_t row, uint32_t value) {
     uint32_t tmp = 0;

    /* Wait for busy flag */
    while (pb_read32(_dev->base + OCOTP_CTRL) & OCOTP_CTRL_BUSY)
        __asm__("nop");
    
    tmp = pb_read32(_dev->base + OCOTP_CTRL);

    if (tmp & OCOTP_CTRL_ERROR) 
    {
        pb_write32(tmp & ~OCOTP_CTRL_ERROR, _dev->base + OCOTP_CTRL);
        return PB_ERR;
    }


    pb_write32((5 << 22) | (0x06 << 16) | (1 << 12) | 0x299, 
                                    _dev->base + OCOTP_TIMING);


    /* Wait for busy flag */
    while (pb_read32(_dev->base + OCOTP_CTRL) & OCOTP_CTRL_BUSY)
        __asm__("nop");
    
    tmp = pb_read32(_dev->base + OCOTP_CTRL);
    if (tmp & OCOTP_CTRL_ERROR) 
    {
        pb_write32(tmp & ~OCOTP_CTRL_ERROR, _dev->base + OCOTP_CTRL);
        return PB_ERR;
    }

    /* Ref manual says this should be 0x3F, but FSL drivers says 0x7F...*/
    tmp = (bank * _dev->words_per_bank + row) & 0x7F;

    pb_write32 ((OCOTP_CTRL_WR_KEY << 16) | tmp, _dev->base + OCOTP_CTRL);
 
    pb_write32(value, _dev->base + OCOTP_DATA);

    /* Wait for busy flag */
    while (pb_read32(_dev->base + OCOTP_CTRL) & OCOTP_CTRL_BUSY)
        __asm__("nop");
    
    tmp = pb_read32(_dev->base + OCOTP_CTRL);
    if (tmp & OCOTP_CTRL_ERROR) 
    {
        pb_write32(tmp & ~OCOTP_CTRL_ERROR, _dev->base + OCOTP_CTRL);
        return PB_ERR;
    }



    return PB_OK;
}

uint32_t ocotp_read (uint32_t bank, uint32_t row, uint32_t * value) {
    uint32_t tmp = 0;

    /* Wait for busy flag */
    while (pb_read32(_dev->base + OCOTP_CTRL) & OCOTP_CTRL_BUSY)
        __asm__("nop");
    
    tmp = pb_read32(_dev->base + OCOTP_CTRL);
    if (tmp & OCOTP_CTRL_ERROR) {
        pb_write32(tmp & ~OCOTP_CTRL_ERROR, _dev->base + OCOTP_CTRL);
        return PB_ERR;
    }

    /* Ref manual says this should be 0x3F, but FSL drivers says 0x7F...*/
    tmp = (bank * _dev->words_per_bank + row) & 0x7F;

    pb_write32 (tmp, _dev->base + OCOTP_CTRL);
    pb_write32 (1, _dev->base + OCOTP_READ_CTRL);

    /* Wait for busy flag */
    while (pb_read32(_dev->base + OCOTP_CTRL) & OCOTP_CTRL_BUSY)
        __asm__("nop");
 
    *value = pb_read32(_dev->base + OCOTP_READ_FUSE_DATA);
    
    return PB_OK;
}



