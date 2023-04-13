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

#include <string.h>
#include <pb/pb.h>
#include <pb/delay.h>
#include <pb/timestamp.h>
#include <drivers/block/bio.h>
#include <drivers/mmc/mmc_core.h>

#define MMC_BIO_FLAG_BOOT0 BIT(0)
#define MMC_BIO_FLAG_BOOT1 BIT(1)
#define MMC_BIO_FLAG_RPMB BIT(2)
#define MMC_BIO_FLAG_USER BIT(3)

#define MMC_DEFAULT_MAX_RETRIES    5
#define SEND_OP_COND_MAX_RETRIES   100

static const struct mmc_hal *mmc_hal;
static const struct mmc_device_config *mmc_cfg;
static uint32_t mmc_ocr_value;
static struct mmc_device_info mmc_dev_info;
static uint32_t rca;
static struct mmc_csd_emmc mmc_csd;
static uint8_t mmc_ext_csd[512] __aligned(64);
static uint8_t mmc_current_part;

#ifdef CONFIG_MMC_CORE_HS200_TUNE
static uint8_t mmc_tuning_rsp[128] PB_ALIGN(16);
static const uint8_t tuning_blk_pattern_4bit[] = {
    0xff, 0x0f, 0xff, 0x00, 0xff, 0xcc, 0xc3, 0xcc,
    0xc3, 0x3c, 0xcc, 0xff, 0xfe, 0xff, 0xfe, 0xef,
    0xff, 0xdf, 0xff, 0xdd, 0xff, 0xfb, 0xff, 0xfb,
    0xbf, 0xff, 0x7f, 0xff, 0x77, 0xf7, 0xbd, 0xef,
    0xff, 0xf0, 0xff, 0xf0, 0x0f, 0xfc, 0xcc, 0x3c,
    0xcc, 0x33, 0xcc, 0xcf, 0xff, 0xef, 0xff, 0xee,
    0xff, 0xfd, 0xff, 0xfd, 0xdf, 0xff, 0xbf, 0xff,
    0xbb, 0xff, 0xf7, 0xff, 0xf7, 0x7f, 0x7b, 0xde,
};

static const uint8_t tuning_blk_pattern_8bit[] = {
    0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00,
    0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc, 0xcc,
    0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff, 0xff,
    0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee, 0xff,
    0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd, 0xdd,
    0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff, 0xbb,
    0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff, 0xff,
    0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee, 0xff,
    0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00,
    0x00, 0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc,
    0xcc, 0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff,
    0xff, 0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee,
    0xff, 0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd,
    0xdd, 0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff,
    0xbb, 0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff,
    0xff, 0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee,
};
#endif

static int mmc_send_cmd(uint16_t cmd_idx, uint32_t arg, uint16_t resp_type,
                        mmc_cmd_resp_t result)
{
    int rc;
    struct mmc_cmd cmd;
    mmc_cmd_resp_t rsp;

#ifdef CONFIG_MMC_CORE_DEBUG_CMDS
    LOG_DBG("idx %u, arg 0x%08x, 0x%x", cmd_idx, arg, resp_type);
#endif
    memset(&cmd, 0, sizeof(cmd));

    cmd.idx = cmd_idx;
    cmd.arg = arg;
    cmd.resp_type = resp_type;

    if (result != NULL) {
        memset(result, 0, sizeof(mmc_cmd_resp_t));
    }

    rc = mmc_hal->send_cmd(&cmd, rsp);

    if (rc != PB_OK) {
        LOG_ERR("Send command %u error: %i", cmd_idx, rc);
    }

    if (result != NULL) {
        memcpy(result, rsp, sizeof(mmc_cmd_resp_t));
    }

    return rc;
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
                    MMC_RSP_R3, &resp_data[0]);
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

