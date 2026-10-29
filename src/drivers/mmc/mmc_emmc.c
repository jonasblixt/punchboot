/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This driver is based on the mmc driver in arm-trusted-firmware.
 *
 * Copyright (c) 2018-2022, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * e.MMC: identification, bus modes, EXT_CSD, the hardware partitions
 * (JESD84-B51).
 */

#include <drivers/mmc/mmc_core.h>
#include <pb/bio.h>
#include <pb/delay.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <string.h>

#include "mmc_core_private.h"

/* EXT_CSD_PART_CONFIG */
#define PART_CONFIG_ACCESS_MASK 0x07 /* Cleared at power on */
#define PART_CONFIG_BOOT_SHIFT  3 /* Kept at power on */
#define PART_CONFIG_BOOT_MASK   (0x07 << PART_CONFIG_BOOT_SHIFT)

#define SEND_OP_COND_TIMEOUT_us 1000000 /* Power up takes at most 1 s */
#define SEND_OP_COND_POLL_us    1000
#define COMPAT_CLK_MAX_HZ       MHz(26) /* Backward compatible timing, JESD84-B51 6.6.2 */

/* BOOT_PARTITION_ENABLE of the first e.MMC, see mmc_get_boot_part_config() */
static int mmc_boot_part_config = -PB_ERR_STATE;
static const struct mmc_device_config *mmc_boot_cfg;

#ifdef CONFIG_MMC_CORE_HS200_TUNE
/* Tuning: a window narrower than this means a marginal board */
#define TUNE_MIN_WINDOW 8

static uint8_t mmc_tuning_rsp[128] __aligned(64);
static const uint8_t tuning_blk_pattern_4bit[] = {
    0xff, 0x0f, 0xff, 0x00, 0xff, 0xcc, 0xc3, 0xcc, 0xc3, 0x3c, 0xcc, 0xff, 0xfe, 0xff, 0xfe, 0xef,
    0xff, 0xdf, 0xff, 0xdd, 0xff, 0xfb, 0xff, 0xfb, 0xbf, 0xff, 0x7f, 0xff, 0x77, 0xf7, 0xbd, 0xef,
    0xff, 0xf0, 0xff, 0xf0, 0x0f, 0xfc, 0xcc, 0x3c, 0xcc, 0x33, 0xcc, 0xcf, 0xff, 0xef, 0xff, 0xee,
    0xff, 0xfd, 0xff, 0xfd, 0xdf, 0xff, 0xbf, 0xff, 0xbb, 0xff, 0xf7, 0xff, 0xf7, 0x7f, 0x7b, 0xde,
};

static const uint8_t tuning_blk_pattern_8bit[] = {
    0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc, 0xcc,
    0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff, 0xff, 0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee, 0xff,
    0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd, 0xdd, 0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff, 0xbb,
    0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff, 0xff, 0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee, 0xff,
    0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc,
    0xcc, 0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff, 0xff, 0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee,
    0xff, 0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd, 0xdd, 0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff,
    0xbb, 0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff, 0xff, 0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee,
};
#endif

/*
 * The CSD TRAN_SPEED (bits 103:96, JESD84-B51 7.3.10), the clock limit of the
 * backward compatible timing. 0 if the field holds a reserved value.
 */
static unsigned int csd_tran_speed_hz(const uint32_t csd[4])
{
    /* Bits 6:3, the multiplier times 10 */
    static const uint8_t mult_x10[16] = { 0,  10, 12, 13, 15, 20, 26, 30,
                                          35, 40, 45, 52, 55, 60, 70, 80 };
    /* Bits 2:0, the unit in Hz divided by 10 */
    static const uint32_t unit_div10[4] = { 10000, 100000, 1000000, 10000000 };
    uint8_t tran_speed = csd[3] & 0xff;
    unsigned int unit = tran_speed & 0x07;

    if (unit >= ARRAY_SIZE(unit_div10))
        return 0;

    return unit_div10[unit] * mult_x10[(tran_speed >> 3) & 0x0f];
}

