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
#include <pb/mmc.h>

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

    LOG_DBG("idx %u, arg 0x%08x, 0x%x", cmd_idx, arg, resp_type);
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
                                (uintptr_t)&mmc_ext_csd);
        if (ret != 0) {
            return ret;
        }

        ret = mmc_send_cmd(MMC_CMD_SEND_EXT_CSD, 0, MMC_RSP_R1, NULL);
        if (ret != 0) {
            return ret;
        }

        ret = mmc_hal->read(0, sizeof(mmc_ext_csd), (uintptr_t)&mmc_ext_csd);
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

static int mmc_set_ios(unsigned int clk, enum mmc_bus_width bus_width)
{
    int ret;
    enum mmc_bus_width width = bus_width;

    LOG_DBG("mmc_card_type = %i", mmc_dev_info.mmc_card_type);

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
    }

    return mmc_hal->set_ios(clk, width);
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

static int mmc_enumerate(void)
{
    int rc;
    mmc_cmd_resp_t resp_data;

    rc = mmc_hal->init();

    if (rc != PB_OK)
        return rc;

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

/* TODO: Here we need to look at the requested bus mode */

    /* Switch to hs timing */

    rc = mmc_set_ext_csd(EXT_CSD_HS_TIMING, 0x01);
    if (rc != PB_OK) {
        LOG_ERR("Could not switch to high speed timing");
        return rc;
    }

    rc = mmc_set_ios(25*1000*1000, MMC_BUS_WIDTH_8BIT);
    if (rc != 0) {
        return rc;
    }

    rc = mmc_fill_device_info();
    if (rc != 0) {
        return rc;
    }

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
    return NULL;
}

int mmc_read(unsigned int lba, size_t length, uintptr_t buf)
{
    return -1;
}

int mmc_write(unsigned int lba, size_t length, const uintptr_t buf)
{
    return -1;
}

int mmc_part_switch(enum mmc_part part)
{
    return -1;
}

ssize_t mmc_part_size(enum mmc_part part)
{
    return 0;
}

int mmc_init(const struct mmc_hal *hal, const struct mmc_device_config *cfg)
{
    if (!(cfg->mode > MMC_BUS_MODE_INVALID && cfg->mode < MMC_BUS_MODE_END) ||
        !(cfg->card_type > MMC_CARD_TYPE_INVALID && cfg->card_type < MMC_CARD_TYPE_END) ||
        (hal == NULL) ||
        (cfg == NULL)) {
        return -PB_ERR_PARAM;
    }

    mmc_hal = hal;
    mmc_cfg = cfg;
    mmc_dev_info.mmc_card_type = cfg->card_type;

    return mmc_enumerate();
}
