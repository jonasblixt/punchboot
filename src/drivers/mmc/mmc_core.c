/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This driver is based on the mmc driver in arm-trusted-firmware.
 *
 * Copyright (c) 2018-2022, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/mmc/mmc_core.h>
#include <pb/bio.h>
#include <pb/delay.h>
#include <pb/pb.h>
#include <pb/timestamp.h>
#include <string.h>

#define MMC_BIO_FLAG_BOOT0       BIT(0)
#define MMC_BIO_FLAG_BOOT1       BIT(1)
#define MMC_BIO_FLAG_RPMB        BIT(2)
#define MMC_BIO_FLAG_USER        BIT(3)

/* EXT_CSD_PART_CONFIG */
#define PART_CONFIG_ACCESS_MASK  0x07 /* Cleared at power on */
#define PART_CONFIG_BOOT_SHIFT   3 /* Kept at power on */
#define PART_CONFIG_BOOT_MASK    (0x07 << PART_CONFIG_BOOT_SHIFT)

#define MMC_DEFAULT_TIMEOUT_ms   15
#define SEND_OP_COND_MAX_RETRIES 100

static const struct mmc_hal *mmc_hal;
static const struct mmc_device_config *mmc_cfg;
static uint32_t mmc_ocr_value;
static struct mmc_device_info mmc_dev_info;
static uint32_t rca;
static struct mmc_csd_emmc mmc_csd;
static uint8_t mmc_ext_csd[512] __aligned(64);
static uint8_t mmc_current_part_config;
static enum mmc_part mmc_current_part;
static int mmc_boot_part_config = -PB_ERR_STATE;
/* The largest value of the EXT_CSD timeout fields, used until they are read and when they are
 * 0, "not defined" (JESD84-B51 7.4.31, 7.4.32, 7.4.56) */
#define MMC_CMD6_TIME_MAX_ms 2550

static unsigned int power_off_long_time_ms;
static unsigned int generic_cmd6_time_ms;
static unsigned int partition_switch_time_ms;

#ifdef CONFIG_MMC_CORE_HS200_TUNE
static uint8_t mmc_tuning_rsp[128] __aligned(16);
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

static int mmc_request(struct mmc_cmd *cmd)
{
    int rc;

#ifdef CONFIG_MMC_CORE_DEBUG_CMDS
    LOG_DBG("idx %u, arg 0x%08x, 0x%x", cmd->idx, cmd->arg, cmd->resp_type);
#endif

    rc = mmc_hal->request(cmd);

    if (rc != PB_OK) {
        LOG_ERR("Send command %u error: %i", cmd->idx, rc);
    }

    return rc;
}

static int mmc_send_cmd(uint16_t cmd_idx, uint32_t arg, uint16_t resp_type, mmc_cmd_resp_t result)
{
    struct mmc_cmd cmd = {
        .idx = cmd_idx,
        .arg = arg,
        .resp_type = resp_type,
    };
    int rc = mmc_request(&cmd);

    if (rc == PB_OK && result != NULL)
        memcpy(result, cmd.resp, sizeof(mmc_cmd_resp_t));
    return rc;
}

/* A command with an R1 response, failing on the error bits it reports */
static int mmc_request_r1(struct mmc_cmd *cmd)
{
    int rc;

    cmd->resp_type = MMC_RSP_R1;
    rc = mmc_request(cmd);
    if (rc != PB_OK)
        return rc;

    if (cmd->resp[0] & STATUS_ERROR_MASK) {
        LOG_ERR("CMD%u card status 0x%08x", cmd->idx, cmd->resp[0]);
        return -PB_ERR_IO;
    }

    return PB_OK;
}

static int mmc_send_cmd_r1(uint16_t cmd_idx, uint32_t arg)
{
    struct mmc_cmd cmd = {
        .idx = cmd_idx,
        .arg = arg,
    };

    return mmc_request_r1(&cmd);
}

static int mmc_reset_to_idle(void)
{
    int rc;

    /* CMD0: reset to IDLE */
    rc = mmc_send_cmd(MMC_CMD_GO_IDLE_STATE, 0, 0, NULL);
    if (rc != PB_OK) {
        return rc;
    }

    pb_delay_ms(2);

    return PB_OK;
}