static int mmc_send_op_cond(struct mmc_device *dev)
{
    int rc;
    mmc_cmd_resp_t resp_data;
    struct pb_timeout to;

    rc = mmc_reset_to_idle(dev);
    if (rc != 0) {
        return rc;
    }

    /* Polled often: the init goes on as soon as the device is up */
    pb_timeout_init_us(&to, SEND_OP_COND_TIMEOUT_us);

    do {
        rc = mmc_send_cmd(dev,
                          MMC_CMD_SEND_OP_COND,
                          OCR_SECTOR_MODE | OCR_VDD_MIN_2V7 | OCR_VDD_MIN_1V7,
                          MMC_RSP_R3,
                          &resp_data[0]);
        if (rc != 0) {
            return rc;
        }

        if ((resp_data[0] & OCR_POWERUP) != 0U) {
            dev->ocr = resp_data[0];
            return 0;
        }

        pb_delay_us(SEND_OP_COND_POLL_us);
    } while (!pb_timeout_has_expired(&to));

    LOG_ERR("CMD1: the device is still busy after 1 s");

    return -PB_ERR_TIMEOUT;
}

static int mmc_extcsd_reload(struct mmc_device *dev)
{
    struct mmc_cmd cmd = {
        .idx = MMC_CMD_SEND_EXT_CSD,
        .buf = dev->ext_csd,
        .block_size = sizeof(dev->ext_csd),
        .blocks = 1,
    };
    int ret;

    dev->info.block_size = MMC_BLOCK_SIZE;

    ret = mmc_request_r1(dev, &cmd);
    if (ret != 0) {
        return ret;
    }

    return mmc_wait_state(dev, MMC_STATE_TRAN, MMC_DEFAULT_TIMEOUT_ms);
}

static int mmc_set_ext_csd(struct mmc_device *dev,
                           unsigned int ext_cmd,
                           unsigned int value,
                           unsigned int timeout)
{
    int ret;

    if (timeout == 0) {
        timeout = dev->generic_cmd6_time_ms;
    }

    ret = mmc_send_cmd(dev,
                       MMC_CMD_SWITCH,
                       EXTCSD_WRITE_BYTES | EXTCSD_CMD(ext_cmd) | EXTCSD_VALUE(value) |
                           EXTCSD_CMD_SET_NORMAL,
                       MMC_RSP_R1b,
                       NULL);
    if (ret != 0) {
        return ret;
    }

    do {
        ret = mmc_device_state(dev, timeout);
        if (ret < 0) {
            return ret;
        }
    } while (ret == MMC_STATE_PRG);

    return 0;
}

int mmc_extcsd_write(struct mmc_device *dev, uint16_t field_id, uint8_t value)
{
    if (field_id >= sizeof(dev->ext_csd))
        return -PB_ERR_PARAM;

    return mmc_set_ext_csd(dev, field_id, value, 0);
}

int mmc_extcsd_read(struct mmc_device *dev, uint16_t field_id, uint8_t *value)
{
    if (value == NULL)
        return -PB_ERR_PARAM;
    if (field_id >= sizeof(dev->ext_csd))
        return -PB_ERR_PARAM;

    *value = dev->ext_csd[field_id];
    return 0;
}

static int mmc_set_bus_width(struct mmc_device *dev, enum mmc_bus_width bus_width)
{
    int ret;
    uint8_t value = EXT_CSD_BUS_WIDTH_1;

    switch (bus_width) {
    case MMC_BUS_WIDTH_1BIT:
        value = EXT_CSD_BUS_WIDTH_1;
        break;
    case MMC_BUS_WIDTH_4BIT:
        value = EXT_CSD_BUS_WIDTH_4;
        break;
    case MMC_BUS_WIDTH_8BIT:
        value = EXT_CSD_BUS_WIDTH_8;
        break;
    case MMC_BUS_WIDTH_4BIT_DDR:
        value = EXT_CSD_BUS_WIDTH_4_DDR;
        break;
    case MMC_BUS_WIDTH_8BIT_DDR:
        value = EXT_CSD_BUS_WIDTH_8_DDR;
        break;
    case MMC_BUS_WIDTH_8BIT_DDR_STROBE:
        value = EXT_CSD_BUS_WIDTH_8_DDR | EXT_CSD_BUS_WIDTH_STROBE;
        break;
    default:
        return -PB_ERR_PARAM;
    }

    ret = mmc_set_ext_csd(dev, EXT_CSD_BUS_WIDTH, value, 0);
    if (ret != 0) {
        return ret;
    }

    return dev->hal->set_bus_width(dev->hal->priv, bus_width);
}

