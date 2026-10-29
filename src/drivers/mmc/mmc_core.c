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
 * The parts shared by the card types: devices, commands, block transfers.
 * e.MMC is in mmc_emmc.c, SD in mmc_sd.c.
 */

#include <drivers/mmc/mmc_core.h>
#include <pb/bio.h>
#include <pb/delay.h>
#include <pb/pb.h>
#include <string.h>

#include "mmc_core_private.h"

static struct mmc_device devices[MMC_CORE_MAX_DEVICES];

int mmc_request(struct mmc_device *dev, struct mmc_cmd *cmd)
{
    int rc;

#ifdef CONFIG_MMC_CORE_DEBUG_CMDS
    LOG_DBG("idx %u, arg 0x%08x, 0x%x", cmd->idx, cmd->arg, cmd->resp_type);
#endif

    rc = dev->hal->request(dev->hal->priv, cmd);

    if (rc != PB_OK && !cmd->quiet) {
        LOG_ERR("Send command %u error: %i", cmd->idx, rc);
    }

    return rc;
}

int mmc_send_cmd(struct mmc_device *dev,
                 uint16_t cmd_idx,
                 uint32_t arg,
                 uint16_t resp_type,
                 mmc_cmd_resp_t result)
{
    struct mmc_cmd cmd = {
        .idx = cmd_idx,
        .arg = arg,
        .resp_type = resp_type,
    };
    int rc = mmc_request(dev, &cmd);

    if (rc == PB_OK && result != NULL)
        memcpy(result, cmd.resp, sizeof(mmc_cmd_resp_t));
    return rc;
}

int mmc_request_r1(struct mmc_device *dev, struct mmc_cmd *cmd)
{
    int rc;

    cmd->resp_type = MMC_RSP_R1;
    rc = mmc_request(dev, cmd);
    if (rc != PB_OK)
        return rc;

    if (cmd->resp[0] & STATUS_ERROR_MASK) {
        if (!cmd->quiet)
            LOG_ERR("CMD%u card status 0x%08x", cmd->idx, cmd->resp[0]);
        return -PB_ERR_IO;
    }

    return PB_OK;
}

int mmc_send_cmd_r1(struct mmc_device *dev, uint16_t cmd_idx, uint32_t arg)
{
    struct mmc_cmd cmd = {
        .idx = cmd_idx,
        .arg = arg,
    };

    return mmc_request_r1(dev, &cmd);
}

int mmc_reset_to_idle(struct mmc_device *dev)
{
    int rc;

    /* CMD0: reset to IDLE */
    rc = mmc_send_cmd(dev, MMC_CMD_GO_IDLE_STATE, 0, 0, NULL);
    if (rc != PB_OK) {
        return rc;
    }

    pb_delay_ms(1);

    return PB_OK;
}

int mmc_device_state(struct mmc_device *dev, unsigned int timeout_ms)
{
    mmc_cmd_resp_t resp_data;

    while (true) {
        int ret;

        if (timeout_ms == 0) {
            LOG_ERR("CMD13 timeout");
            return -PB_ERR_IO;
        }

        ret = mmc_send_cmd(
            dev, MMC_CMD_SEND_STATUS, dev->rca << RCA_SHIFT_OFFSET, MMC_RSP_R1, resp_data);

        if (ret == PB_OK) {
            if ((resp_data[0] & STATUS_ERROR_MASK) != 0U) {
                LOG_ERR("Card status 0x%08x", resp_data[0]);
                return -PB_ERR_IO;
            }

            if ((resp_data[0] & STATUS_READY_FOR_DATA) != 0U)
                break;
        }

        timeout_ms--;
        pb_delay_ms(1);
    }

    return MMC_GET_STATE(resp_data[0]);
}

int mmc_wait_state(struct mmc_device *dev, int state, unsigned int timeout_ms)
{
    int rc;

    do {
        rc = mmc_device_state(dev, timeout_ms);
        if (rc < 0)
            return rc;
    } while (rc != state);

    return PB_OK;
}