static int mmc_send_op_cond(void)
{
    int rc, n;
    mmc_cmd_resp_t resp_data;

    rc = mmc_reset_to_idle();
    if (rc != 0) {
        return rc;
    }

    for (n = 0; n < SEND_OP_COND_MAX_RETRIES; n++) {
        rc = mmc_send_cmd(MMC_CMD_SEND_OP_COND,
                          OCR_SECTOR_MODE | OCR_VDD_MIN_2V7 | OCR_VDD_MIN_1V7,
                          MMC_RSP_R3,
                          &resp_data[0]);
        if (rc != 0) {
            return rc;
        }

        if ((resp_data[0] & OCR_POWERUP) != 0U) {
            mmc_ocr_value = resp_data[0];
            return 0;
        }

        pb_delay_ms(10);
    }

    LOG_ERR("CMD1 failed after %d retries", SEND_OP_COND_MAX_RETRIES);

    return -PB_ERR_IO;
}

static int mmc_device_state(unsigned int timeout)
{
    mmc_cmd_resp_t resp_data;

    while (true) {
        int ret;

        if (timeout == 0) {
            LOG_ERR("CMD13 timeout");
            return -PB_ERR_IO;
        }

        ret = mmc_send_cmd(MMC_CMD_SEND_STATUS, rca << RCA_SHIFT_OFFSET, MMC_RSP_R1, resp_data);

        if (ret == PB_OK) {
            if ((resp_data[0] & STATUS_ERROR_MASK) != 0U) {
                LOG_ERR("Card status 0x%08x", resp_data[0]);
                return -PB_ERR_IO;
            }

            if ((resp_data[0] & STATUS_READY_FOR_DATA) != 0U)
                break;
        }

        timeout--;
        pb_delay_ms(1);
    }

    return MMC_GET_STATE(resp_data[0]);
}

int mmc_extcsd_reload(void)
{
    int ret = 0;

    mmc_dev_info.block_size = MMC_BLOCK_SIZE;

    struct mmc_cmd cmd = {
        .idx = MMC_CMD_SEND_EXT_CSD,
        .buf = mmc_ext_csd,
        .block_size = sizeof(mmc_ext_csd),
        .blocks = 1,
    };

    ret = mmc_request_r1(&cmd);
    if (ret != 0) {
        return ret;
    }

    do {
        ret = mmc_device_state(MMC_DEFAULT_TIMEOUT_ms);
        if (ret < 0) {
            return ret;
        }
    } while (ret != MMC_STATE_TRAN);

    return PB_OK;
}

static int mmc_set_ext_csd(unsigned int ext_cmd, unsigned int value, unsigned int timeout)
{
    int ret;

    if (timeout == 0) {
        timeout = generic_cmd6_time_ms;
    }

    ret = mmc_send_cmd(MMC_CMD_SWITCH,
                       EXTCSD_WRITE_BYTES | EXTCSD_CMD(ext_cmd) | EXTCSD_VALUE(value) |
                           EXTCSD_CMD_SET_NORMAL,
                       MMC_RSP_R1b,
                       NULL);
    if (ret != 0) {
        return ret;
    }

    do {
        ret = mmc_device_state(timeout);
        if (ret < 0) {
            return ret;
        }
    } while (ret == MMC_STATE_PRG);

    return 0;
}

int mmc_extcsd_write(uint16_t field_id, uint8_t value)
{
    if (field_id >= sizeof(mmc_ext_csd))
        return -PB_ERR_PARAM;

    return mmc_set_ext_csd(field_id, value, 0);
}

int mmc_extcsd_read(uint16_t field_id, uint8_t *value)
{
    if (value == NULL)
        return -PB_ERR_PARAM;
    if (field_id >= sizeof(mmc_ext_csd))
        return -PB_ERR_PARAM;

    *value = mmc_ext_csd[field_id];
    return 0;
}

static int mmc_set_bus_clock(unsigned int clk_hz)
{
    return mmc_hal->set_bus_clock(clk_hz);
}

