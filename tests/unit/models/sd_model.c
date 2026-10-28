/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * See sd_model.h.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bench.h"
#include "sd_model.h"

#define BLOCK_SIZE         512
#define SDSC_MAX_SECTORS   (2ULL * 1024 * 1024 * 1024 / BLOCK_SIZE)
#define SDHC_MAX_SECTORS   (32ULL * 1024 * 1024 * 1024 / BLOCK_SIZE)

/* Card status */
#define ST_OUT_OF_RANGE    (1u << 31)
#define ST_ADDRESS_ERROR   (1u << 30)
#define ST_BLOCK_LEN_ERROR (1u << 29)
#define ST_COM_CRC_ERROR   (1u << 23)
#define ST_ILLEGAL_COMMAND (1u << 22)
#define ST_ERROR           (1u << 19)
#define ST_STATE_SHIFT     9
#define ST_READY_FOR_DATA  (1u << 8)
#define ST_APP_CMD         (1u << 5)
#define ST_ERROR_BITS      0xfdf98008u

/* OCR, ACMD41 */
#define OCR_BUSY_N         (1u << 31)
#define OCR_CCS            (1u << 30) /* Response: high capacity */
#define OCR_HCS            (1u << 30) /* Argument: the host supports it */
#define OCR_S18            (1u << 24)
#define OCR_VOLTAGES       0x00ff8000u /* 2.7-3.6 V */

/* CMD8 argument and R7 */
#define IF_COND_VHS(arg)   (((arg) >> 8) & 0xf)
#define VHS_2V7_3V6        0x1

#define ID_CLK_MAX_HZ      400000
#define SWITCH_TIME_US     5000 /* CMD11: the clock is stopped this long */

static const unsigned int speed_max_hz[] = {
    [SD_DEFAULT_SPEED] = 25000000, [SD_HIGH_SPEED] = 50000000, [SD_SDR50] = 100000000,
    [SD_SDR104] = 208000000,       [SD_DDR50] = 50000000,
};

static const char *speed_names[] = { "default speed", "high speed", "SDR50", "SDR104", "DDR50" };

/* Tuning block for a 4 bit bus (CMD19) */
static const uint8_t tuning_4bit[64] = {
    0xff, 0x0f, 0xff, 0x00, 0xff, 0xcc, 0xc3, 0xcc, 0xc3, 0x3c, 0xcc, 0xff, 0xfe, 0xff, 0xfe, 0xef,
    0xff, 0xdf, 0xff, 0xdd, 0xff, 0xfb, 0xff, 0xfb, 0xbf, 0xff, 0x7f, 0xff, 0x77, 0xf7, 0xbd, 0xef,
    0xff, 0xf0, 0xff, 0xf0, 0x0f, 0xfc, 0xcc, 0x3c, 0xcc, 0x33, 0xcc, 0xcf, 0xff, 0xef, 0xff, 0xee,
    0xff, 0xfd, 0xff, 0xfd, 0xdf, 0xff, 0xbf, 0xff, 0xbb, 0xff, 0xf7, 0xff, 0xf7, 0x7f, 0x7b, 0xde,
};

enum xfer_src {
    SRC_MEM,
    SRC_SWITCH, /* CMD6 status */
    SRC_SCR,
    SRC_SD_STATUS,
    SRC_NUM_WR_BLOCKS,
    SRC_TUNING,
};

enum voltage_switch {
    VS_NONE,
    VS_STARTED, /* CMD11 answered, CMD and DAT held low */
    VS_DONE,
    VS_FAILED,
};

static struct sd {
    struct bus_card bus;
    struct sd_model_config cfg;
    bool inserted;
    bool powered;
    enum sd_state state;
    uint16_t rca;
    uint32_t status;
    bool app_cmd;
    bool cmd8_seen;
    bool hcs; /* The host sent HCS */
    unsigned int op_cond_count;
    bool s18a; /* ACMD41 accepted the 1.8 V request */
    enum voltage_switch vs;
    uint64_t vs_start_us;
    bool signal_1v8;
    enum sd_speed speed;
    unsigned int width; /* ACMD6 */
    bool block_count_armed;
    uint16_t block_count;
    uint32_t written_blocks; /* ACMD22 */
    uint32_t cid[4];
    uint32_t csd[4];
    uint8_t switch_status[64];
    struct bench_store store;
    struct bus_cfg bus_cfg;
    uint64_t busy_until;
    enum sd_state after_busy;

