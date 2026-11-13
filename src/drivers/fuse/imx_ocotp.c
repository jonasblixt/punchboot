/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/fuse/imx_ocotp.h>
#include <pb/delay.h>
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
        mmio_write_32(base + OCOTP_CTRL_CLR, OCOTP_CTRL_ERROR);
        return -PB_ERR_IO;
    }

    mmio_write_32(base + OCOTP_TIMING,
                  OCOTP_TIMING_WAIT(5) | OCOTP_TIMING_STROBE_READ(6) | OCOTP_TIMING_RELAX(1) |
                      OCOTP_TIMING_STROBE_PROG(0x299));

    /* Wait for busy flag */
    while (mmio_read_32(base + OCOTP_CTRL) & OCOTP_CTRL_BUSY) {
    };

    tmp = mmio_read_32(base + OCOTP_CTRL);

    if (tmp & OCOTP_CTRL_ERROR) {
        mmio_write_32(base + OCOTP_CTRL_CLR, OCOTP_CTRL_ERROR);
        return -PB_ERR_IO;
    }

    tmp = ((bank * wpb) + row) & OCOTP_CTRL_ADDR_MASK;

    mmio_write_32(base + OCOTP_CTRL, OCOTP_CTRL_WR_UNLOCK(OCOTP_CTRL_WR_KEY) | tmp);
    mmio_write_32(base + OCOTP_DATA, value);

    /* Wait for busy flag */
    while (mmio_read_32(base + OCOTP_CTRL) & OCOTP_CTRL_BUSY) {
    };

    /* The write postamble: 2 us after BUSY cleared before the next access
     * (6UL RM 35.3.1.4) */
    pb_delay_us(2);

    tmp = mmio_read_32(base + OCOTP_CTRL);
    if (tmp & OCOTP_CTRL_ERROR) {
        mmio_write_32(base + OCOTP_CTRL_CLR, OCOTP_CTRL_ERROR);
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
        mmio_write_32(base + OCOTP_CTRL_CLR, OCOTP_CTRL_ERROR);
        return -PB_ERR_IO;
    }

    tmp = ((bank * wpb) + row) & OCOTP_CTRL_ADDR_MASK;

    mmio_write_32(base + OCOTP_CTRL, tmp);
    mmio_write_32(base + OCOTP_READ_CTRL, 1);

    /* Wait for busy flag */
    while (mmio_read_32(base + OCOTP_CTRL) & OCOTP_CTRL_BUSY) {
    };

    *value = mmio_read_32(base + OCOTP_READ_FUSE_DATA);

    return PB_OK;
}

int imx_ocotp_fuse_read(uint32_t index, uint32_t *value)
{
    return imx_ocotp_read(0, index, value);
}

int imx_ocotp_fuse_write(uint32_t index, uint32_t value)
{
    return imx_ocotp_write(0, index, value);
}

int imx_ocotp_lock(uint32_t sw_sticky, bool lock_scs)
{
    if (base == 0)
        return -PB_ERR_STATE;

    mmio_clrsetbits_32(base + OCOTP_SW_STICKY, 0, sw_sticky);

    if (lock_scs)
        mmio_write_32(base + OCOTP_SCS_SET, OCOTP_SCS_LOCK);

    if ((mmio_read_32(base + OCOTP_SW_STICKY) & sw_sticky) != sw_sticky) {
        LOG_ERR("SW_STICKY %08" PRIx32 " didn't lock", mmio_read_32(base + OCOTP_SW_STICKY));
        return -PB_ERR_IO;
    }

    if (lock_scs && !(mmio_read_32(base + OCOTP_SCS) & OCOTP_SCS_LOCK)) {
        LOG_ERR("SCS %08" PRIx32 " didn't lock", mmio_read_32(base + OCOTP_SCS));
        return -PB_ERR_IO;
    }

    return PB_OK;
}
