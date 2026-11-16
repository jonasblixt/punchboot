/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/fuse/imx_ocotp.h>
#include <pb/delay.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <string.h>

static uintptr_t base; /* 0: not set up, every call is -PB_ERR_STATE */
static uint32_t wpb;
static uint32_t timing; /* 0: no ipg_clk rate, no writes */
/* CTRL's BUSY and ERROR, and the widest address: the SoC's layout. The
 * i.MX8M Plus has them one bit higher than the 6UL (F-467) */
static uint32_t ctrl_busy = OCOTP_CTRL_BUSY;
static uint32_t ctrl_error = OCOTP_CTRL_ERROR;
static uint32_t ctrl_addr_mask = OCOTP_CTRL_ADDR_MASK;

/*
 * TIMING's fields count ipg_clk periods (6UL RM 35.5.2): the relax time is
 * RELAX + 1 periods, the read strobe (STROBE_READ + 1) - 2 * (RELAX + 1),
 * the program strobe (STROBE_PROG + 1) - 2 * (RELAX + 1), WAIT the interval
 * between an auto read and a write. Its reset value, 0x01461299, is for an
 * ipg_clk of 66 MHz, and pb wrote it at every rate (F-196): 10.03 us of
 * program strobe at 66 MHz, 3 to 9 us at the RT1060's 75 to 200 MHz.
 *
 * Each time below is the reset value's, in periods of 66 MHz. It is scaled
 * to the rate and rounded up, so no time is shorter than at 66 MHz, and at
 * 66 MHz TIMING is the reset value.
 */
#define OCOTP_REF_KHZ   66000
#define OCOTP_REF_RELAX 2 /* RELAX 1 */
#define OCOTP_REF_READ  3 /* STROBE_READ 6 */
#define OCOTP_REF_PROG  662 /* STROBE_PROG 0x299: 10.03 us */
#define OCOTP_REF_WAIT  5 /* WAIT 5 */
#define OCOTP_MAX_KHZ   1000000 /* Above this the fields overflow */

/* ref periods of 66 MHz in periods of rate_khz, rounded up. In 32 bits:
 * the firmware has no 64 bit division */
static uint32_t ocotp_periods(uint32_t ref, uint32_t rate_khz)
{
    return (ref * rate_khz + OCOTP_REF_KHZ - 1) / OCOTP_REF_KHZ;
}

static int ocotp_timing(unsigned int ipg_rate_hz, uint32_t *out)
{
    uint32_t rate_khz = ipg_rate_hz / 1000 + (ipg_rate_hz % 1000 != 0);
    uint32_t relax, strobe_read, strobe_prog, wait;

    if (rate_khz > OCOTP_MAX_KHZ)
        return -PB_ERR_PARAM;

    relax = ocotp_periods(OCOTP_REF_RELAX, rate_khz) - 1;
    strobe_read = ocotp_periods(OCOTP_REF_READ, rate_khz) + 2 * (relax + 1) - 1;
    strobe_prog = ocotp_periods(OCOTP_REF_PROG, rate_khz) + 2 * (relax + 1) - 1;
    wait = ocotp_periods(OCOTP_REF_WAIT, rate_khz);

    if (relax > 0xf || strobe_read > 0x3f || strobe_prog > 0xfff || wait > 0x3f)
        return -PB_ERR_PARAM;

    *out = OCOTP_TIMING_WAIT(wait) | OCOTP_TIMING_STROBE_READ(strobe_read) |
           OCOTP_TIMING_RELAX(relax) | OCOTP_TIMING_STROBE_PROG(strobe_prog);
    return PB_OK;
}

#ifdef CONFIG_IMX_OCOTP_DEBUG
static void ocotp_dump_fuse(uint32_t bank, uint32_t row)
{
    uint32_t val;
    uint32_t ret;

    ret = imx_ocotp_read(bank, row, &val);
    printf("fuse %i %i: %i, 0x%x\n\r", bank, row, ret, val);
}
#endif