    struct {
        bool active;
        bool read;
        enum xfer_src src;
        uint64_t lba;
        bool counted;
        uint32_t left;
        size_t block_size;
        uint8_t cmd;
    } x;

    unsigned int bad_clk_hz;
    int bad_width;
    unsigned int cmd_count[64];
    unsigned int acmd_count[64];
    enum bus_data_status fail_read;
    enum bus_data_status fail_write;
} sd;

static const char *state_name(enum sd_state s)
{
    static const char *names[] = { "idle", "ready", "ident", "stby", "tran",
                                   "data", "rcv",   "prg",   "dis" };

    return s == SD_INA ? "ina" : names[s];
}

static bool high_capacity(void)
{
    return sd.cfg.sectors > SDSC_MAX_SECTORS;
}

static uint64_t now(void)
{
    return bench_now_us();
}

static bool is_busy(void)
{
    return now() < sd.busy_until;
}

static void set_busy(unsigned int us, enum sd_state after)
{
    sd.busy_until = now() + us;
    sd.after_busy = after;
}

static void progress(void)
{
    if (sd.vs == VS_STARTED && now() - sd.vs_start_us >= SWITCH_TIME_US) {
        /* The host had the clock stopped and switched the signal voltage */
        if (sd.bus_cfg.vccq_mv <= 1950) {
            sd.vs = VS_DONE;
            sd.signal_1v8 = true;
        }
    }

    if (is_busy())
        return;
    if (sd.state == SD_PRG)
        sd.state = sd.after_busy;
    else if (sd.state == SD_DIS)
        sd.state = SD_STBY;
}

static void build_cid(void)
{
    static const char pnm[] = "PBSDM";

    memset(sd.cid, 0, sizeof(sd.cid));
    bench_set_bits(sd.cid, 127, 120, 0x03); /* MID */
    bench_set_bits(sd.cid, 119, 112, 'P'); /* OID */
    bench_set_bits(sd.cid, 111, 104, 'B');
    for (int i = 0; i < 5; i++)
        bench_set_bits(sd.cid, 103 - 8 * i, 96 - 8 * i, (uint8_t)pnm[i]);
    bench_set_bits(sd.cid, 63, 56, 0x10); /* PRV */
    bench_set_bits(sd.cid, 55, 24, 0x87654321); /* PSN */
    bench_set_bits(sd.cid, 19, 8, 0x1a1); /* MDT: 2026-01 */
    bench_reg128_seal(sd.cid);
}

static void build_csd(void)
{
    memset(sd.csd, 0, sizeof(sd.csd));
    if (high_capacity()) {
        /* CSD version 2.0 */
        bench_set_bits(sd.csd, 127, 126, 1);
        bench_set_bits(sd.csd, 119, 112, 0x0e); /* TAAC */
        bench_set_bits(sd.csd, 103, 96, sd.speed == SD_DEFAULT_SPEED ? 0x32 : 0x5a);
        bench_set_bits(sd.csd, 95, 84, 0x5b5); /* CCC */
        bench_set_bits(sd.csd, 83, 80, 9);
        bench_set_bits(sd.csd, 69, 48, (uint32_t)(sd.cfg.sectors / 1024 - 1)); /* C_SIZE */
    } else {
        /* CSD version 1.0: (C_SIZE + 1) * 2^(C_SIZE_MULT + 2) blocks of 2^READ_BL_LEN */
        unsigned int bl_len = 9;
        uint64_t blocks = sd.cfg.sectors;

        while (blocks / 512 > 4096) {
            blocks /= 2;
            bl_len++;
        }
        bench_set_bits(sd.csd, 127, 126, 0);
        bench_set_bits(sd.csd, 119, 112, 0x26);
        bench_set_bits(sd.csd, 103, 96, sd.speed == SD_DEFAULT_SPEED ? 0x32 : 0x5a);
        bench_set_bits(sd.csd, 95, 84, 0x5b5);
        bench_set_bits(sd.csd, 83, 80, bl_len);
        bench_set_bits(sd.csd, 73, 62, (uint32_t)(blocks / 512 - 1));
        bench_set_bits(sd.csd, 49, 47, 7);
    }
    bench_set_bits(sd.csd, 46, 46, 1); /* ERASE_BLK_EN */
    bench_set_bits(sd.csd, 45, 39, 0x7f); /* SECTOR_SIZE */
    bench_set_bits(sd.csd, 28, 26, 2); /* R2W_FACTOR */
    bench_set_bits(sd.csd, 25, 22, 9); /* WRITE_BL_LEN */
    bench_reg128_seal(sd.csd);
}

