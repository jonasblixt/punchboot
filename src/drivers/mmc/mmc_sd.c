/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * SD memory cards: SDSC, SDHC and SDXC, physical layer 1.x and later,
 * default speed and high speed on a 1 or 4 bit bus. The card is one bio
 * device, the user area.
 */

#include <drivers/mmc/mmc_core.h>
#include <pb/bio.h>
#include <pb/delay.h>
#include <pb/pb.h>
#include <string.h>

#include "mmc_core_private.h"

/* CMD8: 2.7-3.6 V and a check pattern, echoed in R7 */
#define SD_IF_COND_VHS_2V7_3V6   BIT(8)
#define SD_IF_COND_CHECK         0xaa
#define SD_IF_COND_ARG           (SD_IF_COND_VHS_2V7_3V6 | SD_IF_COND_CHECK)
#define SD_IF_COND_MASK          0xfff

/* ACMD41 argument and the OCR in R3 */
#define SD_OCR_BUSY_N            BIT(31) /* Power up done */
#define SD_OCR_HCS               BIT(30) /* Argument: the host takes SDHC/SDXC */
#define SD_OCR_CCS               BIT(30) /* Response: SDHC/SDXC, block addresses */
#define SD_OCR_VOLTAGES          GENMASK(23, 15) /* 2.7-3.6 V */
#define SD_OP_COND_TIMEOUT_ms    1000
#define SD_OP_COND_POLL_ms       10

/* R6: the new RCA in [31:16] */
#define SD_R6_RCA_SHIFT          16

/* ACMD6 */
#define SD_BUS_WIDTH_1           0
#define SD_BUS_WIDTH_4           2

/* SCR, big endian bytes */
#define SD_SCR_SD_SPEC(scr)      ((scr)[0] & 0x0f)
#define SD_SCR_SPEC_1_10         1 /* CMD6 */
#define SD_SCR_BUS_WIDTH_4       BIT(2) /* In byte 1 */

/* CMD6 SWITCH_FUNC: mode in bit 31, function group 1 (access mode) in [3:0], 0xf: no change */
#define SD_SWITCH_SET            BIT(31)
#define SD_SWITCH_GROUPS_NO_CHG  0x00fffff0
#define SD_SWITCH_ACCESS_HS      1
#define SD_SWITCH_STATUS_SIZE    64
#define SD_SWITCH_G1_SUPPORT_LSB 13 /* Bits [415:400]: byte 12-13 */
#define SD_SWITCH_G1_RESULT      16 /* Bits [379:376]: byte 16, low nibble */

#define SD_DEFAULT_CLK_HZ        MHz(25)
#define SD_HS_CLK_HZ             MHz(50)

/* CSD bits, as in the R2 response: resp[n / 32] bit n % 32 */
static uint32_t csd_bits(const uint32_t csd[4], unsigned int hi, unsigned int lo)
{
    uint32_t value = 0;

    for (unsigned int bit = hi + 1; bit-- > lo;)
        value = (value << 1) | ((csd[bit / 32] >> (bit % 32)) & 1);
    return value;
}

/* Capacity in 512 byte blocks, from CSD version 1.0 or 2.0 */
static uint64_t csd_blocks(const uint32_t csd[4])
{
    if (csd_bits(csd, 127, 126) == 1) {
        /* (C_SIZE + 1) * 512 KiB */
        return ((uint64_t)csd_bits(csd, 69, 48) + 1) * 1024;
    }

    /* (C_SIZE + 1) * 2^(C_SIZE_MULT + 2) blocks of 2^READ_BL_LEN bytes */
    uint64_t bytes = ((uint64_t)csd_bits(csd, 73, 62) + 1) << (csd_bits(csd, 49, 47) + 2);

    return (bytes << csd_bits(csd, 83, 80)) / MMC_BLOCK_SIZE;
}

/* CMD55 and an application command */
static int sd_app_cmd(struct mmc_device *dev, struct mmc_cmd *cmd)
{
    int rc = mmc_send_cmd_r1(dev, MMC_CMD_APP_CMD, dev->rca << RCA_SHIFT_OFFSET);

    if (rc != PB_OK)
        return rc;
    return mmc_request(dev, cmd);
}

static int sd_send_op_cond(struct mmc_device *dev, bool v2)
{
    struct mmc_cmd cmd = {
        .idx = SD_CMD_SEND_OP_COND,
        .arg = (v2 ? SD_OCR_HCS : 0) | SD_OCR_VOLTAGES,
        .resp_type = MMC_RSP_R3,
    };

    for (unsigned int t = 0; t < SD_OP_COND_TIMEOUT_ms; t += SD_OP_COND_POLL_ms) {
        int rc = sd_app_cmd(dev, &cmd);

        if (rc != PB_OK)
            return rc;

        if (cmd.resp[0] & SD_OCR_BUSY_N) {
            dev->ocr = cmd.resp[0];
            return PB_OK;
        }
        pb_delay_ms(SD_OP_COND_POLL_ms);
    }

    LOG_ERR("ACMD41: the card is still busy");
    return -PB_ERR_TIMEOUT;
}