int imx_ocotp_init(uintptr_t base_,
                   unsigned int words_per_bank,
                   unsigned int ipg_rate_hz,
                   unsigned int flags)
{
    uint32_t timing_ = 0;
    int rc;

    /* Set up only once this init works (drivers.md 2.1) */
    base = 0;

    LOG_DBG("Initializing ocotp at 0x%" PRIxPTR, base_);

    if (words_per_bank < 4) {
        LOG_ERR("Invalid words_per_bank setting");
        return -PB_ERR_PARAM;
    }

    if (ipg_rate_hz != 0) {
        rc = ocotp_timing(ipg_rate_hz, &timing_);

        if (rc != PB_OK) {
            LOG_ERR("No fuse timing for an ipg_clk of %u Hz", ipg_rate_hz);
            return rc;
        }
    }

    wpb = words_per_bank;
    timing = timing_;

    if (flags & IMX_OCOTP_CTRL_8MP) {
        ctrl_busy = OCOTP_CTRL_BUSY_8MP;
        ctrl_error = OCOTP_CTRL_ERROR_8MP;
        ctrl_addr_mask = OCOTP_CTRL_ADDR_MASK_8MP;
    } else {
        ctrl_busy = OCOTP_CTRL_BUSY;
        ctrl_error = OCOTP_CTRL_ERROR;
        ctrl_addr_mask = OCOTP_CTRL_ADDR_MASK;
    }

    base = base_;

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

/*
 * BUSY's bound (drivers.md 3). Not from the RM: the longest operation is a
 * word's program, at most 32 program strobes of 10 us if the bits are
 * programmed one at a time, a third of a millisecond. 10 ms leaves room.
 */
#define OCOTP_BUSY_TIMEOUT_us 10000

static int ocotp_wait_busy(void)
{
    struct pb_timeout to;

    pb_timeout_init_us(&to, OCOTP_BUSY_TIMEOUT_us);

    while (mmio_read_32(base + OCOTP_CTRL) & ctrl_busy) {
        if (pb_timeout_has_expired(&to)) {
            LOG_ERR("Timeout");
            return -PB_ERR_TIMEOUT;
        }
    }

    return PB_OK;
}

/* The word's address in CTRL. One that the field can't hold is refused, not
 * masked onto another word */
static int ocotp_addr(uint32_t bank, uint32_t row, uint32_t *addr)
{
    uint64_t index = (uint64_t)bank * wpb + row;

    if (index > ctrl_addr_mask) {
        LOG_ERR("Fuse %" PRIu32 ", %" PRIu32 " is out of range", bank, row);
        return -PB_ERR_PARAM;
    }

    *addr = (uint32_t)index;
    return PB_OK;
}

int imx_ocotp_write(uint32_t bank, uint32_t row, uint32_t value)
{
    uint32_t tmp = 0;
    uint32_t addr;
    int rc;

    if (base == 0)
        return -PB_ERR_STATE;

    rc = ocotp_addr(bank, row, &addr);

    if (rc != PB_OK)
        return rc;

    if (timing == 0) {
        LOG_ERR("No ipg_clk rate, no fuse timing");
        return -PB_ERR_NOT_SUPPORTED;
    }

    rc = ocotp_wait_busy();

    if (rc != PB_OK)
        return rc;

    tmp = mmio_read_32(base + OCOTP_CTRL);

    if (tmp & ctrl_error) {
        mmio_write_32(base + OCOTP_CTRL_CLR, ctrl_error);
        return -PB_ERR_IO;
    }

    mmio_write_32(base + OCOTP_TIMING, timing);

    rc = ocotp_wait_busy();

    if (rc != PB_OK)
        return rc;

    tmp = mmio_read_32(base + OCOTP_CTRL);

    if (tmp & ctrl_error) {
        mmio_write_32(base + OCOTP_CTRL_CLR, ctrl_error);
        return -PB_ERR_IO;
    }

    mmio_write_32(base + OCOTP_CTRL, OCOTP_CTRL_WR_UNLOCK(OCOTP_CTRL_WR_KEY) | addr);
    mmio_write_32(base + OCOTP_DATA, value);

    rc = ocotp_wait_busy();

    if (rc != PB_OK)
        return rc;

    /* The write postamble: 2 us after BUSY cleared before the next access
     * (6UL RM 35.3.1.4) */
    pb_delay_us(2);

    tmp = mmio_read_32(base + OCOTP_CTRL);
    if (tmp & ctrl_error) {
        mmio_write_32(base + OCOTP_CTRL_CLR, ctrl_error);
        return -PB_ERR_IO;
    }

    /* The word is read back, from the array (6UL RM 35.3.1.2, the shadow
     * registers lag until a reload): a bit the write set that isn't there
     * didn't take, and the write fails (F-166). Bits that were set before
     * stay set, fuses only go from 0 to 1 */
    rc = imx_ocotp_read(bank, row, &tmp);

    if (rc != PB_OK) {
        LOG_ERR("Fuse %" PRIu32 " can't be read back (%i)", addr, rc);
        return rc;
    }

    if ((tmp & value) != value) {
        LOG_ERR("Fuse %" PRIu32 ": wrote 0x%08" PRIx32 ", reads 0x%08" PRIx32, addr, value, tmp);
        return -PB_ERR_IO;
    }

    return PB_OK;
}

int imx_ocotp_read(uint32_t bank, uint32_t row, uint32_t *value)
{
    uint32_t tmp = 0;
    uint32_t addr;
    int rc;

    if (base == 0)
        return -PB_ERR_STATE;

    rc = ocotp_addr(bank, row, &addr);

    if (rc != PB_OK)
        return rc;

    rc = ocotp_wait_busy();

    if (rc != PB_OK)
        return rc;

    tmp = mmio_read_32(base + OCOTP_CTRL);

    if (tmp & ctrl_error) {
        mmio_write_32(base + OCOTP_CTRL_CLR, ctrl_error);
        return -PB_ERR_IO;
    }

    mmio_write_32(base + OCOTP_CTRL, addr);
    mmio_write_32(base + OCOTP_READ_CTRL, 1);

    rc = ocotp_wait_busy();

    if (rc != PB_OK)
        return rc;

    /* A locked word sets ERROR and reads 0xBADABADA */
    if (mmio_read_32(base + OCOTP_CTRL) & ctrl_error) {
        mmio_write_32(base + OCOTP_CTRL_CLR, ctrl_error);
        LOG_ERR("Fuse %" PRIu32 " can't be read", addr);
        return -PB_ERR_IO;
    }

    *value = mmio_read_32(base + OCOTP_READ_FUSE_DATA);

    return PB_OK;
}

int imx_ocotp_read_unique_id(uint32_t bank,
                             uint32_t row1,
                             uint32_t row2,
                             uint8_t *output,
                             size_t *length)
{
    union {
        uint32_t uid[2];
        uint8_t uid_bytes[8];
    } unique;

    int rc;

    if (*length < sizeof(unique.uid_bytes))
        return -PB_ERR_BUF_TOO_SMALL;

    /* A read that fails writes nothing: the device's UUID is made of this */
    rc = imx_ocotp_read(bank, row1, &unique.uid[0]);

    if (rc != PB_OK)
        return rc;

    rc = imx_ocotp_read(bank, row2, &unique.uid[1]);

    if (rc != PB_OK)
        return rc;

    memcpy(output, unique.uid_bytes, sizeof(unique.uid_bytes));
    *length = sizeof(unique.uid_bytes);
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