static void go_idle(void)
{
    sd.state = SD_IDLE;
    sd.rca = 0;
    sd.app_cmd = false;
    sd.cmd8_seen = false;
    sd.hcs = false;
    sd.op_cond_count = 0;
    sd.s18a = false;
    sd.speed = SD_DEFAULT_SPEED;
    sd.width = 1;
    sd.block_count_armed = false;
    sd.x.active = false;
    sd.busy_until = 0;
    /* CMD0 doesn't change the signal voltage, a power cycle does */
    if (sd.vs != VS_DONE)
        sd.vs = VS_NONE;
    build_csd();
}

static void power(bool on)
{
    if (on == sd.powered)
        return;
    sd.powered = on;
    sd.vs = VS_NONE;
    sd.signal_1v8 = false;
    go_idle();
}

static unsigned int max_clock_hz(void)
{
    if (sd.state == SD_IDLE || sd.state == SD_READY || sd.state == SD_IDENT)
        return ID_CLK_MAX_HZ;
    return speed_max_hz[sd.speed];
}

static void check_clock(const char *what)
{
    unsigned int max = max_clock_hz();
    unsigned int clk = sd.bus_cfg.clk_hz;

    if (clk <= max) {
        sd.bad_clk_hz = 0;
        return;
    }
    if (clk == sd.bad_clk_hz)
        return;
    sd.bad_clk_hz = clk;

    if (max == ID_CLK_MAX_HZ)
        bench_violation(SD_PLS "identification",
                        "%s at %u Hz in %s, the limit is 400 kHz",
                        what,
                        clk,
                        state_name(sd.state));
    else
        bench_violation(SD_PLS "CMD6",
                        "%s at %u Hz in %s, the limit is %u Hz",
                        what,
                        clk,
                        speed_names[sd.speed],
                        max);
}

static bool check_data_bus(void)
{
    bool ddr = sd.speed == SD_DDR50;

    check_clock("Data block");
    if (sd.bus_cfg.width != sd.width || sd.bus_cfg.ddr != ddr) {
        int key = (int)(sd.bus_cfg.width * 2 + sd.bus_cfg.ddr);

        if (key != sd.bad_width) {
            sd.bad_width = key;
            bench_violation(SD_PLS "ACMD6",
                            "Data on a %u bit %s bus, the card uses %u bit %s",
                            sd.bus_cfg.width,
                            sd.bus_cfg.ddr ? "DDR" : "SDR",
                            sd.width,
                            ddr ? "DDR" : "SDR");
        }
        return false;
    }
    sd.bad_width = 0;
    return true;
}

static enum bus_rsp_type rsp_type(uint8_t idx, bool app)
{
    if (app && idx == 41)
        return BUS_RSP_48; /* R3 */

    switch (idx) {
    case 0:
    case 4:
    case 15:
        return BUS_RSP_NONE;
    case 2:
    case 9:
    case 10:
        return BUS_RSP_136;
    case 12:
        /* R1b, but only a stopped write leaves the card busy */
        return sd.state == SD_RCV ? BUS_RSP_48_BUSY : BUS_RSP_48;
    case 7:
    case 28:
    case 29:
    case 38:
        return BUS_RSP_48_BUSY;
    default:
        return BUS_RSP_48;
    }
}

static void check_rsp_type(const struct bus_cmd *cmd, bool app)
{
    static const char *names[] = { "none", "48 bit", "48 bit with busy", "136 bit" };
    enum bus_rsp_type want = rsp_type(cmd->idx, app);

    if (cmd->rsp == want || (want == BUS_RSP_48 && cmd->rsp == BUS_RSP_48_BUSY))
        return;
    bench_violation(SD_PLS "responses",
                    "%sCMD%u has a %s response, the host expects %s",
                    app ? "A" : "",
                    cmd->idx,
                    names[want],
                    names[cmd->rsp]);
}