static int mmc_device_state(void)
{
    int retries = MMC_DEFAULT_MAX_RETRIES;
    mmc_cmd_resp_t resp_data;

    do {
        int ret;

        if (retries == 0) {
            LOG_ERR("CMD13 failed after %d retries", MMC_DEFAULT_MAX_RETRIES);
            return -PB_ERR_IO;
        }

        ret = mmc_send_cmd(MMC_CMD_SEND_STATUS, rca << RCA_SHIFT_OFFSET,
                           MMC_RSP_R1, resp_data);
        if (ret != 0) {
            retries--;
            continue;
        }

        if ((resp_data[0] & STATUS_SWITCH_ERROR) != 0U) {
            LOG_ERR("resp_data[0] = 0x%08x", resp_data[0]);
            return -PB_ERR_IO;
        }

        retries--;
    } while ((resp_data[0] & STATUS_READY_FOR_DATA) == 0U);

    return MMC_GET_STATE(resp_data[0]);
}

static int mmc_fill_device_info(void)
{
    int ret = 0;

    mmc_dev_info.block_size = MMC_BLOCK_SIZE;

    ret = mmc_hal->prepare(0, sizeof(mmc_ext_csd),
                            (uintptr_t)mmc_ext_csd);
    if (ret != 0) {
        return ret;
    }

    ret = mmc_send_cmd(MMC_CMD_SEND_EXT_CSD, 0, MMC_RSP_R1, NULL);
    if (ret != 0) {
        return ret;
    }

    ret = mmc_hal->read(0, sizeof(mmc_ext_csd), (uintptr_t)mmc_ext_csd);
    if (ret != 0) {
        return ret;
    }

    do {
        ret = mmc_device_state();
        if (ret < 0) {
            return ret;
        }
    } while (ret != MMC_STATE_TRAN);

    return PB_OK;
}

static int mmc_set_ext_csd(unsigned int ext_cmd, unsigned int value)
{
    int ret;

    ret = mmc_send_cmd(MMC_CMD_SWITCH,
               EXTCSD_WRITE_BYTES | EXTCSD_CMD(ext_cmd) |
               EXTCSD_VALUE(value) | EXTCSD_CMD_SET_NORMAL,
               MMC_RSP_R1b, NULL);
    if (ret != 0) {
        return ret;
    }

    do {
        ret = mmc_device_state();
        if (ret < 0) {
            return ret;
        }
    } while (ret == MMC_STATE_PRG);

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

    ret = mmc_set_ext_csd(EXT_CSD_BUS_WIDTH, value);
    if (ret != 0) {
        return ret;
    }

    return mmc_hal->set_bus_width(bus_width);
}

static int mmc_bio_read(bio_dev_t dev, int lba, size_t length, uintptr_t buf)
{
    int rc = -1;
    uintptr_t buf_ptr = buf;
    size_t bytes_to_read = length;
    int lba_offset = lba;

    int flags = bio_get_hal_flags(dev);

    if (flags & MMC_BIO_FLAG_USER)
        mmc_part_switch(MMC_PART_USER);
    else if (flags & MMC_BIO_FLAG_BOOT0)
        mmc_part_switch(MMC_PART_BOOT0);
    else if (flags & MMC_BIO_FLAG_BOOT1)
        mmc_part_switch(MMC_PART_BOOT1);
    else if (flags & MMC_BIO_FLAG_RPMB)
        mmc_part_switch(MMC_PART_RPMB);

    size_t max_len = mmc_hal->max_chunk_bytes;
    while (bytes_to_read) {
        size_t chunk_len = (bytes_to_read > max_len)?max_len:bytes_to_read;
        rc =  mmc_read(lba_offset, chunk_len, buf_ptr);

        if (rc < 0)
            break;

        bytes_to_read -= chunk_len;
        buf_ptr += chunk_len;
        lba_offset += chunk_len / bio_block_size(dev);
    }

    mmc_part_switch(MMC_PART_USER);
    return rc;
}

