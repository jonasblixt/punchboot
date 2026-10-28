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
static uint8_t last_idx;
static uint8_t data_idx;
static bool data_pending;
static bool counted; /* The data command came right after CMD23 */

static void wait_busy(const char *what)
{
    while (card->ops->busy(card)) {
        bench_advance_us(1);
        bench_check_stuck(what);
    }
}

static int hal_init(void)
{
    memset(&bus, 0, sizeof(bus));
    bus.width = 1;
    bus.vccq_mv = 1800;
    bus.vdd_mv = 3300;
    card->ops->set_bus(card, &bus);
    data_pending = false;
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

static int hal_send_cmd(uint16_t idx, uint32_t arg, uint16_t resp_type, mmc_cmd_resp_t result)
{
    int rc = raw_cmd(idx, arg, resp_type, result);

    counted = last_idx == MMC_CMD_SET_BLOCK_COUNT;
    last_idx = idx;
    data_idx = idx;
    return rc;
}

/* The auto CMD12 after an open-ended multiple block transfer */
static int stop(void)
{
    if ((data_idx == MMC_CMD_READ_MULTIPLE_BLOCK || data_idx == MMC_CMD_WRITE_MULTIPLE_BLOCK) &&
        !counted) {
        uint16_t type = data_idx == MMC_CMD_WRITE_MULTIPLE_BLOCK ? MMC_RSP_R1b : MMC_RSP_R1;
        mmc_cmd_resp_t rsp;
        int rc = raw_cmd(MMC_CMD_STOP_TRANSMISSION, 0, type, rsp);

        uint32_t errors = STATUS_ERROR_MASK;

        /* Execution errors of the transfer are reported here. A read up to
         * the last block may read ahead past it, JESD84-B51 6.8.3 */
        if (data_idx == MMC_CMD_READ_MULTIPLE_BLOCK)
            errors &= ~STATUS_ADDRESS_OUT_OF_RANGE;
        if (rc == PB_OK && (rsp[0] & errors))
            rc = -PB_ERR_IO;
        return rc;
    }
    return PB_OK;
}

static int hal_set_bus_clock(unsigned int clk_hz)
{
    bus.clk_hz = clk_hz;
    card->ops->set_bus(card, &bus);
    return PB_OK;
}

static int hal_set_bus_width(enum mmc_bus_width width)
{
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

static int hal_prepare(unsigned int lba, size_t length, uintptr_t buf)
{
    (void)lba;
    (void)buf;
    data_pending = length > 0;
    return PB_OK;
}

static int hal_read(unsigned int lba, size_t length, uintptr_t buf)
{
    size_t block = length < 512 ? length : 512;
    int rc = PB_OK;
    (void)lba;

    if (!data_pending)
        fail_msg("read() without prepare()");
    data_pending = false;

    for (size_t off = 0; off < length; off += block) {
        if (card->ops->read(card, (uint8_t *)buf + off, block) != BUS_DATA_OK) {
            rc = -PB_ERR_IO;
            break;
        }
    }

    if (stop() != PB_OK)
        rc = -PB_ERR_IO;
    return rc;
}

static int hal_write(unsigned int lba, size_t length, uintptr_t buf)
{
    int rc = PB_OK;
    (void)lba;

    if (!data_pending)
        fail_msg("write() without prepare()");
    data_pending = false;

    for (size_t off = 0; off < length; off += 512) {
        wait_busy("write busy");
        if (card->ops->write(card, (const uint8_t *)buf + off, 512) != BUS_DATA_OK) {
            rc = -PB_ERR_IO;
            break;
        }
    }

    wait_busy("write busy");
    if (stop() != PB_OK)
        rc = -PB_ERR_IO;
    return rc;
}

static int hal_set_delay_tap(unsigned int tap)
{
    (void)tap;
    return PB_OK;
}

const struct mmc_hal *hal_model_init(struct bus_card *c, size_t max_chunk_bytes)
{
    card = c;
    last_idx = 0xff;
    hal = (struct mmc_hal){
        .init = hal_init,
        .send_cmd = hal_send_cmd,
        .set_bus_clock = hal_set_bus_clock,
        .set_bus_width = hal_set_bus_width,
        .prepare = hal_prepare,
        .read = hal_read,
        .write = hal_write,
        .set_delay_tap = hal_set_delay_tap,
        .max_chunk_bytes = max_chunk_bytes,
    };
    return &hal;
}

const struct bus_cfg *hal_model_bus(void)
{
    return &bus;
}