static void
start_xfer(uint8_t idx, bool read, enum xfer_src src, uint64_t lba, size_t bs, uint32_t count)
{
    sd.x.active = true;
    sd.x.read = read;
    sd.x.src = src;
    sd.x.lba = lba;
    sd.x.block_size = bs;
    sd.x.cmd = idx;
    sd.x.counted = count != 0;
    sd.x.left = count;
    sd.state = read ? SD_DATA : SD_RCV;
}

/* CMD6 SWITCH_FUNC, only the access mode group (1) is acted on */
static void cmd_switch_func(uint32_t arg)
{
    bool set = arg & (1u << 31);
    unsigned int fn = arg & 0xf;
    uint16_t support = (1u << SD_DEFAULT_SPEED) | (1u << SD_HIGH_SPEED) | 0x8000;
    unsigned int result = sd.speed;
    uint8_t *s = sd.switch_status;

    if (sd.cfg.uhs && sd.signal_1v8)
        support |= (1u << SD_SDR50) | (1u << SD_SDR104) | (1u << SD_DDR50);

    if (fn != 0xf) {
        if (fn > SD_DDR50 || !(support & (1u << fn)))
            result = 0xf;
        else
            result = fn;
    }

    memset(s, 0, sizeof(sd.switch_status));
    s[0] = 0x00; /* Maximum current: 100 mA */
    s[1] = 0x64;
    for (int g = 0; g < 5; g++) {
        /* Groups 6..2: only the default function */
        s[2 + 2 * g] = 0x80;
        s[3 + 2 * g] = 0x01;
    }
    s[12] = support >> 8;
    s[13] = support & 0xff;
    s[16] = result & 0xf; /* Group 1, the others report 0 */
    s[17] = 0x01; /* Data structure version */

    if (set && result != 0xf) {
        if (result >= SD_SDR50 && sd.width != 4)
            bench_violation(
                SD_PLS "CMD6", "UHS-I mode selected on a %u bit bus, it needs 4 bits", sd.width);
        sd.speed = result;
        build_csd();
    }
}

static bool cmd_rw(uint8_t idx, uint32_t arg, uint32_t *err)
{
    bool read = idx == 17 || idx == 18;
    bool multiple = idx == 18 || idx == 25;
    uint64_t lba;

    if (high_capacity()) {
        lba = arg;
    } else {
        if (arg % BLOCK_SIZE) {
            *err |= ST_ADDRESS_ERROR;
            return true;
        }
        lba = arg / BLOCK_SIZE;
    }
    if (lba >= sd.cfg.sectors) {
        *err |= ST_OUT_OF_RANGE;
        return true;
    }

    start_xfer(idx,
               read,
               SRC_MEM,
               lba,
               BLOCK_SIZE,
               multiple ? (sd.block_count_armed ? sd.block_count : 0) : 1);
    if (!read)
        sd.written_blocks = 0;
    return true;
}

static bool execute_acmd(const struct bus_cmd *cmd, struct bus_rsp *rsp, uint32_t *err)
{
    uint32_t arg = cmd->arg;
    enum sd_state s = sd.state;
    (void)err;

    sd.acmd_count[cmd->idx & 63]++;

    switch (cmd->idx) {
    case 6:
        if (s != SD_TRAN)
            return false;
        if ((arg & 3) == 0)
            sd.width = 1;
        else if ((arg & 3) == 2)
            sd.width = 4;
        else
            return false;
        return true;
    case 13:
        if (s != SD_TRAN)
            return false;
        start_xfer(13, true, SRC_SD_STATUS, 0, 64, 1);
        return true;
    case 22:
        if (s != SD_TRAN)
            return false;
        start_xfer(22, true, SRC_NUM_WR_BLOCKS, 0, 4, 1);
        return true;
    case 23:
        return s == SD_TRAN;
    case 41: {
        uint32_t ocr = OCR_VOLTAGES;
        uint32_t window = arg & 0x00ffffff;

        if (s != SD_IDLE)
            return false;
        rsp->idx = 0x3f;
        if (window != 0 && !(window & OCR_VOLTAGES)) {
            sd.state = SD_INA;
            rsp->timeout = true;
            return true;
        }
        if (window == 0) {
            /* Inquiry: the OCR without leaving idle */
            rsp->r[0] = ocr;
            return true;
        }

        sd.hcs = arg & OCR_HCS;
        /* An SDHC card is not ready for a host that doesn't do CMD8 and HCS */
        bool can_init = !high_capacity() || (sd.cmd8_seen && sd.hcs);

        if (can_init && ++sd.op_cond_count > sd.cfg.op_cond_busy) {
            ocr |= OCR_BUSY_N;
            if (high_capacity())
                ocr |= OCR_CCS;
            if (sd.cfg.uhs && (arg & OCR_S18) && sd.hcs && !sd.signal_1v8) {
                ocr |= OCR_S18;
                sd.s18a = true;
            }
            sd.state = SD_READY;
        }
        rsp->r[0] = ocr;
        return true;
    }
    case 51:
        if (s != SD_TRAN)
            return false;
        start_xfer(51, true, SRC_SCR, 0, 8, 1);
        return true;
    default:
        return false;
    }
}