static int mmc_bio_write(bio_dev_t dev, int lba, size_t length, const uintptr_t buf)
{
    int rc = -1;
    uintptr_t buf_ptr = buf;
    size_t bytes_to_write = length;
    int lba_offset = lba;
    int flags = bio_get_hal_flags(dev);

    if (flags & MMC_BIO_FLAG_USER)
        mmc_part_switch(MMC_PART_USER);
    else if (flags & MMC_BIO_FLAG_BOOT0)
        mmc_part_switch(MMC_PART_BOOT0);
    else if (flags & MMC_BIO_FLAG_BOOT1)
        mmc_part_switch(MMC_PART_BOOT1);
    else if (flags & MMC_BIO_FLAG_RPMB)
        mmc_part_switch(MMC_PART_RPMB);

    size_t max_len = mmc_hal->max_chunk_bytes;
    while (bytes_to_write) {
        size_t chunk_len = (bytes_to_write > max_len)?max_len:bytes_to_write;
        rc =  mmc_write(lba_offset, chunk_len, buf_ptr);

        if (rc < 0)
            break;

        bytes_to_write -= chunk_len;
        buf_ptr += chunk_len;
        lba_offset += chunk_len / bio_block_size(dev);
    }

    mmc_part_switch(MMC_PART_USER);
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
        rc = mmc_hal->prepare(0, sizeof(mmc_tuning_rsp), (uintptr_t)mmc_tuning_rsp);
        if (rc != 0) {
            goto tune_tap_fail;
        }

        rc = mmc_send_cmd(MMC_CMD_SEND_TUNING_BLOCK_HS200, 0, MMC_RSP_R1, NULL);

        if (rc != 0) {
            goto tune_tap_fail;
        }

        rc = mmc_hal->read(0, sizeof(mmc_tuning_rsp), (uintptr_t)mmc_tuning_rsp);
        if (rc != 0) {
            goto tune_tap_fail;
        }

        do {
            rc = mmc_device_state();
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

    for (int i = 0; i < 127; i++)  {
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
                unsigned int center_tap = start_tap + pass_count/2;
                LOG_DBG("Pass count = %i, [%03i - %03i] center tap = %i",
                           pass_count, start_tap, end_tap, center_tap);

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
#endif  // CONFIG_MMC_CORE_HS200_TUNE

static int mmc_setup(void)
{
    int rc;
    mmc_cmd_resp_t resp_data;
    ts("mmc enum start");
    rc = mmc_hal->init();

    if (rc != PB_OK)
        return rc;

    rc = mmc_set_bus_clock(400*1000);
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
    rc = mmc_send_cmd(MMC_CMD_SET_RELATIVE_ADDR, rca << RCA_SHIFT_OFFSET,
               MMC_RSP_R1, NULL);
    if (rc != 0) {
        return rc;
    }

    /* CMD9: CSD Register */
    rc = mmc_send_cmd(MMC_CMD_SEND_CSD, rca << RCA_SHIFT_OFFSET,
               MMC_RSP_R2, resp_data);
    if (rc != 0) {
        return rc;
    }

    memcpy(&mmc_csd, &resp_data, sizeof(resp_data));

/*
    LOG_DBG("csd0: %x", resp_data[0]);
    LOG_DBG("csd1: %x", resp_data[1]);
    LOG_DBG("csd2: %x", resp_data[2]);
    LOG_DBG("csd3: %x", resp_data[3]);
*/
    /* CMD7: Select Card */
    rc = mmc_send_cmd(MMC_CMD_SELECT_CARD, rca << RCA_SHIFT_OFFSET,
               MMC_RSP_R1, NULL);
    if (rc != 0) {
        return rc;
    }

    do {
        rc = mmc_device_state();
        if (rc < 0) {
            return rc;
        }
    } while (rc != MMC_STATE_TRAN);

    switch (mmc_cfg->mode) {
        case MMC_BUS_MODE_DDR52:
        {
            LOG_DBG("Switching to HS DDR52");
            /* Switch to HS, 52MHz DDR MHz */
            rc = mmc_set_ext_csd(EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS);
            if (rc != PB_OK) {
                LOG_ERR("Could not switch to HS timing");
                return rc;
            }

            rc = mmc_set_bus_width(MMC_BUS_WIDTH_8BIT_DDR);
            if (rc != 0) {
                return rc;
            }

            rc = mmc_set_bus_clock(MHz(52));
            if (rc != 0) {
                return rc;
            }
        }
        break;
        case MMC_BUS_MODE_HS200:
        {
            LOG_DBG("Switching to HS200");
            rc = mmc_set_bus_width(MMC_BUS_WIDTH_8BIT);
            if (rc != 0) {
                return rc;
            }

            /* Switch to HS200, 200 MHz */
            rc = mmc_set_ext_csd(EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS200);
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
        }
        break;
        case MMC_BUS_MODE_HS400:
        {
            /* TODO: This is not complete and not usable yet,
             * Strobe DLL configuration is missing. It seems to work
             * on the imx8qxpmek evk */
            LOG_DBG("Switching to HS400");
            /* Before we can switch the bus to 8-bit DDR the device must
             * be in HS mode */
            rc = mmc_set_ext_csd(EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS);
            if (rc != PB_OK) {
                LOG_ERR("Could not switch to HS timing");
                return rc;
            }

            rc = mmc_set_bus_width(MMC_BUS_WIDTH_8BIT_DDR);
            if (rc != 0) {
                return rc;
            }

            /* Switch to HS400, 200 MHz (DDR) */
            rc = mmc_set_ext_csd(EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS400);
            if (rc != PB_OK) {
                LOG_ERR("Could not switch to HS400 timing");
                return rc;
            }

            rc = mmc_set_bus_clock(MHz(200));
            if (rc != 0) {
                return rc;
            }
        }
        break;
        default:
            LOG_ERR("Unsupported mmc bus mode (%i)", mmc_cfg->mode);
            return -PB_ERR_NOT_IMPLEMENTED;
    }

    rc = mmc_fill_device_info();
    if (rc != 0) {
        return rc;
    }

    if (mmc_ext_csd[EXT_CSD_BOOT_BUS_CONDITIONS] != mmc_cfg->boot_mode) {
        LOG_INFO("Updating boot bus conditions to 0x%02x", mmc_cfg->boot_mode);

        rc = mmc_set_ext_csd(EXT_CSD_BOOT_BUS_CONDITIONS,
                             mmc_cfg->boot_mode);
        if (rc != PB_OK) {
            LOG_ERR("Could not update boot bus conditions");
            return rc;
        }
    }

    mmc_current_part = mmc_ext_csd[EXT_CSD_PART_CONFIG];

    size_t sectors = mmc_ext_csd[EXT_CSD_SEC_CNT + 0] << 0 |
            mmc_ext_csd[EXT_CSD_SEC_CNT + 1] << 8 |
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

    bio_dev_t d = bio_allocate(0,
                               mmc_ext_csd[EXT_CSD_BOOT_MULT] * 256 - 1,
                               512,
                               mmc_cfg->boot0_uu,
                               "eMMC BOOT0");

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

    d = bio_allocate(0,
                       mmc_ext_csd[EXT_CSD_BOOT_MULT] * 256 - 1,
                       512,
                       mmc_cfg->boot1_uu,
                       "eMMC BOOT1");

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

    d = bio_allocate(0,
                       sectors - 1,
                       512,
                       mmc_cfg->user_uu,
                       "eMMC USER");

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

struct mmc_device_info * mmc_device_info(void)
{
    return &mmc_dev_info;
}

int mmc_read(unsigned int lba, size_t length, uintptr_t buf)
{
    int ret;
    unsigned int cmd_idx;

#ifdef MMC_CORE_DEBUG_IOS
    LOG_DBG("%u, %zu, %p", lba, length, (void *) buf);
#endif

    if (mmc_hal->max_chunk_bytes > 0 && length > mmc_hal->max_chunk_bytes)
        return -PB_ERR_IO;

    ret = mmc_hal->prepare(lba, length, buf);
    if (ret != 0) {
        return ret;
    }

    if (length > MMC_BLOCK_SIZE) {
        cmd_idx = MMC_CMD_READ_MULTIPLE_BLOCK;
    } else {
        cmd_idx = MMC_CMD_READ_SINGLE_BLOCK;
    }

    ret = mmc_send_cmd(cmd_idx, lba, MMC_RSP_R1, NULL);
    if (ret != 0) {
        return ret;
    }

    ret = mmc_hal->read(lba, length, buf);
    if (ret != 0) {
        return ret;
    }

    /* Wait buffer empty */
    do {
        ret = mmc_device_state();
        if (ret < 0) {
            return ret;
        }
    } while ((ret != MMC_STATE_TRAN) && (ret != MMC_STATE_DATA));

    return PB_OK;
}

int mmc_write(unsigned int lba, size_t length, const uintptr_t buf)
{
    int ret;
    unsigned int cmd_idx;

#ifdef MMC_CORE_DEBUG_IOS
    LOG_DBG("%u, %zu, %p", lba, length, (void *) buf);
#endif

    if (mmc_hal->max_chunk_bytes > 0 && length > mmc_hal->max_chunk_bytes)
        return -PB_ERR_IO;

    ret = mmc_hal->prepare(lba, length, buf);
    if (ret != 0) {
        return ret;
    }

    if (length > MMC_BLOCK_SIZE) {
        cmd_idx = MMC_CMD_WRITE_MULTIPLE_BLOCK;
    } else {
        cmd_idx = MMC_CMD_WRITE_SINGLE_BLOCK;
    }

    ret = mmc_send_cmd(cmd_idx, lba, MMC_RSP_R1, NULL);
    if (ret != 0) {
        return ret;
    }

    ret = mmc_hal->write(lba, length, buf);
    if (ret != 0) {
        return ret;
    }

    /* Wait buffer empty */
    do {
        ret = mmc_device_state();
        if (ret < 0) {
            return ret;
        }
    } while ((ret != MMC_STATE_TRAN) && (ret != MMC_STATE_RCV));

    return PB_OK;
}

int mmc_part_switch(enum mmc_part part)
{
    uint8_t value = 0;
    const char *part_names[] = {
       "User",
       "Boot0",
       "Boot1",
       "RPMB",
    };

    switch (part) {
        case MMC_PART_BOOT0:
            /* Set boot0 R/W, boot1 as bootable */
            value = (2 << 3) | 0x01;
        break;
        case MMC_PART_BOOT1:
            /* Set boot1 R/W, boot0 as bootable */
            value = (1 << 3) | 0x02;
        break;
        case MMC_PART_RPMB:
            /* Enable RPMB access, set boot0 as bootable */
            value = (1 << 3) | 0x03;
        break;
        case MMC_PART_USER:
            /* Boot 0/1 RO, Boot0 bootable */
            value = (1 << 3);
        break;
        default:
            return -PB_ERR_IO;
    }

    /* Switch active partition */
    if (value != mmc_current_part) {
        mmc_current_part = value;
        LOG_DBG("Switching to %s", part_names[part]);
        return mmc_set_ext_csd(EXT_CSD_PART_CONFIG, value);
    } else {
        return PB_OK;
    }
}

ssize_t mmc_part_size(enum mmc_part part)
{
    switch (part) {
        case MMC_PART_BOOT0:
        case MMC_PART_BOOT1:
            return (ssize_t) mmc_ext_csd[EXT_CSD_BOOT_MULT] * 128 * 1024;
        case MMC_PART_RPMB:
            return (ssize_t) mmc_ext_csd[EXT_CSD_RPMB_MULT] * 128 * 1024;
        case MMC_PART_USER:
        {
            size_t sectors = mmc_ext_csd[EXT_CSD_SEC_CNT + 0] << 0 |
                    mmc_ext_csd[EXT_CSD_SEC_CNT + 1] << 8 |
                    mmc_ext_csd[EXT_CSD_SEC_CNT + 2] << 16 |
                    mmc_ext_csd[EXT_CSD_SEC_CNT + 3] << 24;
            return sectors * 512;
        }
        default:
            return -PB_ERR_PARAM;
    }
}

int mmc_init(const struct mmc_hal *hal, const struct mmc_device_config *cfg)
{
    if (!(cfg->mode > MMC_BUS_MODE_INVALID && cfg->mode < MMC_BUS_MODE_END) ||
        (hal == NULL) ||
        (cfg == NULL)) {
        return -PB_ERR_PARAM;
    }

    mmc_hal = hal;
    mmc_cfg = cfg;

    return mmc_setup();
}