static int mmc_set_bus_width(enum mmc_bus_width bus_width)
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

    ret = mmc_set_ext_csd(EXT_CSD_BUS_WIDTH, value, 0);
    if (ret != 0) {
        return ret;
    }

    return mmc_hal->set_bus_width(bus_width);
}

static enum mmc_part bio_part(bio_dev_t dev)
{
    int flags = bio_get_hal_flags(dev);

    if (flags & MMC_BIO_FLAG_BOOT0)
        return MMC_PART_BOOT0;
    else if (flags & MMC_BIO_FLAG_BOOT1)
        return MMC_PART_BOOT1;
    else if (flags & MMC_BIO_FLAG_RPMB)
        return MMC_PART_RPMB;
    else
        return MMC_PART_USER;
}

static int select_part(bio_dev_t dev)
{
    return mmc_part_switch(bio_part(dev));
}

/*
 * BOOT_PARTITION_ENABLE: the partition the ROM boots from. It is kept at
 * power on, so it only changes when a boot partition is written (F-082).
 */
static int set_boot_part(enum mmc_part part)
{
    uint8_t value = (mmc_current_part_config & ~PART_CONFIG_BOOT_MASK) |
                    (part << PART_CONFIG_BOOT_SHIFT);
    int rc;

    if (value == mmc_current_part_config)
        return PB_OK;

    LOG_INFO("Boot partition: %s", (part == MMC_PART_BOOT0) ? "BOOT0" : "BOOT1");

    rc = mmc_set_ext_csd(EXT_CSD_PART_CONFIG, value, partition_switch_time_ms);

    if (rc == PB_OK)
        mmc_current_part_config = value;

    return rc;
}

/*
 * A completed write makes the boot partition bootable. Until then, the ROM
 * boots the other one, see mmc_bio_write().
 */
static int mmc_bio_write_done(bio_dev_t dev, int param)
{
    (void)param;
    return set_boot_part(bio_part(dev));
}

static int mmc_bio_read(bio_dev_t dev, lba_t lba, size_t length, void *buf)
{
    int rc = -1;
    uintptr_t buf_ptr = (uintptr_t)buf;
    size_t bytes_to_read = length;
    ssize_t block_sz = bio_block_size(dev);
    lba_t lba_offset = lba;

    if (block_sz < 0)
        return block_sz;

    rc = select_part(dev);
    if (rc != PB_OK)
        return rc;

    size_t max_len = mmc_hal->max_chunk_bytes;
    while (bytes_to_read) {
        size_t chunk_len = (bytes_to_read > max_len) ? max_len : bytes_to_read;
        rc = mmc_read(lba_offset, chunk_len, buf_ptr);

        if (rc < 0)
            break;

        bytes_to_read -= chunk_len;
        buf_ptr += chunk_len;
        lba_offset += chunk_len / block_sz;
    }

    return rc;
}

static int mmc_bio_write(bio_dev_t dev, lba_t lba, size_t length, const void *buf)
{
    int rc = -1;
    uintptr_t buf_ptr = (uintptr_t)buf;
    size_t bytes_to_write = length;
    ssize_t block_sz = bio_block_size(dev);
    lba_t lba_offset = lba;

    if (block_sz < 0)
        return block_sz;

    /*
     * Never write the boot partition the ROM boots from. If the write is
     * interrupted, the ROM boots the other one until a write completes
     * (mmc_bio_write_done()), and the ROM has no fallback of its own.
     */
    enum mmc_part part = bio_part(dev);
    uint8_t boot_part = (mmc_current_part_config & PART_CONFIG_BOOT_MASK) >> PART_CONFIG_BOOT_SHIFT;

    if (part == MMC_PART_BOOT0 && boot_part == MMC_PART_BOOT0)
        rc = set_boot_part(MMC_PART_BOOT1);
    else if (part == MMC_PART_BOOT1 && boot_part == MMC_PART_BOOT1)
        rc = set_boot_part(MMC_PART_BOOT0);
    else
        rc = PB_OK;

    if (rc != PB_OK)
        return rc;

    rc = select_part(dev);
    if (rc != PB_OK)
        return rc;

    size_t max_len = mmc_hal->max_chunk_bytes;
    while (bytes_to_write) {
        size_t chunk_len = (bytes_to_write > max_len) ? max_len : bytes_to_write;
        rc = mmc_write(lba_offset, chunk_len, buf_ptr);

        if (rc < 0)
            break;

        bytes_to_write -= chunk_len;
        buf_ptr += chunk_len;
        lba_offset += chunk_len / block_sz;
    }

    return rc;
}

