/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/io.h>
#include <plat/imx/ocotp.h>

static __iomem base;
static uint32_t wpb;

#ifdef CONFIG_IMX_OCOTP_DEBUG
static void ocotp_dump_fuse(uint32_t bank, uint32_t row)
{
    uint32_t val;
    uint32_t ret;

    ret = ocotp_read(bank, row, &val);
    printf("fuse %i %i: %i, 0x%x\n\r", bank, row, ret, val);
}
#endif

int ocotp_init(__iomem base_addr, unsigned int words_per_bank)
{
    base = base_addr;
    wpb = words_per_bank;

    if (words_per_bank < 4)
    {
        LOG_ERR("Invalid words_per_bank setting");
        return PB_ERR;
    }

#ifdef CONFIG_IMX_OCOTP_DEBUG
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

int ocotp_write(uint32_t bank, uint32_t row, uint32_t value)
{
     uint32_t tmp = 0;

    /* Wait for busy flag */
    while (pb_read32(base + OCOTP_CTRL) & OCOTP_CTRL_BUSY)
        __asm__("nop");

    tmp = pb_read32(base + OCOTP_CTRL);

    if (tmp & OCOTP_CTRL_ERROR)
    {
        pb_write32(tmp & ~OCOTP_CTRL_ERROR, base + OCOTP_CTRL);
        return PB_ERR;
    }


    pb_write32((5 << 22) | (0x06 << 16) | (1 << 12) | 0x299,
                                    base + OCOTP_TIMING);


    /* Wait for busy flag */
    while (pb_read32(base + OCOTP_CTRL) & OCOTP_CTRL_BUSY)
        __asm__("nop");

    tmp = pb_read32(base + OCOTP_CTRL);
    if (tmp & OCOTP_CTRL_ERROR)
    {
        pb_write32(tmp & ~OCOTP_CTRL_ERROR, base + OCOTP_CTRL);
        return PB_ERR;
    }

    /* Ref manual says this should be 0x3F, but FSL drivers says 0x7F...*/
    tmp = ((bank * wpb) + row) & 0x7F;

    pb_write32((OCOTP_CTRL_WR_KEY << 16) | tmp, base + OCOTP_CTRL);

    pb_write32(value, base + OCOTP_DATA);

    /* Wait for busy flag */
    while (pb_read32(base + OCOTP_CTRL) & OCOTP_CTRL_BUSY)
        __asm__("nop");

    tmp = pb_read32(base + OCOTP_CTRL);
    if (tmp & OCOTP_CTRL_ERROR)
    {
        pb_write32(tmp & ~OCOTP_CTRL_ERROR, base + OCOTP_CTRL);
        return PB_ERR;
    }

    return PB_OK;
}

int ocotp_read(uint32_t bank, uint32_t row, uint32_t * value)
{
    uint32_t tmp = 0;

    /* Wait for busy flag */
    while (pb_read32(base + OCOTP_CTRL) & OCOTP_CTRL_BUSY)
        __asm__("nop");

    tmp = pb_read32(base + OCOTP_CTRL);

    if (tmp & OCOTP_CTRL_ERROR)
    {
        pb_write32(tmp & ~OCOTP_CTRL_ERROR, base + OCOTP_CTRL);
        return PB_ERR;
    }

    /* Ref manual says this should be 0x3F, but FSL drivers says 0x7F...*/
    tmp = ((bank * wpb) + row) & 0x7F;

    pb_write32(tmp, base + OCOTP_CTRL);
    pb_write32(1, base + OCOTP_READ_CTRL);

    /* Wait for busy flag */
    while (pb_read32(base + OCOTP_CTRL) & OCOTP_CTRL_BUSY)
        __asm__("nop");

    *value = pb_read32(base + OCOTP_READ_FUSE_DATA);

    return PB_OK;
}