static int mmc_part_switch(struct mmc_device *dev, enum mmc_part part);

static struct mmc_device *bio_mmc(bio_dev_t bdev)
{
    return (struct mmc_device *)bio_get_private(bdev);
}

static enum mmc_part bio_part(bio_dev_t bdev)
{
    int flags = bio_get_hal_flags(bdev);

    if (flags & MMC_BIO_FLAG_BOOT0)
        return MMC_PART_BOOT0;
    else if (flags & MMC_BIO_FLAG_BOOT1)
        return MMC_PART_BOOT1;
    else if (flags & MMC_BIO_FLAG_RPMB)
        return MMC_PART_RPMB;
    else
        return MMC_PART_USER;
}

/*
 * BOOT_PARTITION_ENABLE: the partition the ROM boots from. It is kept at
 * power on, so it only changes when a boot partition is written (F-082).
 */
static int set_boot_part(struct mmc_device *dev, enum mmc_part part)
{
    uint8_t value = (dev->part_config & ~PART_CONFIG_BOOT_MASK) | (part << PART_CONFIG_BOOT_SHIFT);
    int rc;

    if (value == dev->part_config)
        return PB_OK;

    LOG_INFO("Boot partition: %s", (part == MMC_PART_BOOT0) ? "BOOT0" : "BOOT1");

    rc = mmc_set_ext_csd(dev, EXT_CSD_PART_CONFIG, value, dev->partition_switch_time_ms);

    if (rc == PB_OK)
        dev->part_config = value;

    return rc;
}

/*
 * A completed write makes the boot partition bootable. Until then, the ROM
 * boots the other one, see mmc_bio_write().
 */
static int mmc_bio_write_done(bio_dev_t bdev, int param)
{
    (void)param;
    return set_boot_part(bio_mmc(bdev), bio_part(bdev));
}

static int mmc_bio_read(bio_dev_t bdev, lba_t lba, size_t length, void *buf)
{
    struct mmc_device *dev = bio_mmc(bdev);
    int rc;

    rc = mmc_part_switch(dev, bio_part(bdev));
    if (rc != PB_OK)
        return rc;

    return mmc_bio_xfer(dev, lba, length, (uintptr_t)buf, false);
}

static int mmc_bio_write(bio_dev_t bdev, lba_t lba, size_t length, const void *buf)
{
    struct mmc_device *dev = bio_mmc(bdev);
    int rc;

    /*
     * Never write the boot partition the ROM boots from. If the write is
     * interrupted, the ROM boots the other one until a write completes
     * (mmc_bio_write_done()), and the ROM has no fallback of its own.
     */
    enum mmc_part part = bio_part(bdev);
    uint8_t boot_part = (dev->part_config & PART_CONFIG_BOOT_MASK) >> PART_CONFIG_BOOT_SHIFT;

    if (part == MMC_PART_BOOT0 && boot_part == MMC_PART_BOOT0)
        rc = set_boot_part(dev, MMC_PART_BOOT1);
    else if (part == MMC_PART_BOOT1 && boot_part == MMC_PART_BOOT1)
        rc = set_boot_part(dev, MMC_PART_BOOT0);
    else
        rc = PB_OK;

    if (rc != PB_OK)
        return rc;

    rc = mmc_part_switch(dev, part);
    if (rc != PB_OK)
        return rc;

    return mmc_bio_xfer(dev, lba, length, (uintptr_t)buf, true);
}

