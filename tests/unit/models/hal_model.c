/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <pb/pb.h>
#include <string.h>

#include "bench.h"
#include "hal_model.h"

static struct bus_card *card;
static struct bus_cfg bus;
static struct mmc_hal hal;
static unsigned int tap;
#define HAL_TAPS 128

static int win_lo = 0, win_hi = HAL_TAPS - 1;
static int gap_lo = 1, gap_hi = 0;

static void wait_busy(const char *what)
{
    while (card->ops->busy(card)) {
        bench_advance_us(1);
        bench_check_stuck(what);
    }
}

static int hal_init(void *priv)
{
    (void)priv;
    memset(&bus, 0, sizeof(bus));
    bus.width = 1;
    bus.vccq_mv = 1800;
    bus.vdd_mv = 3300;
    card->ops->set_bus(card, &bus);
    return PB_OK;
}

static int raw_cmd(uint8_t idx, uint32_t arg, uint16_t resp_type, mmc_cmd_resp_t result)
{
    struct bus_cmd cmd = { .idx = idx, .arg = arg };
    struct bus_rsp rsp;

    if (!(resp_type & MMC_RSP_PRESENT))
        cmd.rsp = BUS_RSP_NONE;
    else if (resp_type & MMC_RSP_136)
        cmd.rsp = BUS_RSP_136;
    else if (resp_type & MMC_RSP_BUSY)
        cmd.rsp = BUS_RSP_48_BUSY;
    else
        cmd.rsp = BUS_RSP_48;

    card->ops->cmd(card, &cmd, &rsp);

    if (cmd.rsp == BUS_RSP_NONE)
        return PB_OK;
    if (rsp.timeout)
        return -PB_ERR_TIMEOUT;
    if ((resp_type & MMC_RSP_CRC) && rsp.crc_error)
        return -PB_ERR_IO;
    if ((resp_type & MMC_RSP_OPCODE) && rsp.idx != idx)
        return -PB_ERR_IO;

    if (result != NULL) {
        if (cmd.rsp == BUS_RSP_136) {
            memcpy(result, rsp.r, sizeof(mmc_cmd_resp_t));
            result[0] &= ~0xffu;
        } else {
            result[0] = rsp.r[0];
        }
    }

    if (cmd.rsp == BUS_RSP_48_BUSY)
        wait_busy("R1b busy");

    return PB_OK;
}

/* The CMD12 after an open-ended multiple block transfer */
static int stop(const struct mmc_cmd *cmd)
{
    uint16_t type = cmd->write ? MMC_RSP_R1b : MMC_RSP_R1;
    uint32_t errors = STATUS_ERROR_MASK;
    mmc_cmd_resp_t rsp;
    int rc = raw_cmd(MMC_CMD_STOP_TRANSMISSION, 0, type, rsp);

    /* Execution errors of the transfer are reported here. A read up to
     * the last block may read ahead past it, JESD84-B51 6.8.3 */
    if (!cmd->write)
        errors &= ~STATUS_ADDRESS_OUT_OF_RANGE;
    if (rc == PB_OK && (rsp[0] & errors))
        rc = -PB_ERR_IO;
    return rc;
}

static int hal_request(void *priv, struct mmc_cmd *cmd)
{
    (void)priv;
    int rc = raw_cmd(cmd->idx, cmd->arg, cmd->resp_type, cmd->resp);

    if (rc != PB_OK || cmd->blocks == 0)
        return rc;

    for (size_t n = 0; n < cmd->blocks && rc == PB_OK; n++) {
        uint8_t *block = (uint8_t *)cmd->buf + n * cmd->block_size;

        if (cmd->write) {
            wait_busy("write busy");
            if (card->ops->write(card, block, cmd->block_size) != BUS_DATA_OK)
                rc = -PB_ERR_IO;
        } else if (card->ops->read(card, block, cmd->block_size) != BUS_DATA_OK) {
            rc = -PB_ERR_IO;
        } else if (bus.clk_hz > 52000000 && ((int)tap < win_lo || (int)tap > win_hi ||
                                             ((int)tap >= gap_lo && (int)tap <= gap_hi))) {
            /* Sampled outside the data eye */
            rc = -PB_ERR_IO;
        }
    }
    if (cmd->write)
        wait_busy("write busy");

    /* After an error the card is stopped too */
    if (cmd->stop || (rc != PB_OK && cmd->blocks > 1)) {
        int stop_rc = stop(cmd);

        if (rc == PB_OK)
            rc = stop_rc;
    }
    return rc;
}

static int hal_set_bus_clock(void *priv, unsigned int clk_hz)
{
    (void)priv;
    bus.clk_hz = clk_hz;
    card->ops->set_bus(card, &bus);
    return PB_OK;
}

static int hal_set_bus_width(void *priv, enum mmc_bus_width width)
{
    (void)priv;
    switch (width) {
    case MMC_BUS_WIDTH_1BIT:
        bus.width = 1;
        bus.ddr = false;
        break;
    case MMC_BUS_WIDTH_4BIT:
        bus.width = 4;
        bus.ddr = false;
        break;
    case MMC_BUS_WIDTH_8BIT:
        bus.width = 8;
        bus.ddr = false;
        break;
    case MMC_BUS_WIDTH_4BIT_DDR:
        bus.width = 4;
        bus.ddr = true;
        break;
    case MMC_BUS_WIDTH_8BIT_DDR:
    case MMC_BUS_WIDTH_8BIT_DDR_STROBE:
        bus.width = 8;
        bus.ddr = true;
        break;
    default:
        return -PB_ERR_PARAM;
    }
    card->ops->set_bus(card, &bus);
    return PB_OK;
}

static int hal_set_delay_tap(void *priv, unsigned int t)
{
    (void)priv;
    if (t >= HAL_TAPS)
        fail_msg("Delay tap %u, there are %u", t, HAL_TAPS);
    tap = t;
    return PB_OK;
}

const struct mmc_hal *hal_model_init(struct bus_card *c, size_t max_chunk_bytes)
{
    card = c;
    tap = 0;
    hal = (struct mmc_hal){
        .priv = &hal,
        .init = hal_init,
        .request = hal_request,
        .set_bus_clock = hal_set_bus_clock,
        .set_bus_width = hal_set_bus_width,
        .set_delay_tap = hal_set_delay_tap,
        .delay_taps = HAL_TAPS,
        .max_chunk_bytes = max_chunk_bytes,
    };
    return &hal;
}

const struct bus_cfg *hal_model_bus(void)
{
    return &bus;
}

void hal_model_set_window(int lo, int hi)
{
    win_lo = lo;
    win_hi = hi;
}

unsigned int hal_model_tap(void)
{
    return tap;
}

void hal_model_set_gap(int lo, int hi)
{
    gap_lo = lo;
    gap_hi = hi;
}
