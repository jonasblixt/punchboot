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
#include <drivers/block/bio.h>
#include <drivers/mmc/mmc_core.h>

#define MMC_BIO_FLAG_BOOT0 BIT(0)
#define MMC_BIO_FLAG_BOOT1 BIT(1)
#define MMC_BIO_FLAG_RPMB BIT(2)
#define MMC_BIO_FLAG_USER BIT(3)

#define MMC_DEFAULT_MAX_RETRIES    5
#define SEND_OP_COND_MAX_RETRIES   100
#define MULT_BY_512K_SHIFT         19

static const struct mmc_hal *mmc_hal;
static const struct mmc_device_config *mmc_cfg;
static uint32_t mmc_ocr_value;
static struct mmc_device_info mmc_dev_info;
static uint32_t rca;
static struct mmc_csd_emmc mmc_csd;
static uint32_t scr[2] PB_ALIGN(16);
static uint8_t mmc_ext_csd[512] PB_ALIGN(16);
static struct sd_switch_status sd_switch_func_status;
static uint8_t mmc_tuning_rsp[128] PB_ALIGN(16);
static uint8_t mmc_current_part;

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

static const unsigned char tran_speed_base[16] = {
    0, 10, 12, 13, 15, 20, 26, 30, 35, 40, 45, 52, 55, 60, 70, 80
};

static const unsigned char sd_tran_speed_base[16] = {
    0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80
};
static bool is_cmd23_enabled(void)
{
    return ((mmc_cfg->flags & MMC_FLAG_CMD23) != 0U);
}

static bool is_sd_cmd6_enabled(void)
{
    return ((mmc_cfg->flags & MMC_FLAG_SD_CMD6) != 0U);
}

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