static int sd_read_scr(struct mmc_device *dev)
{
    struct mmc_cmd cmd = {
        .idx = SD_CMD_SEND_SCR,
        .resp_type = MMC_RSP_R1,
        .buf = dev->sd_data,
        .block_size = sizeof(dev->scr),
        .blocks = 1,
    };
    int rc = sd_app_cmd(dev, &cmd);

    if (rc != PB_OK)
        return rc;
    if (cmd.resp[0] & STATUS_ERROR_MASK)
        return -PB_ERR_IO;

    memcpy(dev->scr, dev->sd_data, sizeof(dev->scr));
    return mmc_wait_state(dev, MMC_STATE_TRAN, MMC_DEFAULT_TIMEOUT_ms);
}

static int sd_set_bus_width(struct mmc_device *dev)
{
    struct mmc_cmd cmd = {
        .idx = SD_CMD_SET_BUS_WIDTH,
        .arg = SD_BUS_WIDTH_4,
        .resp_type = MMC_RSP_R1,
    };
    int rc;

    if (dev->cfg->width != MMC_BUS_WIDTH_4BIT || !(dev->scr[1] & SD_SCR_BUS_WIDTH_4))
        return PB_OK;

    rc = sd_app_cmd(dev, &cmd);
    if (rc != PB_OK)
        return rc;
    if (cmd.resp[0] & STATUS_ERROR_MASK)
        return -PB_ERR_IO;

    dev->info.width = MMC_BUS_WIDTH_4BIT;
    return dev->hal->set_bus_width(dev->hal->priv, MMC_BUS_WIDTH_4BIT);
}

/* CMD6 for the access mode group, returns the function the card selects */
static int sd_switch_func(struct mmc_device *dev, bool set, unsigned int fn)
{
    struct mmc_cmd cmd = {
        .idx = SD_CMD_SWITCH_FUNC,
        .arg = (set ? SD_SWITCH_SET : 0) | SD_SWITCH_GROUPS_NO_CHG | fn,
        .buf = dev->sd_data,
        .block_size = SD_SWITCH_STATUS_SIZE,
        .blocks = 1,
    };
    int rc = mmc_request_r1(dev, &cmd);

    if (rc != PB_OK)
        return rc;

    rc = mmc_wait_state(dev, MMC_STATE_TRAN, MMC_DEFAULT_TIMEOUT_ms);
    if (rc != PB_OK)
        return rc;

    if (!(dev->sd_data[SD_SWITCH_G1_SUPPORT_LSB] & BIT(fn)))
        return -PB_ERR_NOT_SUPPORTED;

    return dev->sd_data[SD_SWITCH_G1_RESULT] & 0x0f;
}

/* High speed if the board and the card can (SD 1.10 and later) */
static int sd_select_speed(struct mmc_device *dev)
{
    int rc;

    dev->info.mode = MMC_BUS_MODE_SD_DEFAULT;

    if (dev->cfg->mode < MMC_BUS_MODE_SD_HS || SD_SCR_SD_SPEC(dev->scr) < SD_SCR_SPEC_1_10)
        return PB_OK;

    rc = sd_switch_func(dev, false, SD_SWITCH_ACCESS_HS);
    if (rc == -PB_ERR_NOT_SUPPORTED || rc == 0xf)
        return PB_OK;
    if (rc < 0)
        return rc;

    rc = sd_switch_func(dev, true, SD_SWITCH_ACCESS_HS);
    if (rc < 0)
        return rc;
    if (rc != SD_SWITCH_ACCESS_HS) {
        LOG_ERR("High speed switch failed (%i)", rc);
        return -PB_ERR_IO;
    }

    LOG_DBG("High speed");
    dev->info.mode = MMC_BUS_MODE_SD_HS;
    return mmc_set_bus_clock(dev, SD_HS_CLK_HZ);
}

static int sd_bio_read(bio_dev_t bdev, lba_t lba, size_t length, void *buf)
{
    struct mmc_device *dev = (struct mmc_device *)bio_get_private(bdev);

    return mmc_bio_xfer(dev, lba, length, (uintptr_t)buf, false);
}

static int sd_bio_write(bio_dev_t bdev, lba_t lba, size_t length, const void *buf)
{
    struct mmc_device *dev = (struct mmc_device *)bio_get_private(bdev);

    return mmc_bio_xfer(dev, lba, length, (uintptr_t)buf, true);
}

