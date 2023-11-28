/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/fuse/imx_ocotp.h>
#include <pb/mmio.h>
#include <pb/pb.h>

static uintptr_t base;
static uint32_t wpb;

#ifdef CONFIG_IMX_OCOTP_DEBUG
static void ocotp_dump_fuse(uint32_t bank, uint32_t row)
{
    uint32_t val;
    uint32_t ret;

    ret = imx_ocotp_read(bank, row, &val);
    printf("fuse %i %i: %i, 0x%x\n\r", bank, row, ret, val);
}
#endif

int imx_ocotp_init(uintptr_t base_, unsigned int words_per_bank)
{
    base = base_;
    wpb = words_per_bank;

    LOG_DBG("Initializing ocotp at 0x%" PRIxPTR, base);

    if (words_per_bank < 4) {
        LOG_ERR("Invalid words_per_bank setting");
        return -PB_ERR_PARAM;
    }

#ifdef CONFIG_IMX_OCOTP_DEBUG
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

int imx_ocotp_write(uint32_t bank, uint32_t row, uint32_t value)
{
    uint32_t tmp = 0;

    /* Wait for busy flag */
    while (mmio_read_32(base + OCOTP_CTRL) & OCOTP_CTRL_BUSY) {
    };

    tmp = mmio_read_32(base + OCOTP_CTRL);

    if (tmp & OCOTP_CTRL_ERROR) {
        mmio_write_32(base + OCOTP_CTRL, tmp & ~OCOTP_CTRL_ERROR);
        return -PB_ERR_IO;
    }

    mmio_write_32(base + OCOTP_TIMING, (5 << 22) | (0x06 << 16) | (1 << 12) | 0x299);

    /* Wait for busy flag */
    while (mmio_read_32(base + OCOTP_CTRL) & OCOTP_CTRL_BUSY) {
    };

    tmp = mmio_read_32(base + OCOTP_CTRL);

    if (tmp & OCOTP_CTRL_ERROR) {
        mmio_write_32(base + OCOTP_CTRL, tmp & ~OCOTP_CTRL_ERROR);
        return -PB_ERR_IO;
    }

    /* Ref manual says this should be 0x3F, but FSL drivers says 0x7F...*/
    tmp = ((bank * wpb) + row) & 0x7F;

    mmio_write_32(base + OCOTP_CTRL, (OCOTP_CTRL_WR_KEY << 16) | tmp);
    mmio_write_32(base + OCOTP_DATA, value);

    /* Wait for busy flag */
    while (mmio_read_32(base + OCOTP_CTRL) & OCOTP_CTRL_BUSY) {
    };

    tmp = mmio_read_32(base + OCOTP_CTRL);
    if (tmp & OCOTP_CTRL_ERROR) {
        mmio_write_32(base + OCOTP_CTRL, tmp & ~OCOTP_CTRL_ERROR);
        return -PB_ERR_IO;
    }

    return PB_OK;
}

int imx_ocotp_read(uint32_t bank, uint32_t row, uint32_t *value)
{
    uint32_t tmp = 0;

    /* Wait for busy flag */
    while (mmio_read_32(base + OCOTP_CTRL) & OCOTP_CTRL_BUSY) {
    };

    tmp = mmio_read_32(base + OCOTP_CTRL);

    if (tmp & OCOTP_CTRL_ERROR) {
        mmio_write_32(base + OCOTP_CTRL, tmp & ~OCOTP_CTRL_ERROR);
        return -PB_ERR_IO;
    }

    /* Ref manual says this should be 0x3F, but FSL drivers says 0x7F...*/
    tmp = ((bank * wpb) + row) & 0x7F;

    mmio_write_32(base + OCOTP_CTRL, tmp);
    mmio_write_32(base + OCOTP_READ_CTRL, 1);

    /* Wait for busy flag */
    while (mmio_read_32(base + OCOTP_CTRL) & OCOTP_CTRL_BUSY) {
    };

    *value = mmio_read_32(base + OCOTP_READ_FUSE_DATA);

    return PB_OK;
}