static bool execute(const struct bus_cmd *cmd, struct bus_rsp *rsp, uint32_t *err)
{
    uint32_t arg = cmd->arg;
    uint16_t rca = arg >> 16;
    bool addressed = rca == sd.rca;
    enum sd_state s = sd.state;
    bool armed = sd.block_count_armed;

    sd.block_count_armed = false;

    switch (cmd->idx) {
    case 0:
        go_idle();
        rsp->timeout = true;
        return true;
    case 2:
        if (s != SD_READY)
            return false;
        if (sd.s18a && sd.vs != VS_DONE) {
            bench_violation(SD_PLS "CMD11", "CMD2 after S18A without the voltage switch");
        }
        memcpy(rsp->r, sd.cid, sizeof(rsp->r));
        rsp->idx = 0x3f;
        sd.state = SD_IDENT;
        return true;
    case 3:
        if (s != SD_IDENT && s != SD_STBY)
            return false;
        sd.rca = s == SD_IDENT ? sd.cfg.rca : sd.rca + 1;
        sd.state = SD_STBY;
        /* R6: RCA and status bits 23, 22, 19, 12:0 */
        rsp->r[0] = ((uint32_t)sd.rca << 16) | ((sd.status & ST_COM_CRC_ERROR) ? 1u << 15 : 0) |
                    ((sd.status & ST_ILLEGAL_COMMAND) ? 1u << 14 : 0) |
                    ((sd.status & ST_ERROR) ? 1u << 13 : 0) | ((uint32_t)s << ST_STATE_SHIFT) |
                    ST_READY_FOR_DATA;
        sd.status &= ~ST_ERROR_BITS;
        rsp->idx = 3;
        return true;
    case 4:
        if (s != SD_STBY)
            return false;
        rsp->timeout = true;
        return true;
    case 6:
        if (s != SD_TRAN)
            return false;
        cmd_switch_func(arg);
        start_xfer(6, true, SRC_SWITCH, 0, 64, 1);
        return true;
    case 7:
        if (s == SD_STBY) {
            if (!addressed) {
                rsp->timeout = true;
                return true;
            }
            sd.state = SD_TRAN;
            return true;
        }
        if (s == SD_DIS && addressed) {
            sd.state = SD_PRG;
            sd.after_busy = SD_TRAN;
            return true;
        }
        if ((s == SD_TRAN || s == SD_DATA || s == SD_PRG) && !addressed) {
            sd.x.active = false;
            sd.state = s == SD_PRG ? SD_DIS : SD_STBY;
            rsp->timeout = true;
            return true;
        }
        return false;
    case 8:
        if (s != SD_IDLE || sd.cfg.v1)
            return false;
        sd.cmd8_seen = true;
        if (IF_COND_VHS(arg) != VHS_2V7_3V6) {
            rsp->timeout = true; /* The card can't work at this voltage */
            return true;
        }
        rsp->r[0] = arg & 0xfff;
        return true;
    case 9:
    case 10:
        if (s != SD_STBY)
            return false;
        if (!addressed) {
            rsp->timeout = true;
            return true;
        }
        memcpy(rsp->r, cmd->idx == 9 ? sd.csd : sd.cid, sizeof(rsp->r));
        rsp->idx = 0x3f;
        return true;
    case 11:
        if (s != SD_READY || !sd.s18a || sd.vs != VS_NONE)
            return false;
        sd.vs = VS_STARTED;
        sd.vs_start_us = now();
        return true;
    case 12:
        if (s == SD_DATA) {
            sd.x.active = false;
            sd.state = SD_TRAN;
            return true;
        }
        if (s == SD_RCV) {
            sd.x.active = false;
            sd.state = SD_PRG;
            set_busy(sd.cfg.write_busy_us, SD_TRAN);
            return true;
        }
        if (s == SD_TRAN)
            bench_violation(SD_PLS "CMD23",
                            "CMD12 in tran: a transfer with a CMD23 block count ends by itself");
        return false;
    case 13:
        if (s == SD_IDLE || s == SD_READY || s == SD_IDENT)
            return false;
        if (!addressed) {
            rsp->timeout = true;
            return true;
        }
        return true;
    case 15:
        if (s == SD_IDLE || s == SD_READY || s == SD_IDENT)
            return false;
        if (addressed)
            sd.state = SD_INA;
        rsp->timeout = true;
        return true;
    case 16:
        if (s != SD_TRAN)
            return false;
        if (arg != BLOCK_SIZE)
            *err |= ST_BLOCK_LEN_ERROR;
        return true;
    case 17:
    case 18:
    case 24:
    case 25:
        if (s != SD_TRAN)
            return false;
        sd.block_count_armed = armed;
        cmd_rw(cmd->idx, arg, err);
        sd.block_count_armed = false;
        return true;
    case 19:
        if (s != SD_TRAN)
            return false;
        if (sd.speed != SD_SDR50 && sd.speed != SD_SDR104) {
            bench_violation(
                SD_PLS "CMD19", "Tuning in %s, it is for SDR50 and SDR104", speed_names[sd.speed]);
            return false;
        }
        start_xfer(19, true, SRC_TUNING, 0, 64, 1);
        return true;
    case 23:
        if (s != SD_TRAN)
            return false;
        if (!sd.cfg.cmd23) {
            bench_violation(SD_PLS "SCR", "CMD23 to a card whose SCR doesn't list it");
            return false;
        }
        sd.block_count = arg & 0xffff;
        sd.block_count_armed = true;
        return true;
    case 55:
        if (s == SD_INA)
            return false;
        if (!addressed) {
            rsp->timeout = true;
            return true;
        }
        sd.app_cmd = true;
        return true;
    default:
        return false;
    }
}