#ifdef CONFIG_MMC_CORE_HS200_TUNE
/* One CMD21 at a delay tap: does the tuning block arrive intact? */
static bool tune_tap_ok(struct mmc_device *dev, unsigned int tap)
{
    bool bus8 = dev->cfg->width == MMC_BUS_WIDTH_8BIT;
    const uint8_t *pattern = bus8 ? tuning_blk_pattern_8bit : tuning_blk_pattern_4bit;
    size_t len = bus8 ? sizeof(tuning_blk_pattern_8bit) : sizeof(tuning_blk_pattern_4bit);
    struct mmc_cmd cmd = {
        .idx = MMC_CMD_SEND_TUNING_BLOCK_HS200,
        .buf = mmc_tuning_rsp,
        .block_size = len,
        .blocks = 1,
        .quiet = true,
    };

    dev->hal->set_delay_tap(dev->hal->priv, tap);
    memset(mmc_tuning_rsp, 0, sizeof(mmc_tuning_rsp));
    if (mmc_request_r1(dev, &cmd) != PB_OK)
        return false;
    return memcmp(mmc_tuning_rsp, pattern, len) == 0;
}

/*
 * Tries every delay tap with a tuning block (CMD21, JESD84-B51 6.6.5.1) and
 * uses the middle of the widest run that works, the most margin for drift.
 * A CMD21 takes about 15 us at 200 MHz, so 128 taps are about 2 ms; a
 * coarse sweep misses the narrow failing gaps (two taps on the imx8mpevk).
 * Returns -PB_ERR_NOT_FOUND if no tap works.
 */
static int hs200_tune(struct mmc_device *dev)
{
    unsigned int taps = dev->hal->delay_taps;
    unsigned int start = plat_get_us_tick();
    int best_lo = -1, best_hi = -1, run_lo = -1;

    if (taps == 0 || dev->hal->set_delay_tap == NULL)
        return PB_OK;

    for (unsigned int t = 0; t < taps; t++) {
        bool ok = tune_tap_ok(dev, t);

        if (ok && run_lo < 0)
            run_lo = t;
        if (run_lo >= 0 && (!ok || t + 1 == taps)) {
            int run_hi = ok ? (int)t : (int)t - 1;

            if (best_lo < 0 || run_hi - run_lo > best_hi - best_lo) {
                best_lo = run_lo;
                best_hi = run_hi;
            }
            run_lo = -1;
        }
    }

    if (best_lo < 0) {
        LOG_ERR("HS200 tuning: no delay tap works");
        return -PB_ERR_NOT_FOUND;
    }

    unsigned int width = best_hi - best_lo + 1;
    unsigned int tap = (best_lo + best_hi) / 2;

    if (width < TUNE_MIN_WINDOW)
        LOG_WARN("HS200 tuning: only taps %i-%i work", best_lo, best_hi);
    LOG_INFO("HS200 tuning: taps %i-%i, using %u (%u us)",
             best_lo,
             best_hi,
             tap,
             plat_get_us_tick() - start);

    return dev->hal->set_delay_tap(dev->hal->priv, tap);
}
#endif // CONFIG_MMC_CORE_HS200_TUNE

/* The DEVICE_TYPE bits that allow a bus mode */
static uint8_t mode_card_types(enum mmc_bus_mode mode)
{
    switch (mode) {
    case MMC_BUS_MODE_DDR52:
        return EXT_CSD_CARD_TYPE_DDR52_1V8 | EXT_CSD_CARD_TYPE_DDR52_1V2;
    case MMC_BUS_MODE_HS200:
        return EXT_CSD_CARD_TYPE_HS200_1V8 | EXT_CSD_CARD_TYPE_HS200_1V2;
    case MMC_BUS_MODE_HS400:
        return EXT_CSD_CARD_TYPE_HS400_1V8 | EXT_CSD_CARD_TYPE_HS400_1V2;
    default:
        return 0;
    }
}

/* An EXT_CSD timeout field, in 10 ms units */
static unsigned int ext_csd_time_ms(struct mmc_device *dev, unsigned int index)
{
    if (dev->ext_csd[index] == 0)
        return MMC_CMD6_TIME_MAX_ms;
    return dev->ext_csd[index] * 10;
}