static int sd_send_op_cond(void)
{
    int n;
    mmc_cmd_resp_t resp_data;

    for (n = 0; n < SEND_OP_COND_MAX_RETRIES; n++) {
        int rc;

        /* CMD55: Application Specific Command */
        rc = mmc_send_cmd(MMC_CMD_APP_CMD, 0, MMC_RSP_R1, NULL);
        if (rc != 0) {
            return rc;
        }

        /* ACMD41: SD_SEND_OP_COND */
        rc = mmc_send_cmd(SD_CMD_SEND_OP_COND, OCR_HCS |
            mmc_dev_info.ocr_voltage, MMC_RSP_R3,
            &resp_data[0]);
        if (rc != 0) {
            return rc;
        }

        if ((resp_data[0] & OCR_POWERUP) != 0U) {
            mmc_ocr_value = resp_data[0];
            if ((mmc_ocr_value & OCR_HCS) != 0U) {
                mmc_dev_info.mmc_card_type = MMC_CARD_TYPE_SD_HC;
            } else {
                mmc_dev_info.mmc_card_type = MMC_CARD_TYPE_SD;
            }
            return 0;
        }

        pb_delay_ms(10);
    }

    LOG_ERR("ACMD41 failed after %d retries\n", SEND_OP_COND_MAX_RETRIES);

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
    unsigned long long c_size;
    unsigned int speed_idx;
    unsigned int nb_blocks;
    unsigned int freq_unit;
    int ret = 0;
    struct mmc_csd_sd_v2 *csd_sd_v2;

    switch (mmc_dev_info.mmc_card_type) {
    case MMC_CARD_TYPE_EMMC:
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

        nb_blocks = (mmc_ext_csd[EXT_CSD_SEC_CNT] << 0) |
                (mmc_ext_csd[EXT_CSD_SEC_CNT + 1] << 8) |
                (mmc_ext_csd[EXT_CSD_SEC_CNT + 2] << 16) |
                (mmc_ext_csd[EXT_CSD_SEC_CNT + 3] << 24);

        mmc_dev_info.device_capacity = (unsigned long long)nb_blocks *
            mmc_dev_info.block_size;

        break;

    case MMC_CARD_TYPE_SD:
        /*
         * Use the same mmc_csd struct, as required fields here
         * (READ_BL_LEN, C_SIZE, CSIZE_MULT) are common with eMMC.
         */
        mmc_dev_info.block_size = BIT_32(mmc_csd.read_bl_len);

        c_size = ((unsigned long long)mmc_csd.c_size_high << 2U) |
             (unsigned long long)mmc_csd.c_size_low;
        //assert(c_size != 0xFFFU);

        mmc_dev_info.device_capacity = (c_size + 1U) *
                        BIT_64(mmc_csd.c_size_mult + 2U) *
                        mmc_dev_info.block_size;

        break;

    case MMC_CARD_TYPE_SD_HC:
        mmc_dev_info.block_size = MMC_BLOCK_SIZE;

        /* Need to use mmc_csd_sd_v2 struct */
        csd_sd_v2 = (struct mmc_csd_sd_v2 *)&mmc_csd;
        c_size = ((unsigned long long)csd_sd_v2->c_size_high << 16) |
             (unsigned long long)csd_sd_v2->c_size_low;

        mmc_dev_info.device_capacity = (c_size + 1U) << MULT_BY_512K_SHIFT;

        break;

    default:
        ret = -PB_ERR;
        break;
    }

    if (ret < 0) {
        return ret;
    }

    speed_idx = (mmc_csd.tran_speed & CSD_TRAN_SPEED_MULT_MASK) >>
             CSD_TRAN_SPEED_MULT_SHIFT;

    if (mmc_dev_info.mmc_card_type == MMC_CARD_TYPE_EMMC) {
        mmc_dev_info.max_bus_freq_hz = tran_speed_base[speed_idx];
    } else {
        mmc_dev_info.max_bus_freq_hz = sd_tran_speed_base[speed_idx];
    }

    freq_unit = mmc_csd.tran_speed & CSD_TRAN_SPEED_UNIT_MASK;
    while (freq_unit != 0U) {
        mmc_dev_info.max_bus_freq_hz *= 10U;
        --freq_unit;
    }

    mmc_dev_info.max_bus_freq_hz *= 10000U;
    /* TODO: emmc: This is only valid for backwards compatible mode,
     *  This is not used for HS, HS200, HS400 etc. Probably remove
     *  this for eMMC at least. Investigate how it's used for
     *  SD -cards */
    LOG_DBG("max_bus_freq_hz = %u", mmc_dev_info.max_bus_freq_hz);
    return 0;
}

static int mmc_sd_switch(enum mmc_bus_width bus_width)
{
    int ret;
    int retries = MMC_DEFAULT_MAX_RETRIES;
    unsigned int bus_width_arg = 0;

    ret = mmc_hal->prepare(0, sizeof(scr), (uintptr_t)&scr);
    if (ret != 0) {
        return ret;
    }

    /* CMD55: Application Specific Command */
    ret = mmc_send_cmd(MMC_CMD_APP_CMD, rca << RCA_SHIFT_OFFSET,
               MMC_RSP_R5, NULL);
    if (ret != 0) {
        return ret;
    }

    /* ACMD51: SEND_SCR */
    do {
        ret = mmc_send_cmd(SD_CMD_SEND_SCR, 0, MMC_RSP_R1, NULL);
        if ((ret != 0) && (retries == 0)) {
            LOG_ERR("ACMD51 failed after %d retries (ret=%d)",
                  MMC_DEFAULT_MAX_RETRIES, ret);
            return ret;
        }

        retries--;
    } while (ret != 0);

    ret = mmc_hal->read(0, sizeof(scr), (uintptr_t)&scr);
    if (ret != 0) {
        return ret;
    }

    if (((scr[0] & SD_SCR_BUS_WIDTH_4) != 0U) &&
        (bus_width == MMC_BUS_WIDTH_4BIT)) {
        bus_width_arg = 2;
    }

    /* CMD55: Application Specific Command */
    ret = mmc_send_cmd(MMC_CMD_APP_CMD, rca << RCA_SHIFT_OFFSET,
               MMC_RSP_R5, NULL);
    if (ret != 0) {
        return ret;
    }

    /* ACMD6: SET_BUS_WIDTH */
    ret = mmc_send_cmd(SD_CMD_SET_BUS_WIDTH, bus_width_arg, MMC_RSP_R1, NULL);
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
    enum mmc_bus_width width = bus_width;

    if (mmc_dev_info.mmc_card_type != MMC_CARD_TYPE_EMMC) {
        if (width == MMC_BUS_WIDTH_8BIT) {
            LOG_WARN("Wrong bus config for SD-card, force to 4");
            width = MMC_BUS_WIDTH_4BIT;
        }
        ret = mmc_sd_switch(width);
        if (ret != 0) {
            return ret;
        }
    } else if (mmc_csd.spec_vers == 4U) {
        ret = mmc_set_ext_csd(EXT_CSD_BUS_WIDTH, width);
        if (ret != 0) {
            return ret;
        }
    } else {
        LOG_ERR("Wrong MMC type or spec version\n");
        return -1;
    }

    return mmc_hal->set_bus_width(bus_width);
}