#ifdef CONFIG_MMC_CORE_HS200_TUNE
static int hs200_tune(void)
{
    int rc;
    uint8_t result[127];

    /*
     * TODO: Currently hardcoded tap start = 0 and tap end = 127.
     * This should be passed as configuration from the mmc layer.
     *
     * We probably want:
     *  - delay_tap_start
     *  - delay_tap_end
     *  - delay_tap_step
     *
     *  This also assumes 8-bit bus width.
     */

    LOG_INFO("Starting, this will produce I/O errors for bad delay taps...");
    for (int i = 0; i < 127; i++) {
        mmc_hal->set_delay_tap(i);
        pb_delay_ms(10);
        struct mmc_cmd cmd = {
            .idx = MMC_CMD_SEND_TUNING_BLOCK_HS200,
            .buf = mmc_tuning_rsp,
            .block_size = sizeof(mmc_tuning_rsp),
            .blocks = 1,
        };

        rc = mmc_request_r1(&cmd);
        if (rc != 0) {
            goto tune_tap_fail;
        }

        do {
            rc = mmc_device_state(MMC_DEFAULT_TIMEOUT_ms);
            if (rc < 0) {
                goto tune_tap_fail;
            }
        } while (rc != MMC_STATE_TRAN);

        if (memcmp(tuning_blk_pattern_8bit, mmc_tuning_rsp, 128) == 0) {
            result[i] = 1;
        } else {
            goto tune_tap_fail;
        }
        continue;
tune_tap_fail:
        result[i] = 0;
    }

    LOG_INFO("Done");

    unsigned int start_tap = 0;
    bool find_next_good = true;
    unsigned int high_score = 0;
    unsigned int selected_tap;
    size_t good_count = 0;

    for (int i = 0; i < 127; i++) {
        good_count += result[i];

        if (find_next_good) {
            if (result[i]) {
                find_next_good = false;
                start_tap = i;
            }
        } else {
            if (!result[i] || (i == 127)) {
                unsigned int end_tap = i - 1;
                unsigned int pass_count = end_tap - start_tap;
                unsigned int center_tap = start_tap + pass_count / 2;
                LOG_DBG("Pass count = %i, [%03i - %03i] center tap = %i",
                        pass_count,
                        start_tap,
                        end_tap,
                        center_tap);

                if (pass_count > high_score) {
                    high_score = pass_count;
                    selected_tap = center_tap;
                }
                find_next_good = true;
                start_tap = 0;
            }
        }
    }

    if (good_count == 0) {
        LOG_ERR("Failed");
        return -PB_ERR;
    }

    LOG_INFO("Optimal delay tap = %i", selected_tap);
    mmc_hal->set_delay_tap(selected_tap);

    return PB_OK;
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
static unsigned int ext_csd_time_ms(unsigned int index)
{
    if (mmc_ext_csd[index] == 0)
        return MMC_CMD6_TIME_MAX_ms;
    return mmc_ext_csd[index] * 10;
}

static int mmc_setup(void)
{
    int rc;
    mmc_cmd_resp_t resp_data;
    ts("mmc enum start");
    generic_cmd6_time_ms = MMC_CMD6_TIME_MAX_ms;
    power_off_long_time_ms = MMC_CMD6_TIME_MAX_ms;
    partition_switch_time_ms = MMC_CMD6_TIME_MAX_ms;

    rc = mmc_hal->init();

    if (rc != PB_OK)
        return rc;

    rc = mmc_set_bus_clock(400 * 1000);
    if (rc != 0) {
        return rc;
    }

    ts("mmc opcond start");
    rc = mmc_send_op_cond();
    ts("mmc opcond end");

    if (rc != 0) {
        return rc;
    }

    /* CMD2: Card Identification */
    rc = mmc_send_cmd(MMC_CMD_ALL_SEND_CID, 0, MMC_RSP_R2, NULL);
    if (rc != 0) {
        return rc;
    }

    /* CMD3: Set Relative Address */
    rca = MMC_FIX_RCA;
    rc = mmc_send_cmd(MMC_CMD_SET_RELATIVE_ADDR, rca << RCA_SHIFT_OFFSET, MMC_RSP_R1, NULL);
    if (rc != 0) {
        return rc;
    }

    /* CMD9: CSD Register */
    rc = mmc_send_cmd(MMC_CMD_SEND_CSD, rca << RCA_SHIFT_OFFSET, MMC_RSP_R2, resp_data);
    if (rc != 0) {
        return rc;
    }

    memcpy(&mmc_csd, &resp_data, sizeof(resp_data));

    LOG_DBG("csd0: %x", resp_data[0]);
    LOG_DBG("csd1: %x", resp_data[1]);
    LOG_DBG("csd2: %x", resp_data[2]);
    LOG_DBG("csd3: %x", resp_data[3]);

    /* CMD7: Select Card */
    rc = mmc_send_cmd(MMC_CMD_SELECT_CARD, rca << RCA_SHIFT_OFFSET, MMC_RSP_R1, NULL);
    if (rc != 0) {
        return rc;
    }

    do {
        rc = mmc_device_state(MMC_DEFAULT_TIMEOUT_ms);
        if (rc < 0) {
            return rc;
        }
    } while (rc != MMC_STATE_TRAN);

    /* The device must support the mode, JESD84-B51 6.6.2 */
    rc = mmc_extcsd_reload();
    if (rc != PB_OK)
        return rc;

    if (!(mmc_ext_csd[EXT_CSD_CARD_TYPE] & mode_card_types(mmc_cfg->mode))) {
        LOG_ERR("Bus mode %i not supported, DEVICE_TYPE 0x%02x",
                mmc_cfg->mode,
                mmc_ext_csd[EXT_CSD_CARD_TYPE]);
        return -PB_ERR_NOT_SUPPORTED;
    }

    switch (mmc_cfg->mode) {
    case MMC_BUS_MODE_DDR52: {
        LOG_DBG("Switching to HS DDR52");
        /* Switch to HS, 52MHz DDR MHz */
        rc = mmc_set_ext_csd(EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS, 0);
        if (rc != PB_OK) {
            LOG_ERR("Could not switch to HS timing");
            return rc;
        }

        rc = mmc_set_bus_width(mmc_cfg->width);
        if (rc != 0) {
            return rc;
        }

        rc = mmc_set_bus_clock(MHz(52));
        if (rc != 0) {
            return rc;
        }
    } break;
    case MMC_BUS_MODE_HS200: {
        LOG_DBG("Switching to HS200");
        rc = mmc_set_bus_width(mmc_cfg->width);
        if (rc != 0) {
            return rc;
        }

        /* Switch to HS200, 200 MHz */
        rc = mmc_set_ext_csd(EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS200, 0);
        if (rc != PB_OK) {
            LOG_ERR("Could not switch to HS200 timing");
            return rc;
        }

        rc = mmc_set_bus_clock(MHz(200));
        if (rc != 0) {
            return rc;
        }
#ifdef CONFIG_MMC_CORE_HS200_TUNE
        ts("hs200 tune begin");
        rc = hs200_tune();
        ts("hs200 tune end");
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
        rc = mmc_set_ext_csd(EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS, 0);
        if (rc != PB_OK) {
            LOG_ERR("Could not switch to HS timing");
            return rc;
        }

        rc = mmc_set_bus_width(mmc_cfg->width);
        if (rc != 0) {
            return rc;
        }

        /* Switch to HS400, 200 MHz (DDR) */
        rc = mmc_set_ext_csd(EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS400, 0);
        if (rc != PB_OK) {
            LOG_ERR("Could not switch to HS400 timing");
            return rc;
        }

        rc = mmc_set_bus_clock(MHz(200));
        if (rc != 0) {
            return rc;
        }
    } break;
    default:
        LOG_ERR("Unsupported mmc bus mode (%i)", mmc_cfg->mode);
        return -PB_ERR_NOT_IMPLEMENTED;
    }

    rc = mmc_extcsd_reload();
    if (rc != 0) {
        return rc;
    }

    generic_cmd6_time_ms = ext_csd_time_ms(EXT_CSD_GENERIC_CMD6_TIME);
    power_off_long_time_ms = ext_csd_time_ms(EXT_CSD_POWER_OFF_LONG_TIME);
    partition_switch_time_ms = ext_csd_time_ms(EXT_CSD_PART_SWITCH_TIME);

    LOG_DBG("Generic CMD6 timeout: %ums", generic_cmd6_time_ms);
    LOG_DBG("Power off long time: %ums", power_off_long_time_ms);
    LOG_DBG("Partition switch time: %ums", partition_switch_time_ms);

    if (mmc_ext_csd[EXT_CSD_BOOT_BUS_CONDITIONS] != mmc_cfg->boot_mode) {
        LOG_INFO("Updating boot bus conditions to 0x%02x", mmc_cfg->boot_mode);

        rc = mmc_set_ext_csd(EXT_CSD_BOOT_BUS_CONDITIONS, mmc_cfg->boot_mode, 0);
        if (rc != PB_OK) {
            LOG_ERR("Could not update boot bus conditions");
            return rc;
        }
    }

    mmc_current_part_config = mmc_ext_csd[EXT_CSD_PART_CONFIG];

    /*
     * Record BOOT_PARTITION_ENABLE before we switch partitions, see
     * mmc_get_boot_part_config().
     */
    if (mmc_boot_part_config == -PB_ERR_STATE) {
        switch ((mmc_current_part_config & PART_CONFIG_BOOT_MASK) >> PART_CONFIG_BOOT_SHIFT) {
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
            LOG_ERR("Bad PARTITION_CONFIG: 0x%x", mmc_current_part_config);
            break;
        }
    }

    size_t sectors = mmc_ext_csd[EXT_CSD_SEC_CNT + 0] << 0 | mmc_ext_csd[EXT_CSD_SEC_CNT + 1] << 8 |
                     mmc_ext_csd[EXT_CSD_SEC_CNT + 2] << 16 |
                     mmc_ext_csd[EXT_CSD_SEC_CNT + 3] << 24;

    /*
        LOG_INFO("%zu sectors, %zu kBytes",
            sectors, sectors >> 1);
        LOG_INFO("Partconfig: %x", mmc_ext_csd[EXT_CSD_PART_CONFIG]);
        LOG_INFO("Boot partition size %u kB", mmc_ext_csd[EXT_CSD_BOOT_MULT] * 128);
        LOG_INFO("RPMB partition size %u kB", mmc_ext_csd[EXT_CSD_RPMB_MULT] * 128);
        LOG_INFO("HS timing: %u", mmc_ext_csd[EXT_CSD_HS_TIMING]);
        LOG_INFO("Device type: 0x%08x", mmc_ext_csd[EXT_CSD_CARD_TYPE]);
        LOG_INFO("Life time A (MLC) %x", mmc_ext_csd[EXT_CSD_DEVICE_LIFE_TIME_EST_TYP_A]);
        LOG_INFO("Life time B (SLC) %x", mmc_ext_csd[EXT_CSD_DEVICE_LIFE_TIME_EST_TYP_B]);
        LOG_INFO("Pre EOL %x", mmc_ext_csd[EXT_CSD_PRE_EOL_INFO]);
    */

    lba_t boot_part_last_lba = 0;
    lba_t rpmb_part_last_lba = 0;

#ifdef CONFIG_MMC_CORE_OVERRIDE_BOOT_PART_SZ
    boot_part_last_lba = CONFIG_MMC_CORE_BOOT_PART_SZ_KiB * 2 - 1;
#else
    boot_part_last_lba = mmc_ext_csd[EXT_CSD_BOOT_MULT] * 256 - 1;
#endif

    rpmb_part_last_lba = mmc_ext_csd[EXT_CSD_RPMB_MULT] * 256 - 1;

    bio_dev_t d = bio_allocate(0, boot_part_last_lba, 512, mmc_cfg->boot0_uu, "eMMC BOOT0");

    if (d < 0)
        return d;

    rc = bio_set_hal_flags(d, MMC_BIO_FLAG_BOOT0);
    if (rc < 0)
        return rc;

    rc = bio_set_flags(d, BIO_FLAG_VISIBLE | BIO_FLAG_WRITABLE);
    if (rc < 0)
        return rc;

    rc = bio_set_ios(d, mmc_bio_read, mmc_bio_write);

    if (rc < 0)
        return rc;

    rc = bio_set_write_done_cb(d, mmc_bio_write_done);

    if (rc < 0)
        return rc;

    d = bio_allocate(0, boot_part_last_lba, 512, mmc_cfg->boot1_uu, "eMMC BOOT1");

    if (d < 0)
        return d;

    rc = bio_set_hal_flags(d, MMC_BIO_FLAG_BOOT1);
    if (rc < 0)
        return rc;

    rc = bio_set_flags(d, BIO_FLAG_VISIBLE | BIO_FLAG_WRITABLE);
    if (rc < 0)
        return rc;

    rc = bio_set_ios(d, mmc_bio_read, mmc_bio_write);

    if (rc < 0)
        return rc;

    rc = bio_set_write_done_cb(d, mmc_bio_write_done);

    if (rc < 0)
        return rc;

    /* RPMB is currently R/O and only accessible from within punchboot */
    d = bio_allocate(0, rpmb_part_last_lba, 512, mmc_cfg->rpmb_uu, "eMMC RPMB");

    if (d < 0)
        return d;

    rc = bio_set_hal_flags(d, MMC_BIO_FLAG_RPMB);
    if (rc < 0)
        return rc;

    rc = bio_set_ios(d, mmc_bio_read, mmc_bio_write);

    if (rc < 0)
        return rc;

    d = bio_allocate(0, sectors - 1, 512, mmc_cfg->user_uu, "eMMC USER");

    if (d < 0)
        return d;

    rc = bio_set_hal_flags(d, MMC_BIO_FLAG_USER);
    if (rc < 0)
        return rc;

    rc = bio_set_flags(d, BIO_FLAG_VISIBLE | BIO_FLAG_WRITABLE);

    if (rc < 0)
        return rc;

    rc = bio_set_ios(d, mmc_bio_read, mmc_bio_write);

    if (rc < 0)
        return rc;

    ts("mmc enum done");
    return rc;
}

struct mmc_device_info *mmc_device_info(void)
{
    return &mmc_dev_info;
}

/* Devices of up to 2 GiB use byte addresses, JESD84-B51 6.4.2 */
static unsigned int mmc_data_address(unsigned int lba)
{
    if ((mmc_ocr_value & OCR_ACCESS_MODE_MASK) == OCR_SECTOR_MODE)
        return lba;

    return lba * MMC_BLOCK_SIZE;
}

static int mmc_rw(unsigned int lba, size_t length, uintptr_t buf, bool write)
{
    struct mmc_cmd cmd = {
        .arg = mmc_data_address(lba),
        .buf = (void *)buf,
        .block_size = MMC_BLOCK_SIZE,
        .blocks = length / MMC_BLOCK_SIZE,
        .write = write,
    };
    int ret;

#ifdef MMC_CORE_DEBUG_IOS
    LOG_DBG("%s %u, %zu, %p", write ? "write" : "read", lba, length, (void *)buf);
#endif

    if (mmc_hal->max_chunk_bytes > 0 && length > mmc_hal->max_chunk_bytes)
        return -PB_ERR_IO;

    /* Access to RPMB always uses the multiple blocks command even if there's
     * just one block to be handled. RPMB access also requires us to set
     * the block count. Because the standard says so.
     *
     * This seems to be different between manufacturers, at least some Micron
     * memories seem fine without this and just works when reading blocks.
     * Other memories refuse to do read accesses without this and the standard
     * also says that it should be done in this way.
     */
    if (mmc_current_part == MMC_PART_RPMB) {
        ret = mmc_send_cmd_r1(MMC_CMD_SET_BLOCK_COUNT, cmd.blocks);
        if (ret != 0) {
            return ret;
        }

        cmd.idx = write ? MMC_CMD_WRITE_MULTIPLE_BLOCK : MMC_CMD_READ_MULTIPLE_BLOCK;
    } else if (cmd.blocks > 1) {
        cmd.idx = write ? MMC_CMD_WRITE_MULTIPLE_BLOCK : MMC_CMD_READ_MULTIPLE_BLOCK;
        cmd.stop = true;
    } else {
        cmd.idx = write ? MMC_CMD_WRITE_SINGLE_BLOCK : MMC_CMD_READ_SINGLE_BLOCK;
    }

    ret = mmc_request_r1(&cmd);
    if (ret != 0) {
        return ret;
    }

    /* Wait buffer empty */
    do {
        ret = mmc_device_state(MMC_DEFAULT_TIMEOUT_ms);
        if (ret < 0) {
            return ret;
        }
    } while ((ret != MMC_STATE_TRAN) && (ret != (write ? MMC_STATE_RCV : MMC_STATE_DATA)));

    return PB_OK;
}

int mmc_read(unsigned int lba, size_t length, uintptr_t buf)
{
    return mmc_rw(lba, length, buf, false);
}

int mmc_write(unsigned int lba, size_t length, const uintptr_t buf)
{
    return mmc_rw(lba, length, buf, true);
}

int mmc_part_switch(enum mmc_part part)
{
    uint8_t value = 0;
    int rc;
#if LOGLEVEL >= 3
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
    value = (mmc_current_part_config & ~PART_CONFIG_ACCESS_MASK) | part;

    if (value == mmc_current_part_config) {
        mmc_current_part = part;
        return PB_OK;
    }

    LOG_DBG("Switching to %s", part_names[part]);
    rc = mmc_set_ext_csd(EXT_CSD_PART_CONFIG, value, partition_switch_time_ms);

    if (rc == PB_OK) {
        mmc_current_part_config = value;
        mmc_current_part = part;
    }

    return rc;
}

int mmc_get_boot_part_config(uuid_t part_uu)
{
    const unsigned char *uu;

    switch (mmc_boot_part_config) {
    case MMC_PART_BOOT0:
        uu = mmc_cfg->boot0_uu;
        break;
    case MMC_PART_BOOT1:
        uu = mmc_cfg->boot1_uu;
        break;
    case MMC_PART_USER:
        uu = mmc_cfg->user_uu;
        break;
    default:
        return -PB_ERR_STATE;
    }

    memcpy(part_uu, uu, sizeof(uuid_t));
    return PB_OK;
}

int mmc_power_off(void)
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
    rc = mmc_set_ext_csd(EXT_CSD_POWER_OFF_NOTIFICATION, 0x01, 0);

    if (rc != 0)
        return rc;

    /*
     * Any command after POWER_OFF_LONG, CMD13 too, sets it back to
     * POWERED_ON (JESD84-B51 6.6.36). The HAL waits for the end of busy
     * on DAT0 (R1b), the card can then be powered off.
     */
    return mmc_send_cmd(MMC_CMD_SWITCH,
                        EXTCSD_WRITE_BYTES | EXTCSD_CMD(EXT_CSD_POWER_OFF_NOTIFICATION) |
                            EXTCSD_VALUE(0x03) | EXTCSD_CMD_SET_NORMAL,
                        MMC_RSP_R1b,
                        NULL);
}

int mmc_init(const struct mmc_hal *hal, const struct mmc_device_config *cfg)
{
    if (!(cfg->mode > MMC_BUS_MODE_INVALID && cfg->mode < MMC_BUS_MODE_END) || (hal == NULL) ||
        (cfg == NULL)) {
        return -PB_ERR_PARAM;
    }

    mmc_hal = hal;
    mmc_cfg = cfg;

    return mmc_setup();
}