static void card_cmd(struct bus_card *card, const struct bus_cmd *cmd, struct bus_rsp *rsp)
{
    enum sd_state before;
    uint32_t err = 0;
    uint32_t pending;
    bool app;
    bool ok;
    (void)card;

    memset(rsp, 0, sizeof(*rsp));
    rsp->idx = cmd->idx;

    if (!sd.powered || sd.state == SD_INA || sd.vs == VS_FAILED) {
        rsp->timeout = true;
        return;
    }

    progress();
    sd.cmd_count[cmd->idx & 63]++;

    if (sd.vs == VS_STARTED) {
        if (sd.bus_cfg.vccq_mv > 1950)
            bench_violation(
                SD_PLS "CMD11", "CMD%u while the signal voltage is still 3.3 V", cmd->idx);
        else
            bench_violation(SD_PLS "CMD11",
                            "CMD%u less than 5 ms after CMD11, the clock is stopped that long",
                            cmd->idx);
        sd.vs = VS_FAILED;
        rsp->timeout = true;
        return;
    }
    if (sd.signal_1v8 && sd.bus_cfg.vccq_mv > 1950) {
        bench_violation(SD_PLS "CMD11", "3.3 V signaling after the switch to 1.8 V");
        rsp->timeout = true;
        return;
    }

    check_clock("Command");

    before = sd.state;
    pending = sd.status;
    app = sd.app_cmd;
    sd.app_cmd = false;

    if (before == SD_PRG && is_busy() && cmd->idx != 0 && cmd->idx != 7 && cmd->idx != 13 &&
        cmd->idx != 15) {
        bench_violation(SD_PLS "states", "CMD%u while the card is busy programming", cmd->idx);
        sd.status |= ST_ILLEGAL_COMMAND;
        rsp->timeout = true;
        return;
    }

    check_rsp_type(cmd, app);

    if (app && cmd->idx != 55)
        ok = execute_acmd(cmd, rsp, &err);
    else
        ok = execute(cmd, rsp, &err);

    if (!ok) {
        if (before != SD_IDLE) {
            bench_violation(SD_PLS "states",
                            "%sCMD%u (0x%08x) is illegal in %s",
                            app ? "A" : "",
                            cmd->idx,
                            cmd->arg,
                            state_name(before));
        }
        sd.status |= ST_ILLEGAL_COMMAND;
        sd.state = before;
        rsp->timeout = true;
        return;
    }

    if (rsp->timeout || rsp->idx == 0x3f || (cmd->idx == 3 && !app) || (cmd->idx == 8 && !app))
        return;

    rsp->r[0] = pending | err | ((uint32_t)before << ST_STATE_SHIFT) |
                (before == SD_PRG && is_busy() ? 0 : ST_READY_FOR_DATA) |
                (sd.app_cmd || app ? ST_APP_CMD : 0);
    sd.status &= ~(pending & ST_ERROR_BITS);
}