static int sd_add_bio(struct mmc_device *dev)
{
    bio_dev_t d = bio_allocate(0, dev->blocks - 1, MMC_BLOCK_SIZE, dev->cfg->user_uu, "SD");
    int rc;

    if (d < 0)
        return d;

    rc = bio_set_flags(d, BIO_FLAG_VISIBLE | BIO_FLAG_WRITABLE);
    if (rc < 0)
        return rc;

    rc = bio_set_private(d, (uintptr_t)dev);
    if (rc < 0)
        return rc;

    return bio_set_ios(d, sd_bio_read, sd_bio_write);
}

int mmc_sd_setup(struct mmc_device *dev)
{
    mmc_cmd_resp_t resp;
    bool v2;
    int rc;

    LOG_DBG("SD init");
    dev->info.block_size = MMC_BLOCK_SIZE;
    dev->info.width = MMC_BUS_WIDTH_1BIT;

    rc = dev->hal->init(dev->hal->priv);
    if (rc != PB_OK)
        return rc;

    if (dev->hal->card_present != NULL && !dev->hal->card_present(dev->hal->priv)) {
        LOG_INFO("No SD card");
        return -PB_ERR_NOT_FOUND;
    }

    rc = mmc_set_bus_clock(dev, MMC_ID_CLK_HZ);
    if (rc != PB_OK)
        return rc;

    rc = mmc_reset_to_idle(dev);
    if (rc != PB_OK)
        return rc;

    /* CMD8: a 2.0 card echoes the pattern, a 1.x card doesn't answer */
    rc = mmc_send_cmd(dev, SD_CMD_SEND_IF_COND, SD_IF_COND_ARG, MMC_RSP_R7, resp);
    if (rc == PB_OK) {
        if ((resp[0] & SD_IF_COND_MASK) != SD_IF_COND_ARG) {
            LOG_ERR("CMD8: 0x%08x, the card can't work at 3.3 V", resp[0]);
            return -PB_ERR_NOT_SUPPORTED;
        }
        v2 = true;
    } else if (rc == -PB_ERR_TIMEOUT) {
        v2 = false;
    } else {
        return rc;
    }

    rc = sd_send_op_cond(dev, v2);
    if (rc != PB_OK)
        return rc;

    dev->block_addressing = dev->ocr & SD_OCR_CCS;

    rc = mmc_send_cmd(dev, MMC_CMD_ALL_SEND_CID, 0, MMC_RSP_R2, NULL);
    if (rc != PB_OK)
        return rc;

    /* CMD3: the card publishes its RCA */
    rc = mmc_send_cmd(dev, SD_CMD_SEND_RELATIVE_ADDR, 0, MMC_RSP_R6, resp);
    if (rc != PB_OK)
        return rc;
    dev->rca = resp[0] >> SD_R6_RCA_SHIFT;

    rc = mmc_send_cmd(dev, MMC_CMD_SEND_CSD, dev->rca << RCA_SHIFT_OFFSET, MMC_RSP_R2, resp);
    if (rc != PB_OK)
        return rc;
    memcpy(dev->csd, resp, sizeof(dev->csd));
    dev->blocks = csd_blocks(dev->csd);

    rc = mmc_set_bus_clock(dev, SD_DEFAULT_CLK_HZ);
    if (rc != PB_OK)
        return rc;

    rc = mmc_send_cmd(dev, MMC_CMD_SELECT_CARD, dev->rca << RCA_SHIFT_OFFSET, MMC_RSP_R1b, NULL);
    if (rc != PB_OK)
        return rc;

    rc = mmc_wait_state(dev, MMC_STATE_TRAN, MMC_DEFAULT_TIMEOUT_ms);
    if (rc != PB_OK)
        return rc;

    /* SDSC: the block length may be larger after power on */
    if (!dev->block_addressing) {
        rc = mmc_send_cmd_r1(dev, MMC_CMD_SET_BLOCKLEN, MMC_BLOCK_SIZE);
        if (rc != PB_OK)
            return rc;
    }

    rc = sd_read_scr(dev);
    if (rc != PB_OK)
        return rc;

    rc = sd_set_bus_width(dev);
    if (rc != PB_OK)
        return rc;

    rc = sd_select_speed(dev);
    if (rc != PB_OK)
        return rc;

    LOG_INFO("SD%s card, %llu MiB, %s, %u bit",
             dev->block_addressing ? "HC/XC" : "SC",
             (unsigned long long)(dev->blocks / 2048),
             dev->info.mode == MMC_BUS_MODE_SD_HS ? "high speed" : "default speed",
             dev->info.width == MMC_BUS_WIDTH_4BIT ? 4 : 1);

    rc = sd_add_bio(dev);
    return rc;
}