static int mmc_select_bus_mode(struct mmc_device *dev)
{
    const struct mmc_device_config *cfg = dev->cfg;
    int rc;

    switch (cfg->mode) {
    case MMC_BUS_MODE_DDR52: {
        LOG_DBG("Switching to HS DDR52");
        /* Switch to HS, 52MHz DDR MHz */
        rc = mmc_set_ext_csd(dev, EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS, 0);
        if (rc != PB_OK) {
            LOG_ERR("Could not switch to HS timing");
            return rc;
        }

        rc = mmc_set_bus_width(dev, cfg->width);
        if (rc != 0) {
            return rc;
        }

        rc = mmc_set_bus_clock(dev, MHz(52));
        if (rc != 0) {
            return rc;
        }
    } break;
    case MMC_BUS_MODE_HS200: {
        LOG_DBG("Switching to HS200");
        rc = mmc_set_bus_width(dev, cfg->width);
        if (rc != 0) {
            return rc;
        }

        /* Switch to HS200, 200 MHz */
        rc = mmc_set_ext_csd(dev, EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS200, 0);
        if (rc != PB_OK) {
            LOG_ERR("Could not switch to HS200 timing");
            return rc;
        }

        rc = mmc_set_bus_clock(dev, MHz(200));
        if (rc != 0) {
            return rc;
        }
#ifdef CONFIG_MMC_CORE_HS200_TUNE
        rc = hs200_tune(dev);
        if (rc == -PB_ERR_NOT_FOUND) {
            /* Boot slower rather than not at all: high speed needs no tuning */
            LOG_WARN("HS200 tuning failed, using high speed (52 MHz)");
            dev->hal->set_delay_tap(dev->hal->priv, dev->hal->delay_tap_default);
            /* CMD6 in HS200, the clock down, then the status (JESD84-B51 6.6.2.3) */
            rc = mmc_send_cmd(dev,
                              MMC_CMD_SWITCH,
                              EXTCSD_WRITE_BYTES | EXTCSD_CMD(EXT_CSD_HS_TIMING) |
                                  EXTCSD_VALUE(EXT_CSD_TIMING_HS) | EXTCSD_CMD_SET_NORMAL,
                              MMC_RSP_R1b,
                              NULL);
            if (rc == PB_OK)
                rc = mmc_set_bus_clock(dev, MHz(52));
            if (rc == PB_OK)
                rc = mmc_wait_state(dev, MMC_STATE_TRAN, dev->generic_cmd6_time_ms);
        }
        if (rc != 0)
            return rc;
#endif
    } break;
    case MMC_BUS_MODE_HS400: {
        /* TODO: This is not complete and not usable yet,
         * Strobe DLL configuration is missing. It seems to work
         * on the imx8qxpmek evk */
        LOG_DBG("Switching to HS400");
        /* Before we can switch the bus to 8-bit DDR the device must
         * be in HS mode */
        rc = mmc_set_ext_csd(dev, EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS, 0);
        if (rc != PB_OK) {
            LOG_ERR("Could not switch to HS timing");
            return rc;
        }

        rc = mmc_set_bus_width(dev, cfg->width);
        if (rc != 0) {
            return rc;
        }

        /* Switch to HS400, 200 MHz (DDR) */
        rc = mmc_set_ext_csd(dev, EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS400, 0);
        if (rc != PB_OK) {
            LOG_ERR("Could not switch to HS400 timing");
            return rc;
        }

        rc = mmc_set_bus_clock(dev, MHz(200));
        if (rc != 0) {
            return rc;
        }
    } break;
    default:
        LOG_ERR("Unsupported mmc bus mode (%i)", cfg->mode);
        return -PB_ERR_NOT_IMPLEMENTED;
    }

    return PB_OK;
}

/* A bio device for a hardware partition */
static int mmc_add_part(struct mmc_device *dev,
                        lba_t last_lba,
                        const unsigned char *uu,
                        const char *name,
                        uint8_t hal_flag,
                        uint16_t flags,
                        bool boot)
{
    bio_dev_t d = bio_allocate(0, last_lba, MMC_BLOCK_SIZE, uu, name);
    int rc;

    if (d < 0)
        return d;

    rc = bio_set_hal_flags(d, hal_flag);
    if (rc < 0)
        return rc;

    if (flags) {
        rc = bio_set_flags(d, flags);
        if (rc < 0)
            return rc;
    }

    rc = bio_set_private(d, (uintptr_t)dev);
    if (rc < 0)
        return rc;

    rc = bio_set_ios(d, mmc_bio_read, mmc_bio_write);
    if (rc < 0)
        return rc;

    if (boot)
        return bio_set_write_done_cb(d, mmc_bio_write_done);

    return PB_OK;
}