static void fill_scr(uint8_t *out)
{
    unsigned int security = !high_capacity() ? 2 : sd.cfg.sectors > SDHC_MAX_SECTORS ? 4 : 3;

    memset(out, 0, 8);
    out[0] = sd.cfg.v1 ? 0x00 : 0x02; /* SCR_STRUCTURE 0, SD_SPEC */
    out[1] = (uint8_t)(security << 4) | 0x05; /* SD_SECURITY, SD_BUS_WIDTHS 1 and 4 bit */
    out[2] = sd.cfg.v1 ? 0x00 : 0x80; /* SD_SPEC3 */
    out[3] = sd.cfg.cmd23 ? 0x02 : 0x00; /* CMD_SUPPORT */
}

static enum bus_data_status card_read(struct bus_card *card, void *buf, size_t len)
{
    enum bus_data_status st = BUS_DATA_OK;
    (void)card;

    if (!sd.powered)
        return BUS_DATA_TIMEOUT;
    progress();
    if (sd.state != SD_DATA || !sd.x.active || !sd.x.read)
        return BUS_DATA_TIMEOUT;

    if (!check_data_bus())
        st = BUS_DATA_CRC;
    if (len != sd.x.block_size) {
        bench_violation(SD_PLS "data",
                        "CMD%u: the host reads %zu byte blocks, the card sends %zu",
                        sd.x.cmd,
                        len,
                        sd.x.block_size);
        st = BUS_DATA_CRC;
    }

    switch (sd.x.src) {
    case SRC_MEM:
        if (sd.x.lba >= sd.cfg.sectors) {
            sd.status |= ST_OUT_OF_RANGE;
            return BUS_DATA_TIMEOUT;
        }
        bench_store_read(&sd.store, sd.x.lba, buf);
        break;
    case SRC_SWITCH:
        memcpy(buf, sd.switch_status, 64);
        break;
    case SRC_SCR:
        fill_scr(buf);
        break;
    case SRC_SD_STATUS: {
        uint8_t *out = buf;

        memset(out, 0, 64);
        out[0] = (sd.width == 4 ? 2 : 0) << 6; /* DAT_BUS_WIDTH */
        break;
    }
    case SRC_NUM_WR_BLOCKS: {
        uint8_t *out = buf;

        out[0] = sd.written_blocks >> 24;
        out[1] = sd.written_blocks >> 16;
        out[2] = sd.written_blocks >> 8;
        out[3] = sd.written_blocks;
        break;
    }
    case SRC_TUNING:
        memcpy(buf, tuning_4bit, 64);
        break;
    }

    if (sd.fail_read != BUS_DATA_OK) {
        st = sd.fail_read;
        sd.fail_read = BUS_DATA_OK;
    }
    if (st != BUS_DATA_OK)
        memset(buf, 0xa5, len);

    sd.x.lba++;
    if (sd.x.counted && --sd.x.left == 0) {
        sd.x.active = false;
        sd.state = SD_TRAN;
    }
    return st;
}

static enum bus_data_status card_write(struct bus_card *card, const void *buf, size_t len)
{
    (void)card;

    if (!sd.powered)
        return BUS_DATA_TIMEOUT;
    progress();
    if (sd.state != SD_RCV || !sd.x.active || sd.x.read)
        return BUS_DATA_TIMEOUT;