int mmc_set_bus_clock(struct mmc_device *dev, unsigned int clk_hz)
{
    return dev->hal->set_bus_clock(dev->hal->priv, clk_hz);
}

static int mmc_rw(struct mmc_device *dev, unsigned int lba, size_t length, uintptr_t buf, bool write)
{
    struct mmc_cmd cmd = {
        .arg = dev->block_addressing ? lba : lba * MMC_BLOCK_SIZE,
        .buf = (void *)buf,
        .block_size = MMC_BLOCK_SIZE,
        .blocks = length / MMC_BLOCK_SIZE,
        .write = write,
    };
    int ret;

#ifdef MMC_CORE_DEBUG_IOS
    LOG_DBG("%s %u, %zu, %p", write ? "write" : "read", lba, length, (void *)buf);
#endif

    if (dev->hal->max_chunk_bytes > 0 && length > dev->hal->max_chunk_bytes)
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
    if (dev->part == MMC_PART_RPMB) {
        ret = mmc_send_cmd_r1(dev, MMC_CMD_SET_BLOCK_COUNT, cmd.blocks);
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

    ret = mmc_request_r1(dev, &cmd);
    if (ret != 0) {
        return ret;
    }

    /* Wait buffer empty */
    do {
        ret = mmc_device_state(dev, MMC_DEFAULT_TIMEOUT_ms);
        if (ret < 0) {
            return ret;
        }
    } while ((ret != MMC_STATE_TRAN) && (ret != (write ? MMC_STATE_RCV : MMC_STATE_DATA)));

    return PB_OK;
}

int mmc_bio_xfer(struct mmc_device *dev, lba_t lba, size_t length, uintptr_t buf, bool write)
{
    size_t max_len = dev->hal->max_chunk_bytes;
    int rc = -1;

    while (length) {
        size_t chunk_len = (length > max_len) ? max_len : length;

        rc = mmc_rw(dev, lba, chunk_len, buf, write);
        if (rc < 0)
            break;

        length -= chunk_len;
        buf += chunk_len;
        lba += chunk_len / MMC_BLOCK_SIZE;
    }

    return rc;
}

struct mmc_device_info *mmc_device_info(struct mmc_device *dev)
{
    return &dev->info;
}

int mmc_read(struct mmc_device *dev, unsigned int lba, size_t length, uintptr_t buf)
{
    return mmc_rw(dev, lba, length, buf, false);
}

int mmc_write(struct mmc_device *dev, unsigned int lba, size_t length, const uintptr_t buf)
{
    return mmc_rw(dev, lba, length, buf, true);
}

int mmc_init(const struct mmc_hal *hal,
             const struct mmc_device_config *cfg,
             struct mmc_device **dev_out)
{
    struct mmc_device *dev = NULL;
    int rc;

    if ((hal == NULL) || (cfg == NULL) ||
        !(cfg->mode > MMC_BUS_MODE_INVALID && cfg->mode < MMC_BUS_MODE_END)) {
        return -PB_ERR_PARAM;
    }

    /* Again on the same HAL: the card is initialized again */
    for (unsigned int i = 0; i < MMC_CORE_MAX_DEVICES && dev == NULL; i++) {
        if (devices[i].used && devices[i].hal->priv == hal->priv)
            dev = &devices[i];
    }
    for (unsigned int i = 0; i < MMC_CORE_MAX_DEVICES && dev == NULL; i++) {
        if (!devices[i].used)
            dev = &devices[i];
    }
    if (dev == NULL)
        return -PB_ERR_MEM;

    memset(dev, 0, sizeof(*dev));
    dev->used = true;
    dev->hal = hal;
    dev->cfg = cfg;

    switch (cfg->card) {
    case MMC_CARD_TYPE_EMMC:
        rc = mmc_emmc_setup(dev);
        break;
    case MMC_CARD_TYPE_SD:
        rc = mmc_sd_setup(dev);
        break;
    default:
        rc = -PB_ERR_PARAM;
        break;
    }

    if (rc != PB_OK) {
        dev->used = false;
        return rc;
    }

    if (dev_out != NULL)
        *dev_out = dev;
    return PB_OK;
}