int mmc_emmc_setup(struct mmc_device *dev)
{
    const struct mmc_device_config *cfg = dev->cfg;
    int rc;
    mmc_cmd_resp_t resp_data;

    LOG_DBG("eMMC init");
    dev->generic_cmd6_time_ms = MMC_CMD6_TIME_MAX_ms;
    dev->power_off_long_time_ms = MMC_CMD6_TIME_MAX_ms;
    dev->partition_switch_time_ms = MMC_CMD6_TIME_MAX_ms;

    rc = dev->hal->init(dev->hal->priv);

    if (rc != PB_OK)
        return rc;

    rc = mmc_set_bus_clock(dev, MMC_ID_CLK_HZ);
    if (rc != 0) {
        return rc;
    }

    rc = mmc_send_op_cond(dev);

    if (rc != 0) {
        return rc;
    }

    /* Devices of up to 2 GiB use byte addresses, JESD84-B51 6.4.2 */
    dev->block_addressing = (dev->ocr & OCR_ACCESS_MODE_MASK) == OCR_SECTOR_MODE;

    /* CMD2: Card Identification */
    rc = mmc_send_cmd(dev, MMC_CMD_ALL_SEND_CID, 0, MMC_RSP_R2, NULL);
    if (rc != 0) {
        return rc;
    }

    /* CMD3: Set Relative Address */
    dev->rca = MMC_FIX_RCA;
    rc = mmc_send_cmd(
        dev, MMC_CMD_SET_RELATIVE_ADDR, dev->rca << RCA_SHIFT_OFFSET, MMC_RSP_R1, NULL);
    if (rc != 0) {
        return rc;
    }

    /* CMD9: CSD Register */
    rc = mmc_send_cmd(dev, MMC_CMD_SEND_CSD, dev->rca << RCA_SHIFT_OFFSET, MMC_RSP_R2, resp_data);
    if (rc != 0) {
        return rc;
    }

    memcpy(dev->csd, resp_data, sizeof(dev->csd));

    LOG_DBG("csd0: %x", resp_data[0]);
    LOG_DBG("csd1: %x", resp_data[1]);
    LOG_DBG("csd2: %x", resp_data[2]);
    LOG_DBG("csd3: %x", resp_data[3]);

    /* Out of identification mode and the CSD read: the clock can go up to
     * TRAN_SPEED, JESD84-B51 6.6. Everything up to the bus mode switch,
     * EXT_CSD included, runs at it instead of 400 kHz. */
    unsigned int clk_hz = csd_tran_speed_hz(dev->csd);

    if (clk_hz == 0 || clk_hz > COMPAT_CLK_MAX_HZ)
        clk_hz = COMPAT_CLK_MAX_HZ;

    rc = mmc_set_bus_clock(dev, clk_hz);
    if (rc != 0) {
        return rc;
    }

    /* CMD7: Select Card */
    rc = mmc_send_cmd(dev, MMC_CMD_SELECT_CARD, dev->rca << RCA_SHIFT_OFFSET, MMC_RSP_R1, NULL);
    if (rc != 0) {
        return rc;
    }

    rc = mmc_wait_state(dev, MMC_STATE_TRAN, MMC_DEFAULT_TIMEOUT_ms);
    if (rc != PB_OK)
        return rc;

    /* The device must support the mode, JESD84-B51 6.6.2 */
    rc = mmc_extcsd_reload(dev);
    if (rc != PB_OK)
        return rc;

    if (!(dev->ext_csd[EXT_CSD_CARD_TYPE] & mode_card_types(cfg->mode))) {
        LOG_ERR("Bus mode %i not supported, DEVICE_TYPE 0x%02x",
                cfg->mode,
                dev->ext_csd[EXT_CSD_CARD_TYPE]);
        return -PB_ERR_NOT_SUPPORTED;
    }

    rc = mmc_select_bus_mode(dev);
    if (rc != PB_OK)
        return rc;

    rc = mmc_extcsd_reload(dev);
    if (rc != 0) {
        return rc;
    }

    dev->generic_cmd6_time_ms = ext_csd_time_ms(dev, EXT_CSD_GENERIC_CMD6_TIME);
    dev->power_off_long_time_ms = ext_csd_time_ms(dev, EXT_CSD_POWER_OFF_LONG_TIME);
    dev->partition_switch_time_ms = ext_csd_time_ms(dev, EXT_CSD_PART_SWITCH_TIME);

    LOG_DBG("Generic CMD6 timeout: %ums", dev->generic_cmd6_time_ms);
    LOG_DBG("Power off long time: %ums", dev->power_off_long_time_ms);
    LOG_DBG("Partition switch time: %ums", dev->partition_switch_time_ms);

    if (!(cfg->flags & MMC_CFG_FLAG_KEEP_BOOT_BUS_CONDITIONS) &&
        dev->ext_csd[EXT_CSD_BOOT_BUS_CONDITIONS] != cfg->boot_mode) {
        LOG_INFO("Updating boot bus conditions to 0x%02x", cfg->boot_mode);

        rc = mmc_set_ext_csd(dev, EXT_CSD_BOOT_BUS_CONDITIONS, cfg->boot_mode, 0);
        if (rc != PB_OK) {
            LOG_ERR("Could not update boot bus conditions");
            return rc;
        }
    }

    dev->part_config = dev->ext_csd[EXT_CSD_PART_CONFIG];
    dev->part = dev->part_config & PART_CONFIG_ACCESS_MASK;

    /*
     * Record BOOT_PARTITION_ENABLE of the first e.MMC before we switch
     * partitions, see mmc_get_boot_part_config().
     */
    if (mmc_boot_part_config == -PB_ERR_STATE) {
        mmc_boot_cfg = cfg;
        switch ((dev->part_config & PART_CONFIG_BOOT_MASK) >> PART_CONFIG_BOOT_SHIFT) {
        case 1:
            mmc_boot_part_config = MMC_PART_BOOT0;
            LOG_INFO("BOOT_PARTITION_ENABLE: BOOT0");
            break;
        case 2:
            mmc_boot_part_config = MMC_PART_BOOT1;
            LOG_INFO("BOOT_PARTITION_ENABLE: BOOT1");
            break;
        case 0:
        case 7:
            mmc_boot_part_config = MMC_PART_USER;
            LOG_INFO("BOOT_PARTITION_ENABLE: User");
            break;
        default:
            LOG_ERR("Bad PARTITION_CONFIG: 0x%x", dev->part_config);
            break;
        }
    }

    dev->blocks = (uint32_t)dev->ext_csd[EXT_CSD_SEC_CNT + 0] << 0 |
                  (uint32_t)dev->ext_csd[EXT_CSD_SEC_CNT + 1] << 8 |
                  (uint32_t)dev->ext_csd[EXT_CSD_SEC_CNT + 2] << 16 |
                  (uint32_t)dev->ext_csd[EXT_CSD_SEC_CNT + 3] << 24;

    lba_t boot_part_last_lba = 0;
    lba_t rpmb_part_last_lba = 0;

#ifdef CONFIG_MMC_CORE_OVERRIDE_BOOT_PART_SZ
    boot_part_last_lba = CONFIG_MMC_CORE_BOOT_PART_SZ_KiB * 2 - 1;
#else
    boot_part_last_lba = dev->ext_csd[EXT_CSD_BOOT_MULT] * 256 - 1;
#endif

    rpmb_part_last_lba = dev->ext_csd[EXT_CSD_RPMB_MULT] * 256 - 1;

    rc = mmc_add_part(dev,
                      boot_part_last_lba,
                      cfg->boot0_uu,
                      "eMMC BOOT0",
                      MMC_BIO_FLAG_BOOT0,
                      BIO_FLAG_VISIBLE | BIO_FLAG_WRITABLE,
                      true);
    if (rc < 0)
        return rc;

    rc = mmc_add_part(dev,
                      boot_part_last_lba,
                      cfg->boot1_uu,
                      "eMMC BOOT1",
                      MMC_BIO_FLAG_BOOT1,
                      BIO_FLAG_VISIBLE | BIO_FLAG_WRITABLE,
                      true);
    if (rc < 0)
        return rc;

    /* RPMB is currently R/O and only accessible from within punchboot */
    rc = mmc_add_part(
        dev, rpmb_part_last_lba, cfg->rpmb_uu, "eMMC RPMB", MMC_BIO_FLAG_RPMB, 0, false);
    if (rc < 0)
        return rc;

    rc = mmc_add_part(dev,
                      dev->blocks - 1,
                      cfg->user_uu,
                      "eMMC USER",
                      MMC_BIO_FLAG_USER,
                      BIO_FLAG_VISIBLE | BIO_FLAG_WRITABLE,
                      false);
    if (rc < 0)
        return rc;

    LOG_DBG("eMMC ready");
    return PB_OK;
}