    if (is_busy())
        bench_violation(SD_PLS "data", "Data block sent while DAT0 is busy");
    if (!check_data_bus())
        return BUS_DATA_CRC;
    if (len != sd.x.block_size) {
        bench_violation(SD_PLS "data",
                        "CMD%u: %zu byte blocks, the card takes %zu",
                        sd.x.cmd,
                        len,
                        sd.x.block_size);
        return BUS_DATA_CRC;
    }
    if (sd.fail_write != BUS_DATA_OK) {
        enum bus_data_status st = sd.fail_write;

        sd.fail_write = BUS_DATA_OK;
        sd.x.active = false;
        if (sd.x.cmd != 25)
            sd.state = SD_TRAN;
        return st;
    }
    if (sd.x.lba >= sd.cfg.sectors) {
        sd.status |= ST_OUT_OF_RANGE;
        sd.x.active = false;
        return BUS_DATA_OK;
    }

    bench_store_write(&sd.store, sd.x.lba, buf);
    sd.x.lba++;
    sd.written_blocks++;
    set_busy(sd.cfg.write_busy_us, SD_TRAN);
    if (sd.x.counted && --sd.x.left == 0) {
        sd.x.active = false;
        sd.state = SD_PRG;
    }
    return BUS_DATA_OK;
}

static bool card_busy(struct bus_card *card)
{
    (void)card;

    if (!sd.powered)
        return false;
    progress();
    /* During the voltage switch the card holds DAT[3:0] low */
    return is_busy() || sd.vs == VS_STARTED;
}

static void card_set_bus(struct bus_card *card, const struct bus_cfg *cfg)
{
    (void)card;
    sd.bus_cfg = *cfg;
    power(sd.inserted && cfg->vdd_mv >= 2700);
}

static bool card_present(struct bus_card *card)
{
    (void)card;
    return sd.inserted;
}

static const struct bus_card_ops ops = {
    .cmd = card_cmd,
    .read = card_read,
    .write = card_write,
    .busy = card_busy,
    .set_bus = card_set_bus,
    .present = card_present,
};

void sd_model_default_config(struct sd_model_config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->sectors = 32ULL * 1024 * 1024 * 2; /* 32 GiB, SDHC */
    cfg->uhs = true;
    cfg->cmd23 = true;
    cfg->op_cond_busy = 5;
    cfg->write_busy_us = 100;
    cfg->rca = 0xaaaa;
}

struct bus_card *sd_model_init(const struct sd_model_config *cfg)
{
    sd_model_free();
    sd.bus.ops = &ops;
    sd.cfg = *cfg;
    sd.inserted = true;
    bench_store_init(&sd.store, cfg->sectors, BLOCK_SIZE);
    build_cid();
    go_idle();
    return &sd.bus;
}

void sd_model_free(void)
{
    if (sd.store.chunks != NULL)
        bench_store_free(&sd.store);
    memset(&sd, 0, sizeof(sd));
}

void sd_model_insert(bool inserted)
{
    sd.inserted = inserted;
    power(inserted && sd.bus_cfg.vdd_mv >= 2700);
}

enum sd_state sd_model_state(void)
{
    progress();
    return sd.state;
}

enum sd_speed sd_model_speed(void)
{
    return sd.speed;
}

unsigned int sd_model_bus_width(void)
{
    return sd.width;
}

bool sd_model_1v8(void)
{
    progress();
    return sd.signal_1v8;
}

unsigned int sd_model_cmd_count(uint8_t idx)
{
    return sd.cmd_count[idx & 63];
}

unsigned int sd_model_acmd_count(uint8_t idx)
{
    return sd.acmd_count[idx & 63];
}

void sd_model_read(uint64_t lba, void *buf, size_t blocks)
{
    for (size_t i = 0; i < blocks; i++)
        bench_store_read(&sd.store, lba + i, (uint8_t *)buf + i * BLOCK_SIZE);
}

void sd_model_write(uint64_t lba, const void *buf, size_t blocks)
{
    for (size_t i = 0; i < blocks; i++)
        bench_store_write(&sd.store, lba + i, (const uint8_t *)buf + i * BLOCK_SIZE);
}

void sd_model_fail_read(enum bus_data_status status)
{
    sd.fail_read = status;
}

void sd_model_fail_write(enum bus_data_status status)
{
    sd.fail_write = status;
}