static int sd_switch(unsigned int mode, unsigned char group,
             unsigned char func)
{
    unsigned int group_shift = (group - 1U) * 4U;
    unsigned int group_mask = GENMASK(group_shift + 3U,  group_shift);
    unsigned int arg;
    int ret;

    ret = mmc_hal->prepare(0, sizeof(sd_switch_func_status),
                        (uintptr_t)&sd_switch_func_status);
    if (ret != 0) {
        return ret;
    }

    /* MMC CMD6: SWITCH_FUNC */
    arg = mode | SD_SWITCH_ALL_GROUPS_MASK;
    arg &= ~group_mask;
    arg |= func << group_shift;
    ret = mmc_send_cmd(MMC_CMD_SWITCH, arg, MMC_RSP_R1, NULL);
    if (ret != 0) {
        return ret;
    }

    return mmc_hal->read(0, sizeof(sd_switch_func_status),
                     (uintptr_t)&sd_switch_func_status);
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
        mmc_part_switch(MMC_PART_BOOT0);
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
        mmc_part_switch(MMC_PART_BOOT0);
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

    LOG_INFO("Starting, this will produce I/O errors for bad delay taps...");
    for (int i = 0; i < 127; i++) {
        mmc_hal->set_delay_tap(i);
        pb_delay_ms(10);
        rc = mmc_hal->prepare(0, sizeof(mmc_tuning_rsp), (uintptr_t)mmc_tuning_rsp);
        if (rc != 0) {
            goto tune_fail;
        }

        rc = mmc_send_cmd(MMC_CMD_SEND_TUNING_BLOCK_HS200, 0, MMC_RSP_R1, NULL);

        if (rc != 0) {
            goto tune_fail;
        }

        rc = mmc_hal->read(0, sizeof(mmc_tuning_rsp), (uintptr_t)mmc_tuning_rsp);
        if (rc != 0) {
            goto tune_fail;
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
    bool found_start = false;
    unsigned int high_score = 0;
    unsigned int selected_tap;
    size_t good_count = 0;

    for (int i = 0; i < 127; i++)  {
        good_count += result[i];

        if (!found_start) {
            if (result[i]) {
                found_start = true;
                start_tap = i;
            }
        } else {
            if (!result[i] || (i == 127)) {
                unsigned int pass_count = i - start_tap;
                unsigned int center_tap = start_tap + pass_count/2;
                LOG_DBG("Pass count = %i, center tap = %i", pass_count, center_tap);

                if (pass_count > high_score) {
                    high_score = pass_count;
                    selected_tap = center_tap;
                }
                found_start = false;
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

static int mmc_enumerate(void)
{
    int rc;
    mmc_cmd_resp_t resp_data;

    rc = mmc_hal->init();

    if (rc != PB_OK)
        return rc;

    rc = mmc_set_bus_clock(400*1000);
    if (rc != 0) {
        return rc;
    }

    rc = mmc_reset_to_idle();
    if (rc != PB_OK) {
        return rc;
    }

    if (mmc_cfg->card_type == MMC_CARD_TYPE_EMMC) {
        rc = mmc_send_op_cond();
    } else {
        /* CMD8: Send Interface Condition Command */
        rc = mmc_send_cmd(MMC_CMD_SEND_EXT_CSD,
                          VHS_2_7_3_6_V | CMD8_CHECK_PATTERN,
                          MMC_RSP_R5, resp_data);

        if ((rc == 0) && ((resp_data[0] & 0xffU) == CMD8_CHECK_PATTERN)) {
            rc = sd_send_op_cond();
        }
    }

    if (rc != 0) {
        return rc;
    }

    /* CMD2: Card Identification */
    rc = mmc_send_cmd(MMC_CMD_ALL_SEND_CID, 0, MMC_RSP_R2, NULL);
    if (rc != 0) {
        return rc;
    }

    /* CMD3: Set Relative Address */
    if (mmc_cfg->card_type == MMC_CARD_TYPE_EMMC) {
        rca = MMC_FIX_RCA;
        rc = mmc_send_cmd(MMC_CMD_SET_RELATIVE_ADDR, rca << RCA_SHIFT_OFFSET,
                   MMC_RSP_R1, NULL);
        if (rc != 0) {
            return rc;
        }
    } else {
        rc = mmc_send_cmd(MMC_CMD_SET_RELATIVE_ADDR, 0, MMC_RSP_R6, resp_data);
        if (rc != 0) {
            return rc;
        }

        rca = (resp_data[0] & 0xFFFF0000U) >> 16;
    }

    /* CMD9: CSD Register */
    rc = mmc_send_cmd(MMC_CMD_SEND_CSD, rca << RCA_SHIFT_OFFSET,
               MMC_RSP_R2, resp_data);
    if (rc != 0) {
        return rc;
    }

    memcpy(&mmc_csd, &resp_data, sizeof(resp_data));

    LOG_DBG("csd0: %x", resp_data[0]);
    LOG_DBG("csd1: %x", resp_data[1]);
    LOG_DBG("csd2: %x", resp_data[2]);
    LOG_DBG("csd3: %x", resp_data[3]);

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
        case MMC_BUS_MODE_HS200:
        {
            LOG_DBG("Switching to HS200");
            rc = mmc_set_bus_width(MMC_BUS_WIDTH_8BIT);
            if (rc != 0) {
                return rc;
            }

            /* Switch to HS200, 200 MHz */
            rc = mmc_set_ext_csd(EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS200 | (1 << 4));
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

    LOG_DBG("Got ext csd!");

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

    if (is_sd_cmd6_enabled() &&
        (mmc_cfg->card_type == MMC_CARD_TYPE_SD_HC)) {
        /* Try to switch to High Speed Mode */
        rc = sd_switch(SD_SWITCH_FUNC_CHECK, 1U, 1U);
        if (rc != 0) {
            return rc;
        }

        if ((sd_switch_func_status.support_g1 & BIT(9)) == 0U) {
            /* High speed not supported, keep default speed */
            return 0;
        }

        rc = sd_switch(SD_SWITCH_FUNC_SWITCH, 1U, 1U);
        if (rc != 0) {
            return rc;
        }

        if ((sd_switch_func_status.sel_g2_g1 & 0x1U) == 0U) {
            /* Cannot switch to high speed, keep default speed */
            return 0;
        }

        mmc_dev_info.max_bus_freq_hz = 50000000U;
        //rc = mmc_hal->set_ios(clk, bus_width);
    }

    return rc;
}

struct mmc_device_info * mmc_device_info(void)
{
    return &mmc_dev_info;
}

int mmc_read(unsigned int lba, size_t length, uintptr_t buf)
{
    int ret;
    unsigned int cmd_idx, cmd_arg;

#ifdef MMC_CORE_DEBUG_IOS
    LOG_DBG("%u, %zu, %p", lba, length, (void *) buf);
#endif

    if (mmc_hal->max_chunk_bytes > 0 && length > mmc_hal->max_chunk_bytes)
        return -PB_ERR_IO;

    ret = mmc_hal->prepare(lba, length, buf);
    if (ret != 0) {
        return ret;
    }

    if (is_cmd23_enabled()) {
        /* Set block count */
        ret = mmc_send_cmd(MMC_CMD_SET_BLOCK_COUNT, length / MMC_BLOCK_SIZE,
                   MMC_RSP_R1, NULL);
        if (ret != 0) {
            return ret;
        }

        cmd_idx = MMC_CMD_READ_MULTIPLE_BLOCK;
    } else {
        if (length > MMC_BLOCK_SIZE) {
            cmd_idx = MMC_CMD_READ_MULTIPLE_BLOCK;
        } else {
            cmd_idx = MMC_CMD_READ_SINGLE_BLOCK;
        }
    }

    if (((mmc_ocr_value & OCR_ACCESS_MODE_MASK) == OCR_BYTE_MODE) &&
        (mmc_dev_info.mmc_card_type != MMC_CARD_TYPE_SD_HC)) {
        cmd_arg = lba * MMC_BLOCK_SIZE;
    } else {
        cmd_arg = lba;
    }

    ret = mmc_send_cmd(cmd_idx, cmd_arg, MMC_RSP_R1, NULL);
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
    unsigned int cmd_idx, cmd_arg;

#ifdef MMC_CORE_DEBUG_IOS
    LOG_DBG("%u, %zu, %p", lba, length, (void *) buf);
#endif

    if (mmc_hal->max_chunk_bytes > 0 && length > mmc_hal->max_chunk_bytes)
        return -PB_ERR_IO;

    ret = mmc_hal->prepare(lba, length, buf);
    if (ret != 0) {
        return ret;
    }

    if (is_cmd23_enabled()) {
        /* Set block count */
        ret = mmc_send_cmd(MMC_CMD_SET_BLOCK_COUNT, length / MMC_BLOCK_SIZE,
                   MMC_RSP_R1, NULL);
        if (ret != 0) {
            return ret;
        }

        cmd_idx = MMC_CMD_WRITE_MULTIPLE_BLOCK;
    } else {
        if (length > MMC_BLOCK_SIZE) {
            cmd_idx = MMC_CMD_WRITE_MULTIPLE_BLOCK;
        } else {
            cmd_idx = MMC_CMD_WRITE_SINGLE_BLOCK;
        }
    }

    if ((mmc_ocr_value & OCR_ACCESS_MODE_MASK) == OCR_BYTE_MODE) {
        cmd_arg = lba * MMC_BLOCK_SIZE;
    } else {
        cmd_arg = lba;
    }

    ret = mmc_send_cmd(cmd_idx, cmd_arg, MMC_RSP_R1, NULL);
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

    if (mmc_cfg->card_type != MMC_CARD_TYPE_EMMC)
        return -PB_ERR_NOT_SUPPORTED;

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
    if (mmc_cfg->card_type != MMC_CARD_TYPE_EMMC &&
            part != MMC_PART_USER) {
        return -PB_ERR_NOT_SUPPORTED;
    }

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
            return -PB_ERR;
    }

    return 0;
}

int mmc_init(const struct mmc_hal *hal, const struct mmc_device_config *cfg)
{
    int rc;
    if (!(cfg->mode > MMC_BUS_MODE_INVALID && cfg->mode < MMC_BUS_MODE_END) ||
        !(cfg->card_type > MMC_CARD_TYPE_INVALID && cfg->card_type < MMC_CARD_TYPE_END) ||
        (hal == NULL) ||
        (cfg == NULL)) {
        return -PB_ERR_PARAM;
    }

    mmc_hal = hal;
    mmc_cfg = cfg;
    mmc_dev_info.mmc_card_type = cfg->card_type;

    rc = mmc_enumerate();

    if (rc != PB_OK)
        return rc;

    return PB_OK;
}