static int mmc_part_switch(struct mmc_device *dev, enum mmc_part part)
{
    uint8_t value = 0;
    int rc;
#if LOG_MAX_LEVEL >= 3
    const char *part_names[] = {
        "User",
        "Boot0",
        "Boot1",
        "RPMB",
    };
#endif

    if ((unsigned int)part >= MMC_PART_END)
        return -PB_ERR_IO;

    /* PARTITION_ACCESS only, the boot partition and BOOT_ACK stay */
    value = (dev->part_config & ~PART_CONFIG_ACCESS_MASK) | part;

    if (value == dev->part_config) {
        dev->part = part;
        return PB_OK;
    }

    LOG_DBG("Switching to %s", part_names[part]);
    rc = mmc_set_ext_csd(dev, EXT_CSD_PART_CONFIG, value, dev->partition_switch_time_ms);

    if (rc == PB_OK) {
        dev->part_config = value;
        dev->part = part;
    }

    return rc;
}

int mmc_get_boot_part_config(uuid_t part_uu)
{
    const unsigned char *uu;

    switch (mmc_boot_part_config) {
    case MMC_PART_BOOT0:
        uu = mmc_boot_cfg->boot0_uu;
        break;
    case MMC_PART_BOOT1:
        uu = mmc_boot_cfg->boot1_uu;
        break;
    case MMC_PART_USER:
        uu = mmc_boot_cfg->user_uu;
        break;
    default:
        return -PB_ERR_STATE;
    }

    memcpy(part_uu, uu, sizeof(uuid_t));
    return PB_OK;
}

int mmc_power_off(struct mmc_device *dev)
{
    int rc;

    /**
     * Power off notification register:
     *
     * 0 - Host does not support power off notifications (default and reset at POR)
     * 1 - Host must notify card before cutting power
     * 2 - Power off short, timeout is the generic cmd6 timeout
     * 3 - Power off long, timeout is defined by 'power_off_long_time_ms'
     * 4 - Sleep notification
     */

    /**
     * From POR the power off notification register is set to zero, indicating
     * that the host is will not send any heads up to the card when power is about
     * to be cut.
     *
     * We must first tell the card that the host is going to send the power off
     * notification message by changing this register to '1'
     */
    rc = mmc_set_ext_csd(dev, EXT_CSD_POWER_OFF_NOTIFICATION, 0x01, 0);

    if (rc != 0)
        return rc;

    /*
     * Any command after POWER_OFF_LONG, CMD13 too, sets it back to
     * POWERED_ON (JESD84-B51 6.6.36). The HAL waits for the end of busy
     * on DAT0 (R1b), the card can then be powered off.
     */
    return mmc_send_cmd(dev,
                        MMC_CMD_SWITCH,
                        EXTCSD_WRITE_BYTES | EXTCSD_CMD(EXT_CSD_POWER_OFF_NOTIFICATION) |
                            EXTCSD_VALUE(0x03) | EXTCSD_CMD_SET_NORMAL,
                        MMC_RSP_R1b,
                        NULL);
}
