/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * See emmc_model.h. Section numbers are those of JESD84-B51 (e.MMC 5.1).
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
#include "emmc_model.h"

#define BLOCK_SIZE                     512
#define HIGH_CAPACITY                  (2ULL * 1024 * 1024 * 1024 / BLOCK_SIZE) /* 6.4.2 */
#define SZ_128K_BLOCKS                 (128 * 1024 / BLOCK_SIZE)

/* Device status bits, Table 68 */
#define ST_ADDRESS_OUT_OF_RANGE        (1u << 31)
#define ST_ADDRESS_MISALIGN            (1u << 30)
#define ST_BLOCK_LEN_ERROR             (1u << 29)
#define ST_ERASE_SEQ_ERROR             (1u << 28)
#define ST_COM_CRC_ERROR               (1u << 23)
#define ST_ILLEGAL_COMMAND             (1u << 22)
#define ST_ERROR                       (1u << 19)
#define ST_STATE_SHIFT                 9
#define ST_READY_FOR_DATA              (1u << 8)
#define ST_SWITCH_ERROR                (1u << 7)
#define ST_APP_CMD                     (1u << 5)
/* Error bits, cleared once reported (clear condition B) */
#define ST_ERROR_BITS                  0xfdffe080u

/* OCR, 6.4.2 and 7.1 */
#define OCR_BUSY_N                     (1u << 31) /* Set when power up is done */
#define OCR_SECTOR                     (2u << 29)
#define OCR_ACCESS_MASK                (3u << 29)
#define OCR_VOLTAGES                   0x00ff8080u /* 1.70-1.95 V and 2.7-3.6 V */

/* EXT_CSD, 7.4 */
#define EXT_CSD_S_CMD_SET              504
#define EXT_CSD_HPI_FEATURES           503
#define EXT_CSD_BKOPS_SUPPORT          502
#define EXT_CSD_PRE_EOL_INFO           267
#define EXT_CSD_LIFE_TIME_A            268
#define EXT_CSD_LIFE_TIME_B            269
#define EXT_CSD_GENERIC_CMD6_TIME      248
#define EXT_CSD_POWER_OFF_LONG_TIME    247
#define EXT_CSD_BOOT_INFO              228
#define EXT_CSD_BOOT_SIZE_MULT         226
#define EXT_CSD_HC_ERASE_GRP_SIZE      224
#define EXT_CSD_ERASE_TIMEOUT_MULT     223
#define EXT_CSD_REL_WR_SEC_C           222
#define EXT_CSD_HC_WP_GRP_SIZE         221
#define EXT_CSD_S_A_TIMEOUT            217
#define EXT_CSD_SEC_COUNT              212
#define EXT_CSD_PARTITION_SWITCH_TIME  199
#define EXT_CSD_OUT_OF_INTERRUPT_TIME  198
#define EXT_CSD_DRIVER_STRENGTH        197
#define EXT_CSD_DEVICE_TYPE            196
#define EXT_CSD_CSD_STRUCTURE          194
#define EXT_CSD_REV                    192
#define EXT_CSD_HS_TIMING              185
#define EXT_CSD_STROBE_SUPPORT         184
#define EXT_CSD_BUS_WIDTH              183
#define EXT_CSD_PARTITION_CONFIG       179
#define EXT_CSD_BOOT_CONFIG_PROT       178
#define EXT_CSD_BOOT_BUS_CONDITIONS    177
#define EXT_CSD_RPMB_SIZE_MULT         168
#define EXT_CSD_WR_REL_PARAM           166
#define EXT_CSD_PARTITIONING_SUPPORT   160
#define EXT_CSD_POWER_OFF_NOTIFICATION 34

/* DEVICE_TYPE bits, 7.4.59 */
#define DT_HS26                        (1u << 0)
#define DT_HS52                        (1u << 1)
#define DT_DDR52_18_3V                 (1u << 2)
#define DT_DDR52_12                    (1u << 3)
#define DT_HS200_18                    (1u << 4)
#define DT_HS200_12                    (1u << 5)
#define DT_HS400_18                    (1u << 6)
#define DT_HS400_12                    (1u << 7)

/* HS_TIMING, 7.4.65 */
#define TIMING_COMPAT                  0
#define TIMING_HS                      1
#define TIMING_HS200                   2
#define TIMING_HS400                   3

/* BUS_WIDTH, 7.4.67 */
#define BW_STROBE                      0x80
#define BW_MODE_MASK                   0x0f
#define BW_RESERVED                    0x70

/* PARTITION_CONFIG, 7.4.69 */
#define PC_ACCESS_MASK                 0x07
#define PC_BOOT_MASK                   0x38
#define PC_BOOT_SHIFT                  3
#define PC_BOOT_ACK                    0x40
#define PC_RESERVED                    0x80

/* BOOT_CONFIG_PROT, 7.4.70 */
#define BCP_PWR                        (1u << 0)
#define BCP_PERM                       (1u << 4)

/* POWER_OFF_NOTIFICATION, 7.4.110 */
#define PON_NONE                       0
#define PON_POWERED_ON                 1
#define PON_OFF_SHORT                  2
#define PON_OFF_LONG                   3
#define PON_SLEEP                      4

/* Clock limits, 10.6 and 10.8 (fOD has a +20 kHz tolerance) */
#define FOD_MAX_HZ                     420000
#define F_COMPAT_MAX_HZ                26000000
#define F_HS_MAX_HZ                    52000000
#define F_HS200_MAX_HZ                 200000000

/* CMD6 argument, Table 49 */
#define SWITCH_ACCESS(arg)             (((arg) >> 24) & 3)
#define SWITCH_INDEX(arg)              (((arg) >> 16) & 0xff)
#define SWITCH_VALUE(arg)              (((arg) >> 8) & 0xff)
#define SWITCH_MBZ                     0xfc0000f8u
#define ACCESS_CMD_SET                 0
#define ACCESS_SET_BITS                1
#define ACCESS_CLEAR_BITS              2
#define ACCESS_WRITE_BYTE              3

/* RPMB frame, 6.6.22.1: offsets in the 512 byte frame */
#define RPMB_FRAME_RESULT              508
#define RPMB_FRAME_REQ_RESP            510
#define RPMB_RESULT_NO_KEY             0x0007

/*
 * How an EXT_CSD field may be written, from the cell types of Table 93
 * ("E": kept over resets, "E_P": cleared by power, reset and CMD0, "C_P":
 * cleared by power and hardware reset only).
 */
enum field_type {
    F_RO, /* R and reserved: CMD6 fails */
    F_OTP, /* R/W and W: once, ever */
    F_NV, /* R/W/E */
    F_EP, /* R/W/E_P */
    F_EP_WO, /* W/E_P: reads as 0 */
    F_CP, /* R/W/C_P */
};

struct field {
    uint16_t index;
    uint8_t len;
    enum field_type type;
    const char *name;
};

/* The modes segment, [191:0]. Not listed: reserved */
static const struct field fields[] = {
    { 191, 1, F_EP, "CMD_SET" },
    { 189, 1, F_RO, "CMD_SET_REV" },
    { 187, 1, F_EP, "POWER_CLASS" },
    { 185, 1, F_EP, "HS_TIMING" },
    { 184, 1, F_RO, "STROBE_SUPPORT" },
    { 183, 1, F_EP_WO, "BUS_WIDTH" },
    { 181, 1, F_RO, "ERASED_MEM_CONT" },
    { 179, 1, F_NV, "PARTITION_CONFIG" }, /* PARTITION_ACCESS is E_P */
    { 178, 1, F_CP, "BOOT_CONFIG_PROT" }, /* PERM_BOOT_CONFIG_PROT is R/W */
    { 177, 1, F_NV, "BOOT_BUS_CONDITIONS" },
    { 175, 1, F_EP, "ERASE_GROUP_DEF" },
    { 174, 1, F_RO, "BOOT_WP_STATUS" },
    { 173, 1, F_OTP, "BOOT_WP" }, /* Mixed, has permanent bits */
    { 171, 1, F_OTP, "USER_WP" }, /* Mixed, has permanent bits */
    { 169, 1, F_OTP, "FW_CONFIG" },
    { 168, 1, F_RO, "RPMB_SIZE_MULT" },
    { 167, 1, F_OTP, "WR_REL_SET" },
    { 166, 1, F_RO, "WR_REL_PARAM" },
    { 165, 1, F_EP_WO, "SANITIZE_START" },
    { 164, 1, F_EP_WO, "BKOPS_START" },
    { 163, 1, F_OTP, "BKOPS_EN" }, /* MANUAL_EN is R/W */
    { 162, 1, F_OTP, "RST_n_FUNCTION" },
    { 161, 1, F_EP, "HPI_MGMT" },
    { 160, 1, F_RO, "PARTITIONING_SUPPORT" },
    { 157, 3, F_RO, "MAX_ENH_SIZE_MULT" },
    { 156, 1, F_OTP, "PARTITIONS_ATTRIBUTE" },
    { 155, 1, F_OTP, "PARTITION_SETTING_COMPLETED" },
    { 143, 12, F_OTP, "GP_SIZE_MULT" },
    { 140, 3, F_OTP, "ENH_SIZE_MULT" },
    { 136, 4, F_OTP, "ENH_START_ADDR" },
    { 134, 1, F_OTP, "SEC_BAD_BLK_MGMNT" },
    { 133, 1, F_NV, "PRODUCTION_STATE_AWARENESS" },
    { 132, 1, F_EP_WO, "TCASE_SUPPORT" },
    { 131, 1, F_NV, "PERIODIC_WAKEUP" },
    { 130, 1, F_RO, "PROGRAM_CID_CSD_DDR_SUPPORT" },
    { 63, 1, F_RO, "NATIVE_SECTOR_SIZE" },
    { 62, 1, F_OTP, "USE_NATIVE_SECTOR" },
    { 61, 1, F_RO, "DATA_SECTOR_SIZE" },
    { 60, 1, F_RO, "INI_TIMEOUT_EMU" },
    { 59, 1, F_EP, "CLASS_6_CTRL" },
    { 58, 1, F_RO, "DYNCAP_NEEDED" },
    { 56, 2, F_EP, "EXCEPTION_EVENTS_CTRL" },
    { 54, 2, F_RO, "EXCEPTION_EVENTS_STATUS" },
    { 52, 2, F_OTP, "EXT_PARTITIONS_ATTRIBUTE" },
    { 37, 15, F_EP, "CONTEXT_CONF" },
    { 36, 1, F_RO, "PACKED_COMMAND_STATUS" },
    { 35, 1, F_RO, "PACKED_FAILURE_INDEX" },
    { 34, 1, F_EP, "POWER_OFF_NOTIFICATION" },
    { 33, 1, F_EP, "CACHE_CTRL" },
    { 32, 1, F_EP_WO, "FLUSH_CACHE" },
    { 31, 1, F_OTP, "BARRIER_CTRL" },
    { 30, 1, F_EP, "MODE_CONFIG" },
    { 29, 1, F_EP_WO, "MODE_OPERATION_CODES" },
    { 26, 1, F_RO, "FFU_STATUS" },
    { 22, 4, F_EP, "PRE_LOADING_DATA_SIZE" },
    { 18, 4, F_RO, "MAX_PRE_LOADING_DATA_SIZE" },
    { 17, 1, F_NV, "PRODUCT_STATE_AWARENESS_ENABLEMENT" },
    { 16, 1, F_OTP, "SECURE_REMOVAL_TYPE" },
    { 15, 1, F_EP, "CMDQ_MODE_EN" },
};

/* Tuning block for an 8 bit bus, 6.6.5.1 Figure 32 */
static const uint8_t tuning_8bit[128] = {
    0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc, 0xcc,
    0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff, 0xff, 0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee, 0xff,
    0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd, 0xdd, 0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff, 0xbb,
    0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff, 0xff, 0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee, 0xff,
    0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc,
    0xcc, 0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff, 0xff, 0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee,
    0xff, 0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd, 0xdd, 0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff,
    0xbb, 0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff, 0xff, 0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee,
};

enum xfer_src {
    SRC_MEM,
    SRC_EXT_CSD,
    SRC_TUNING,
    SRC_RPMB,
};

struct xfer {
    bool active;
    bool read; /* Device to host */
    enum xfer_src src;
    enum emmc_part part;
    uint64_t lba;
    bool counted; /* CMD23, or a single block command */
    uint32_t left;
    size_t block_size;
    uint8_t cmd;
};

static struct emmc {
    struct bus_card bus;
    struct emmc_model_config cfg;
    bool powered;
    enum emmc_state state;
    uint16_t rca;
    bool csd_read; /* 6.6: the clock stays at fOD until the CSD is known */
    uint32_t status; /* Pending error bits */
    bool app_cmd;
    uint8_t ext_csd[512];
    uint32_t cid[4];
    uint32_t csd[4];
    struct bench_store store[EMMC_PART_COUNT];
    struct bus_cfg bus_cfg;
    uint64_t busy_until;
    bool timing_busy; /* The busy period of an HS_TIMING change */
    enum emmc_state after_busy;
    unsigned int op_cond_count;
    bool block_count_armed; /* CMD23 for the next command */
    uint16_t block_count;
    bool tuned; /* CMD21 seen since HS_TIMING was set to HS200 */
    struct xfer xfer;
    uint8_t rpmb_request[BLOCK_SIZE];
    bool rpmb_request_valid;
    /* Logged once per value, the clock is checked on every transaction */
    unsigned int bad_clk_hz;
    int bad_width;
    unsigned int cmd_count[64];
    unsigned int blocks_read;
    unsigned int blocks_written;
    /* Faults */
    int fail_cmd_idx;
    enum emmc_fault fail_cmd_kind;
    int fail_switch_index;
    enum bus_data_status fail_read;
    enum bus_data_status fail_write;
    int status_error_idx;
    uint32_t status_error_bits;
    int power_loss_blocks;
} emmc;

static const char *state_name(enum emmc_state s)
{
    static const char *names[] = { "idle", "ready", "ident", "stby", "tran", "data",
                                   "rcv",  "prg",   "dis",   "btst", "slp" };

    if (s == EMMC_INA)
        return "ina";
    return names[s];
}

static const struct field *field_of(unsigned int index)
{
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        if (index >= fields[i].index && index < (unsigned int)fields[i].index + fields[i].len)
            return &fields[i];
    }
    return NULL;
}

static uint8_t hs_timing(void)
{
    return emmc.ext_csd[EXT_CSD_HS_TIMING] & 0x0f;
}

static uint8_t bus_mode(void)
{
    return emmc.ext_csd[EXT_CSD_BUS_WIDTH] & BW_MODE_MASK;
}

static bool high_capacity(void)
{
    return emmc.cfg.user_sectors > HIGH_CAPACITY;
}

static uint64_t part_blocks(enum emmc_part part)
{
    switch (part) {
    case EMMC_PART_USER:
        return emmc.cfg.user_sectors;
    case EMMC_PART_BOOT1:
    case EMMC_PART_BOOT2:
        return (uint64_t)emmc.cfg.boot_size_mult * SZ_128K_BLOCKS;
    case EMMC_PART_RPMB:
        return (uint64_t)emmc.cfg.rpmb_size_mult * SZ_128K_BLOCKS;
    default:
        return 0;
    }
}

static enum emmc_part current_part(void)
{
    return emmc.ext_csd[EXT_CSD_PARTITION_CONFIG] & PC_ACCESS_MASK;
}

static void build_cid(void)
{
    static const char pnm[] = "PBMODL";

    memset(emmc.cid, 0, sizeof(emmc.cid));
    bench_set_bits(emmc.cid, 127, 120, 0x15); /* MID */
    bench_set_bits(emmc.cid, 113, 112, 1); /* CBX: BGA */
    bench_set_bits(emmc.cid, 111, 104, 0x42); /* OID */
    for (int i = 0; i < 6; i++)
        bench_set_bits(emmc.cid, 103 - 8 * i, 96 - 8 * i, (uint8_t)pnm[i]);
    bench_set_bits(emmc.cid, 55, 48, 0x10); /* PRV */
    bench_set_bits(emmc.cid, 47, 16, 0x12345678); /* PSN */
    bench_set_bits(emmc.cid, 15, 8, 0xa1); /* MDT */
    bench_reg128_seal(emmc.cid);
}

/* 7.3, Table 78 */
static void build_csd(void)
{
    memset(emmc.csd, 0, sizeof(emmc.csd));
    bench_set_bits(emmc.csd, 127, 126, 3); /* CSD_STRUCTURE: see EXT_CSD */
    bench_set_bits(emmc.csd, 125, 122, 4); /* SPEC_VERS: 4.x and later */
    bench_set_bits(emmc.csd, 119, 112, 0x27); /* TAAC */
    bench_set_bits(emmc.csd, 111, 104, 0x01); /* NSAC */
    bench_set_bits(emmc.csd, 103, 96, 0x32); /* TRAN_SPEED: 26 MHz */
    bench_set_bits(emmc.csd, 95, 84, 0x8f5); /* CCC */
    bench_set_bits(emmc.csd, 83, 80, 9); /* READ_BL_LEN: 512 */
    if (high_capacity()) {
        bench_set_bits(emmc.csd, 73, 62, 0xfff); /* C_SIZE: see SEC_COUNT */
        bench_set_bits(emmc.csd, 49, 47, 7);
    } else {
        /* capacity = (C_SIZE + 1) * 2^(C_SIZE_MULT + 2) * 512 */
        uint64_t c_size = emmc.cfg.user_sectors / (1u << 9) - 1;

        bench_set_bits(emmc.csd, 73, 62, (uint32_t)c_size);
        bench_set_bits(emmc.csd, 49, 47, 7);
    }
    bench_set_bits(emmc.csd, 46, 42, 0x1f); /* ERASE_GRP_SIZE */
    bench_set_bits(emmc.csd, 41, 37, 0x1f); /* ERASE_GRP_MULT */
    bench_set_bits(emmc.csd, 36, 32, 0x0f); /* WP_GRP_SIZE */
    bench_set_bits(emmc.csd, 28, 26, 2); /* R2W_FACTOR */
    bench_set_bits(emmc.csd, 25, 22, 9); /* WRITE_BL_LEN: 512 */
    bench_reg128_seal(emmc.csd);
}

static void build_ext_csd(void)
{
    uint8_t *x = emmc.ext_csd;
    uint64_t sec = emmc.cfg.user_sectors;

    memset(x, 0, sizeof(emmc.ext_csd));
    x[EXT_CSD_S_CMD_SET] = 0x01;
    x[EXT_CSD_HPI_FEATURES] = 0x01;
    x[EXT_CSD_BKOPS_SUPPORT] = 0x01;
    x[EXT_CSD_LIFE_TIME_B] = 0x01;
    x[EXT_CSD_LIFE_TIME_A] = 0x01;
    x[EXT_CSD_PRE_EOL_INFO] = 0x01;
    x[EXT_CSD_GENERIC_CMD6_TIME] = emmc.cfg.generic_cmd6_time;
    x[EXT_CSD_POWER_OFF_LONG_TIME] = emmc.cfg.power_off_long_time;
    x[EXT_CSD_BOOT_INFO] = 0x07;
    x[EXT_CSD_BOOT_SIZE_MULT] = emmc.cfg.boot_size_mult;
    x[EXT_CSD_HC_ERASE_GRP_SIZE] = 0x01;
    x[EXT_CSD_ERASE_TIMEOUT_MULT] = 0x01;
    x[EXT_CSD_REL_WR_SEC_C] = 0x01;
    x[EXT_CSD_HC_WP_GRP_SIZE] = 0x01;
    x[EXT_CSD_S_A_TIMEOUT] = 0x11;
    x[EXT_CSD_SEC_COUNT + 0] = sec & 0xff;
    x[EXT_CSD_SEC_COUNT + 1] = (sec >> 8) & 0xff;
    x[EXT_CSD_SEC_COUNT + 2] = (sec >> 16) & 0xff;
    x[EXT_CSD_SEC_COUNT + 3] = (sec >> 24) & 0xff;
    x[EXT_CSD_PARTITION_SWITCH_TIME] = emmc.cfg.partition_switch_time;
    x[EXT_CSD_OUT_OF_INTERRUPT_TIME] = 0x05;
    x[EXT_CSD_DRIVER_STRENGTH] = emmc.cfg.driver_strength;
    x[EXT_CSD_DEVICE_TYPE] = emmc.cfg.device_type;
    x[EXT_CSD_CSD_STRUCTURE] = 0x02;
    x[EXT_CSD_REV] = 0x08; /* 5.1 */
    x[EXT_CSD_STROBE_SUPPORT] = emmc.cfg.strobe_support;
    x[EXT_CSD_PARTITION_CONFIG] = emmc.cfg.partition_config & ~PC_ACCESS_MASK;
    x[EXT_CSD_BOOT_CONFIG_PROT] = emmc.cfg.boot_config_prot;
    x[EXT_CSD_BOOT_BUS_CONDITIONS] = emmc.cfg.boot_bus_conditions;
    x[EXT_CSD_RPMB_SIZE_MULT] = emmc.cfg.rpmb_size_mult;
    x[EXT_CSD_WR_REL_PARAM] = 0x15;
    x[EXT_CSD_PARTITIONING_SUPPORT] = 0x07;
}

/* What CMD0 and power on clear: the E_P fields and PARTITION_ACCESS */
static void reset_modes(bool power)
{
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        enum field_type t = fields[i].type;

        if (t == F_EP || t == F_EP_WO || (power && t == F_CP))
            memset(&emmc.ext_csd[fields[i].index], 0, fields[i].len);
    }
    if (power)
        emmc.ext_csd[EXT_CSD_BOOT_CONFIG_PROT] &= ~BCP_PWR;
    emmc.ext_csd[EXT_CSD_PARTITION_CONFIG] &= ~PC_ACCESS_MASK;
}

static void go_idle(void)
{
    emmc.state = EMMC_IDLE;
    emmc.rca = 1; /* 6.4.1 */
    emmc.csd_read = false;
    emmc.app_cmd = false;
    emmc.busy_until = 0;
    emmc.timing_busy = false;
    emmc.op_cond_count = 0;
    emmc.block_count_armed = false;
    emmc.tuned = false;
    emmc.xfer.active = false;
    emmc.rpmb_request_valid = false;
    reset_modes(false);
}

static uint64_t now(void)
{
    return bench_now_us();
}

static bool is_busy(void)
{
    return now() < emmc.busy_until;
}

/* Completes programming once the busy period is over */
static void progress(void)
{
    if (is_busy())
        return;

    emmc.timing_busy = false;
    if (emmc.state == EMMC_PRG)
        emmc.state = emmc.after_busy;
    else if (emmc.state == EMMC_DIS)
        emmc.state = EMMC_STBY;
}

static void set_busy(unsigned int us, enum emmc_state after)
{
    emmc.busy_until = now() + us;
    emmc.after_busy = after;
}

/* 6.6, 6.6.2, 10.6, 10.8: the clock limit of the current timing mode */
static unsigned int max_clock_hz(void)
{
    if (emmc.state == EMMC_IDLE || emmc.state == EMMC_READY || emmc.state == EMMC_IDENT ||
        !emmc.csd_read)
        return FOD_MAX_HZ;

    switch (hs_timing()) {
    case TIMING_HS:
        return F_HS_MAX_HZ;
    case TIMING_HS200:
    case TIMING_HS400:
        return F_HS200_MAX_HZ;
    default:
        return F_COMPAT_MAX_HZ;
    }
}

static void check_clock(const char *what)
{
    unsigned int max = max_clock_hz();
    unsigned int clk = emmc.bus_cfg.clk_hz;

    if (clk <= max) {
        emmc.bad_clk_hz = 0;
        return;
    }
    if (clk == emmc.bad_clk_hz)
        return;

    emmc.bad_clk_hz = clk;
    if (max == FOD_MAX_HZ) {
        bench_violation(JESD84 "6.6",
                        "%s at %u Hz in %s%s, the identification clock is at most 400 kHz",
                        what,
                        clk,
                        state_name(emmc.state),
                        emmc.csd_read ? "" : " before the CSD was read");
    } else {
        bench_violation(JESD84 "6.6.2",
                        "%s at %u Hz with HS_TIMING %u, the limit is %u Hz",
                        what,
                        clk,
                        hs_timing(),
                        max);
    }
}

/* The device's bus width and data rate, from BUS_WIDTH */
static void device_bus(unsigned int *width, bool *ddr)
{
    switch (bus_mode()) {
    case 1:
        *width = 4;
        *ddr = false;
        break;
    case 2:
        *width = 8;
        *ddr = false;
        break;
    case 5:
        *width = 4;
        *ddr = true;
        break;
    case 6:
        *width = 8;
        *ddr = true;
        break;
    default:
        *width = 1;
        *ddr = false;
        break;
    }
}

/* Each data block: the host must drive the bus as the device is set up */
static bool check_data_bus(void)
{
    unsigned int width;
    bool ddr;
    bool ok = true;

    check_clock("Data block");
    device_bus(&width, &ddr);

    if (emmc.bus_cfg.width != width || emmc.bus_cfg.ddr != ddr) {
        int key = (int)(emmc.bus_cfg.width * 2 + emmc.bus_cfg.ddr) * 100 + (int)(width * 2 + ddr);

        if (key != emmc.bad_width) {
            emmc.bad_width = key;
            bench_violation(JESD84 "6.6.6",
                            "Data on a %u bit %s bus, the device uses %u bit %s (BUS_WIDTH %u)",
                            emmc.bus_cfg.width,
                            emmc.bus_cfg.ddr ? "DDR" : "SDR",
                            width,
                            ddr ? "DDR" : "SDR",
                            bus_mode());
        }
        ok = false;
    } else {
        emmc.bad_width = 0;
    }

    if (hs_timing() == TIMING_HS200 && (width == 1 || ddr)) {
        bench_violation(JESD84 "6.6.2.2", "Data in HS200 needs a 4 or 8 bit SDR bus");
    } else if (hs_timing() == TIMING_HS400 && !(width == 8 && ddr)) {
        bench_violation(JESD84 "6.6.2.3", "Data in HS400 needs an 8 bit DDR bus");
    }

    return ok;
}

/* The response type of each command, Table 49 - 59 */
static enum bus_rsp_type rsp_type(uint8_t idx)
{
    switch (idx) {
    case 0:
    case 4:
    case 15:
        return BUS_RSP_NONE;
    case 2:
    case 9:
    case 10:
        return BUS_RSP_136;
    case 5:
    case 6:
    case 28:
    case 29:
    case 38:
        return BUS_RSP_48_BUSY;
    case 7:
        /* R1b from Disconnect, R1 otherwise */
        return emmc.state == EMMC_DIS ? BUS_RSP_48_BUSY : BUS_RSP_48;
    case 12:
        /* R1 for reads, R1b for writes */
        return emmc.state == EMMC_RCV ? BUS_RSP_48_BUSY : BUS_RSP_48;
    default:
        return BUS_RSP_48;
    }
}

static const char *rsp_name(enum bus_rsp_type t)
{
    static const char *names[] = { "none", "48 bit", "48 bit with busy", "136 bit" };
    return names[t];
}

static void check_rsp_type(const struct bus_cmd *cmd)
{
    enum bus_rsp_type want = rsp_type(cmd->idx);

    /* Waiting for busy after a plain R1 is harmless, busy just isn't there */
    if (cmd->rsp == want || (want == BUS_RSP_48 && cmd->rsp == BUS_RSP_48_BUSY))
        return;

    bench_violation(JESD84 "6.10.4",
                    "CMD%u has a %s response, the host expects %s",
                    cmd->idx,
                    rsp_name(want),
                    rsp_name(cmd->rsp));
}

/* Data address to block number, 6.4.2 and Table 50 note 1 */
static bool block_address(uint32_t arg, uint64_t *lba, uint32_t *err)
{
    if (high_capacity()) {
        *lba = arg;
        return true;
    }
    if (arg % BLOCK_SIZE) {
        *err |= ST_ADDRESS_MISALIGN;
        return false;
    }
    *lba = arg / BLOCK_SIZE;
    return true;
}

static int switch_error(const char *section, const char *fmt, unsigned int index, unsigned int value)
{
    if (section != NULL) {
        char text[160];

        snprintf(text, sizeof(text), fmt, index, value);
        bench_violation(section, "CMD6 %s", text);
    }
    return -1;
}

/* HS_TIMING, 6.6.2 and 7.4.65 */
static int write_hs_timing(uint8_t value)
{
    uint8_t timing = value & 0x0f;
    uint8_t strength = value >> 4;
    uint8_t dt = emmc.cfg.device_type;
    bool v18 = emmc.cfg.vccq_mv <= 1950;
    bool v12 = emmc.cfg.vccq_mv <= 1300;

    if (timing > TIMING_HS400)
        return switch_error(JESD84 "7.4.65", "HS_TIMING [%u] = 0x%02x, invalid timing", 185, value);

    if (strength != 0 && !(emmc.cfg.driver_strength & (1u << strength)))
        return switch_error(
            JESD84 "6.6.2.2", "HS_TIMING [%u] = 0x%02x, driver strength not supported", 185, value);

    switch (timing) {
    case TIMING_HS:
        if (!(dt & (DT_HS26 | DT_HS52)))
            return switch_error(
                JESD84 "6.6.2.1", "HS_TIMING [%u] = 0x%02x, no HS support", 185, value);
        break;
    case TIMING_HS200:
        if (!((v18 && (dt & DT_HS200_18)) || (v12 && (dt & DT_HS200_12))))
            return switch_error(JESD84 "6.6.2.2",
                                "HS_TIMING [%u] = 0x%02x, DEVICE_TYPE has no HS200 at this VCCQ",
                                185,
                                value);
        if (bus_mode() == 5 || bus_mode() == 6)
            bench_violation(JESD84 "6.6.2.2", "HS200 selected with a DDR bus width");
        emmc.tuned = false;
        break;
    case TIMING_HS400:
        if (!((v18 && (dt & DT_HS400_18)) || (v12 && (dt & DT_HS400_12))))
            return switch_error(JESD84 "6.6.2.3",
                                "HS_TIMING [%u] = 0x%02x, DEVICE_TYPE has no HS400 at this VCCQ",
                                185,
                                value);
        if (bus_mode() != 6)
            return switch_error(JESD84 "6.6.2.3",
                                "HS_TIMING [%u] = 0x%02x, BUS_WIDTH is not 8 bit DDR",
                                185,
                                value);
        if (hs_timing() != TIMING_HS)
            bench_violation(
                JESD84 "6.6.2.3", "HS400 selected from HS_TIMING %u, not from HS", hs_timing());
        if (!(emmc.ext_csd[EXT_CSD_BUS_WIDTH] & BW_STROBE) && !emmc.tuned)
            bench_violation(JESD84 "6.6.2.3",
                            "HS400 selected without tuning in HS200 first (or Enhanced Strobe)");
        break;
    default:
        break;
    }

    emmc.ext_csd[EXT_CSD_HS_TIMING] = value;
    emmc.timing_busy = true;
    return 0;
}

/* BUS_WIDTH, 6.6.6, 6.6.23 and 7.4.67 */
static int write_bus_width(uint8_t value)
{
    uint8_t mode = value & BW_MODE_MASK;
    uint8_t dt = emmc.cfg.device_type;

    if ((value & BW_RESERVED) || !(mode <= 2 || mode == 5 || mode == 6))
        return switch_error(JESD84 "7.4.67", "BUS_WIDTH [%u] = 0x%02x, invalid", 183, value);

    if (mode == 5 || mode == 6) {
        bool ddr52 = dt & (DT_DDR52_18_3V | DT_DDR52_12);
        bool hs400 = mode == 6 && (dt & (DT_HS400_18 | DT_HS400_12));

        if (!ddr52 && !hs400)
            return switch_error(
                JESD84 "6.6.23", "BUS_WIDTH [%u] = 0x%02x, DEVICE_TYPE has no DDR", 183, value);
        if (hs_timing() != TIMING_HS)
            return switch_error(JESD84 "7.4.67",
                                "BUS_WIDTH [%u] = 0x%02x, DDR needs HS_TIMING 1 first",
                                183,
                                value);
    }

    if ((value & BW_STROBE) && (mode != 6 || !emmc.cfg.strobe_support))
        return switch_error(
            JESD84 "7.4.67", "BUS_WIDTH [%u] = 0x%02x, Enhanced Strobe not supported", 183, value);

    emmc.ext_csd[EXT_CSD_BUS_WIDTH] = value;
    return 0;
}

/* PARTITION_CONFIG, 6.2.5 and 7.4.69 */
static int write_partition_config(uint8_t value)
{
    uint8_t old = emmc.ext_csd[EXT_CSD_PARTITION_CONFIG];
    uint8_t access = value & PC_ACCESS_MASK;
    uint8_t boot = (value & PC_BOOT_MASK) >> PC_BOOT_SHIFT;

    if (value & PC_RESERVED)
        return switch_error(
            JESD84 "7.4.69", "PARTITION_CONFIG [%u] = 0x%02x, reserved bit", 179, value);
    if (access >= EMMC_PART_COUNT || part_blocks(access) == 0) {
        /* 6.2.5: not created, the device sets SWITCH_ERROR */
        return switch_error(
            JESD84 "6.2.5", "PARTITION_CONFIG [%u] = 0x%02x, no such partition", 179, value);
    }
    if (!(boot <= 2 || boot == 7))
        return switch_error(
            JESD84 "7.4.69", "PARTITION_CONFIG [%u] = 0x%02x, reserved boot partition", 179, value);
    if (((old ^ value) & (PC_BOOT_MASK | PC_BOOT_ACK)) &&
        (emmc.ext_csd[EXT_CSD_BOOT_CONFIG_PROT] & (BCP_PWR | BCP_PERM))) {
        /* 7.4.70, the device refuses, the host has to cope */
        return switch_error(NULL, NULL, 0, 0);
    }

    emmc.ext_csd[EXT_CSD_PARTITION_CONFIG] = value;
    if (access != EMMC_PART_RPMB)
        emmc.rpmb_request_valid = false;
    return 0;
}

/* POWER_OFF_NOTIFICATION, 6.6.36 and 7.4.110 */
static int write_power_off_notification(uint8_t value)
{
    uint8_t old = emmc.ext_csd[EXT_CSD_POWER_OFF_NOTIFICATION];

    if (value > PON_SLEEP)
        return switch_error(
            JESD84 "7.4.110", "POWER_OFF_NOTIFICATION [%u] = 0x%02x, invalid", 34, value);
    if (value == PON_NONE && old != PON_NONE)
        return switch_error(JESD84 "6.6.36",
                            "POWER_OFF_NOTIFICATION [%u] = 0x%02x after it was enabled",
                            34,
                            value);
    if (value >= PON_OFF_SHORT && old == PON_NONE)
        bench_violation(JESD84 "6.6.36", "Power off notification without POWERED_ON first");

    emmc.ext_csd[EXT_CSD_POWER_OFF_NOTIFICATION] = value;
    return 0;
}

/* CMD6, 6.6.1 */
static void cmd_switch(uint32_t arg)
{
    unsigned int access = SWITCH_ACCESS(arg);
    unsigned int index = SWITCH_INDEX(arg);
    uint8_t value = SWITCH_VALUE(arg);
    uint8_t old;
    const struct field *f;
    int rc = 0;

    if (arg & SWITCH_MBZ)
        bench_violation(JESD84 "6.10.4", "CMD6 argument 0x%08x has bits set that must be 0", arg);

    set_busy(emmc.cfg.switch_busy_us, EMMC_TRAN);

    if (access == ACCESS_CMD_SET)
        return;

    if (index >= 192) {
        bench_violation(JESD84 "6.6.1", "CMD6 to EXT_CSD [%u], only 0-191 can be written", index);
        emmc.status |= ST_SWITCH_ERROR;
        return;
    }

    old = emmc.ext_csd[index];
    if (access == ACCESS_SET_BITS)
        value = old | value;
    else if (access == ACCESS_CLEAR_BITS)
        value = old & ~value;

    if ((int)index == emmc.fail_switch_index) {
        emmc.fail_switch_index = -1;
        emmc.status |= ST_SWITCH_ERROR;
        return;
    }

    f = field_of(index);
    if (f == NULL || f->type == F_RO) {
        bench_violation(JESD84 "7.4",
                        "CMD6 to EXT_CSD [%u]%s%s, which is read only or reserved",
                        index,
                        f ? " " : "",
                        f ? f->name : "");
        emmc.status |= ST_SWITCH_ERROR;
        return;
    }
    if (f->type == F_OTP && !emmc.cfg.allow_otp_writes) {
        bench_violation(JESD84 "7.4",
                        "CMD6 writes %s [%u] = 0x%02x, a one-time programmable field",
                        f->name,
                        index,
                        value);
    }
    if (index == EXT_CSD_BOOT_CONFIG_PROT && (value & BCP_PERM) && !emmc.cfg.allow_otp_writes) {
        bench_violation(JESD84 "7.4.70", "CMD6 sets PERM_BOOT_CONFIG_PROT, a permanent setting");
    }
    if (index == EXT_CSD_BOOT_BUS_CONDITIONS &&
        (emmc.ext_csd[EXT_CSD_BOOT_CONFIG_PROT] & (BCP_PWR | BCP_PERM))) {
        emmc.status |= ST_SWITCH_ERROR; /* 7.4.70 */
        return;
    }

    switch (index) {
    case EXT_CSD_HS_TIMING:
        rc = write_hs_timing(value);
        break;
    case EXT_CSD_BUS_WIDTH:
        rc = write_bus_width(value);
        break;
    case EXT_CSD_PARTITION_CONFIG:
        rc = write_partition_config(value);
        break;
    case EXT_CSD_POWER_OFF_NOTIFICATION:
        rc = write_power_off_notification(value);
        break;
    default:
        emmc.ext_csd[index] = value;
        break;
    }

    if (rc != 0)
        emmc.status |= ST_SWITCH_ERROR;
}

static void start_xfer(uint8_t idx, bool read, enum xfer_src src, uint64_t lba, size_t block_size)
{
    emmc.xfer.active = true;
    emmc.xfer.read = read;
    emmc.xfer.src = src;
    emmc.xfer.part = current_part();
    emmc.xfer.lba = lba;
    emmc.xfer.block_size = block_size;
    emmc.xfer.cmd = idx;

    if (idx == 18 || idx == 25) {
        emmc.xfer.counted = emmc.block_count_armed && emmc.block_count != 0;
        emmc.xfer.left = emmc.block_count;
    } else {
        emmc.xfer.counted = true;
        emmc.xfer.left = 1;
    }
    emmc.state = read ? EMMC_DATA : EMMC_RCV;
}

/* CMD17, 18, 24, 25 */
static bool cmd_rw(uint8_t idx, uint32_t arg, uint32_t *err)
{
    bool read = idx == 17 || idx == 18;
    bool multiple = idx == 18 || idx == 25;
    enum emmc_part part = current_part();
    uint64_t lba;

    if (part == EMMC_PART_RPMB) {
        if (!multiple) {
            bench_violation(JESD84 "6.2.2", "CMD%u in the RPMB partition", idx);
            return false;
        }
        if (!emmc.block_count_armed)
            bench_violation(JESD84 "6.6.22", "CMD%u to RPMB without CMD23 first", idx);
        start_xfer(idx, read, SRC_RPMB, 0, BLOCK_SIZE);
        return true;
    }

    if (!block_address(arg, &lba, err))
        return true;
    if (lba >= part_blocks(part)) {
        *err |= ST_ADDRESS_OUT_OF_RANGE;
        return true;
    }

    start_xfer(idx, read, SRC_MEM, lba, BLOCK_SIZE);
    return true;
}

static void rpmb_response(uint8_t *frame)
{
    memset(frame, 0, BLOCK_SIZE);
    if (emmc.rpmb_request_valid) {
        uint16_t req = (emmc.rpmb_request[RPMB_FRAME_REQ_RESP] << 8) |
                       emmc.rpmb_request[RPMB_FRAME_REQ_RESP + 1];
        uint16_t resp = req << 8;

        frame[RPMB_FRAME_REQ_RESP] = resp >> 8;
        frame[RPMB_FRAME_REQ_RESP + 1] = resp & 0xff;
    }
    /* 6.6.22.1: the only result until the key is programmed */
    frame[RPMB_FRAME_RESULT] = RPMB_RESULT_NO_KEY >> 8;
    frame[RPMB_FRAME_RESULT + 1] = RPMB_RESULT_NO_KEY & 0xff;
}

/*
 * Runs a command that is legal in the current state. Returns false if it
 * is illegal after all, *err gets the response mode errors.
 */
static bool execute(const struct bus_cmd *cmd, struct bus_rsp *rsp, uint32_t *err)
{
    uint32_t arg = cmd->arg;
    uint16_t rca = arg >> 16;
    bool addressed = rca == emmc.rca;
    enum emmc_state s = emmc.state;
    bool app = emmc.app_cmd;
    bool armed = emmc.block_count_armed;

    emmc.app_cmd = false;
    /* CMD23 counts only for the command right after it, 6.6.7.1 */
    emmc.block_count_armed = false;

    if (app && cmd->idx != 13)
        return false; /* No application commands on e.MMC */

    switch (cmd->idx) {
    case 0:
        go_idle();
        rsp->timeout = true;
        return true;
    case 1:
        if (s != EMMC_IDLE)
            return false;
        if (arg != 0 && !(arg & OCR_VOLTAGES)) {
            emmc.state = EMMC_INA;
            rsp->timeout = true;
            return true;
        }
        if (arg != 0 && high_capacity() && (arg & OCR_ACCESS_MASK) != OCR_SECTOR) {
            /* 6.4.2: the host can't address this device */
            bench_violation(JESD84 "6.4.2",
                            "CMD1 argument 0x%08x lacks sector mode, the device goes Inactive",
                            arg);
            emmc.state = EMMC_INA;
            rsp->timeout = true;
            return true;
        }
        rsp->r[0] = OCR_VOLTAGES | (high_capacity() ? OCR_SECTOR : 0);
        if (arg != 0 && ++emmc.op_cond_count > emmc.cfg.op_cond_busy) {
            rsp->r[0] |= OCR_BUSY_N;
            emmc.state = EMMC_READY;
        }
        rsp->idx = 0x3f;
        return true;
    case 2:
        if (s != EMMC_READY)
            return false;
        memcpy(rsp->r, emmc.cid, sizeof(rsp->r));
        rsp->idx = 0x3f;
        emmc.state = EMMC_IDENT;
        return true;
    case 3:
        if (s != EMMC_IDENT)
            return false;
        if (rca == 0)
            bench_violation(JESD84 "6.10.4", "CMD3 assigns RCA 0, which deselects");
        emmc.rca = rca;
        emmc.state = EMMC_STBY;
        return true;
    case 4:
        if (s != EMMC_STBY)
            return false;
        rsp->timeout = true;
        return true;
    case 6:
        if (s != EMMC_TRAN)
            return false;
        emmc.state = EMMC_PRG;
        cmd_switch(arg);
        return true;
    case 7:
        if (s == EMMC_STBY) {
            if (!addressed) {
                rsp->timeout = true;
                return true;
            }
            emmc.state = EMMC_TRAN;
            return true;
        }
        if (s == EMMC_DIS) {
            if (!addressed) {
                rsp->timeout = true;
                return true;
            }
            emmc.state = is_busy() ? EMMC_PRG : EMMC_TRAN;
            emmc.after_busy = EMMC_TRAN;
            return true;
        }
        if (s == EMMC_TRAN || s == EMMC_DATA || s == EMMC_PRG) {
            if (addressed)
                return false;
            emmc.xfer.active = false;
            emmc.state = s == EMMC_PRG ? EMMC_DIS : EMMC_STBY;
            if (s == EMMC_PRG)
                emmc.after_busy = EMMC_STBY;
            /* Deselected, no response */
            rsp->timeout = true;
            return true;
        }
        return false;
    case 8:
        if (s != EMMC_TRAN)
            return false;
        start_xfer(8, true, SRC_EXT_CSD, 0, BLOCK_SIZE);
        return true;
    case 9:
    case 10:
        if (s != EMMC_STBY)
            return false;
        if (!addressed) {
            rsp->timeout = true;
            return true;
        }
        memcpy(rsp->r, cmd->idx == 9 ? emmc.csd : emmc.cid, sizeof(rsp->r));
        rsp->idx = 0x3f;
        if (cmd->idx == 9)
            emmc.csd_read = true;
        return true;
    case 12:
        if (s == EMMC_DATA) {
            emmc.xfer.active = false;
            emmc.state = EMMC_TRAN;
            return true;
        }
        if (s == EMMC_RCV) {
            emmc.xfer.active = false;
            emmc.state = EMMC_PRG;
            set_busy(emmc.cfg.write_busy_us, EMMC_TRAN);
            return true;
        }
        if (s == EMMC_PRG && (arg & 1)) {
            /* HPI */
            emmc.busy_until = now();
            emmc.state = EMMC_TRAN;
            return true;
        }
        if (s == EMMC_TRAN)
            bench_violation(JESD84 "6.6.7.1",
                            "CMD12 in tran: a transfer with a CMD23 block count ends by itself");
        return false;
    case 13:
        if (s == EMMC_IDLE || s == EMMC_READY || s == EMMC_IDENT || s == EMMC_SLP)
            return false;
        if (!addressed) {
            rsp->timeout = true;
            return true;
        }
        if (emmc.timing_busy && is_busy() && emmc.cfg.unreliable_cmd13_in_timing_switch)
            rsp->crc_error = true;
        return true;
    case 15:
        if (s == EMMC_IDLE || s == EMMC_READY || s == EMMC_IDENT || s == EMMC_SLP)
            return false;
        if (addressed)
            emmc.state = EMMC_INA;
        rsp->timeout = true;
        return true;
    case 16:
        if (s != EMMC_TRAN)
            return false;
        if (bus_mode() == 5 || bus_mode() == 6) {
            bench_violation(JESD84 "6.6.24", "CMD16 in dual data rate mode");
            return false;
        }
        if (arg != BLOCK_SIZE)
            *err |= ST_BLOCK_LEN_ERROR;
        return true;
    case 17:
    case 18:
    case 24:
    case 25:
        if (s != EMMC_TRAN)
            return false;
        emmc.block_count_armed = armed;
        if (!cmd_rw(cmd->idx, arg, err))
            return false;
        emmc.block_count_armed = false;
        return true;
    case 21:
        if (s != EMMC_TRAN)
            return false;
        if (hs_timing() != TIMING_HS200) {
            bench_violation(JESD84 "6.6.5.1",
                            "CMD21 with HS_TIMING %u, it is valid only in HS200",
                            hs_timing());
            return false;
        }
        if (bus_mode() == 0)
            bench_violation(JESD84 "6.6.5.1", "CMD21 on a 1 bit bus");
        emmc.tuned = true;
        start_xfer(21, true, SRC_TUNING, 0, bus_mode() == 2 ? 128 : 64);
        return true;
    case 23:
        if (s != EMMC_TRAN)
            return false;
        emmc.block_count = arg & 0xffff;
        emmc.block_count_armed = true;
        return true;
    case 55:
        if (s == EMMC_IDLE || s == EMMC_READY || s == EMMC_IDENT || s == EMMC_SLP)
            return false;
        if (!addressed) {
            rsp->timeout = true;
            return true;
        }
        emmc.app_cmd = true;
        return true;
    default:
        /* Not supported by the model, or reserved */
        return false;
    }
}

static void card_cmd(struct bus_card *card, const struct bus_cmd *cmd, struct bus_rsp *rsp)
{
    enum emmc_state before;
    uint32_t err = 0;
    uint32_t pending;
    (void)card;

    memset(rsp, 0, sizeof(*rsp));
    rsp->idx = cmd->idx;

    if (!emmc.powered) {
        rsp->timeout = true;
        return;
    }

    progress();
    emmc.cmd_count[cmd->idx & 63]++;

    if (emmc.state == EMMC_INA) {
        rsp->timeout = true;
        return;
    }

    if ((int)cmd->idx == emmc.fail_cmd_idx) {
        emmc.fail_cmd_idx = -1;
        if (emmc.fail_cmd_kind == EMMC_FAULT_NO_RESPONSE) {
            /* As if the command CRC failed, 6.8.1 */
            emmc.status |= ST_COM_CRC_ERROR;
            rsp->timeout = true;
            return;
        }
        rsp->crc_error = true;
    }

    check_clock("Command");

    /* 6.6.36: any command after a power off notification restores it */
    if (cmd->idx != 0 && !is_busy() &&
        emmc.ext_csd[EXT_CSD_POWER_OFF_NOTIFICATION] >= PON_OFF_SHORT &&
        emmc.ext_csd[EXT_CSD_POWER_OFF_NOTIFICATION] <= PON_OFF_LONG)
        emmc.ext_csd[EXT_CSD_POWER_OFF_NOTIFICATION] = PON_POWERED_ON;

    before = emmc.state;
    /* Errors of this command's execution go out with the next response */
    pending = emmc.status;

    /* 6.6: in prg only status, deselect, stop (HPI) and resets are accepted */
    if (before == EMMC_PRG && is_busy() && cmd->idx != 0 && cmd->idx != 7 && cmd->idx != 12 &&
        cmd->idx != 13 && cmd->idx != 15) {
        bench_violation(JESD84 "6.6", "CMD%u while the device is busy programming", cmd->idx);
        emmc.status |= ST_ILLEGAL_COMMAND;
        rsp->timeout = true;
        return;
    }

    check_rsp_type(cmd);

    if (!execute(cmd, rsp, &err)) {
        /* 6.8.1: no response, no state change, reported with the next one */
        if (before != EMMC_IDLE) {
            bench_violation(JESD84 "6.11",
                            "CMD%u (0x%08x) is illegal in %s",
                            cmd->idx,
                            cmd->arg,
                            state_name(before));
        }
        emmc.status |= ST_ILLEGAL_COMMAND;
        emmc.state = before;
        rsp->timeout = true;
        return;
    }

    if (rsp->timeout || rsp->idx == 0x3f)
        return;

    /* R1: the state when the command was received, 6.13 */
    rsp->r[0] = pending | err | ((uint32_t)before << ST_STATE_SHIFT) |
                (before == EMMC_PRG && is_busy() ? 0 : ST_READY_FOR_DATA) |
                (emmc.app_cmd ? ST_APP_CMD : 0);
    emmc.status &= ~(pending & ST_ERROR_BITS);

    /* Execution mode errors for the test, reported with the next response */
    if ((int)cmd->idx == emmc.status_error_idx) {
        emmc.status_error_idx = -1;
        emmc.status |= emmc.status_error_bits;
    }
}

static enum bus_data_status card_read(struct bus_card *card, void *buf, size_t len)
{
    struct xfer *x = &emmc.xfer;
    enum bus_data_status st = BUS_DATA_OK;
    (void)card;

    if (!emmc.powered)
        return BUS_DATA_TIMEOUT;
    progress();
    if (emmc.state != EMMC_DATA || !x->active || !x->read)
        return BUS_DATA_TIMEOUT;

    if (!check_data_bus())
        st = BUS_DATA_CRC;

    if (len != x->block_size) {
        bench_violation(JESD84 "6.6.7.1",
                        "CMD%u: the host reads %zu byte blocks, the device sends %zu",
                        x->cmd,
                        len,
                        x->block_size);
        st = BUS_DATA_CRC;
    }

    switch (x->src) {
    case SRC_MEM:
        if (x->lba >= part_blocks(x->part)) {
            /* Open-ended past the end, the device stops sending, 6.6.7.1 */
            emmc.status |= ST_ADDRESS_OUT_OF_RANGE;
            return BUS_DATA_TIMEOUT;
        }
        bench_store_read(&emmc.store[x->part], x->lba, buf);
        break;
    case SRC_EXT_CSD: {
        uint8_t *out = buf;

        memcpy(out, emmc.ext_csd, sizeof(emmc.ext_csd));
        /* 6.6.1: write only fields read as 0 */
        for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
            if (fields[i].type == F_EP_WO)
                memset(&out[fields[i].index], 0, fields[i].len);
        }
        break;
    }
    case SRC_TUNING:
        if (x->block_size == 128) {
            memcpy(buf, tuning_8bit, 128);
        } else {
            /* DAT[3:0] of the 8 bit pattern, 6.6.5.1 */
            uint8_t *out = buf;

            for (int i = 0; i < 64; i++)
                out[i] = (tuning_8bit[2 * i] & 0x0f) << 4 | (tuning_8bit[2 * i + 1] & 0x0f);
        }
        break;
    case SRC_RPMB:
        rpmb_response(buf);
        break;
    }

    if (emmc.fail_read != BUS_DATA_OK) {
        st = emmc.fail_read;
        emmc.fail_read = BUS_DATA_OK;
    }
    if (st != BUS_DATA_OK)
        memset(buf, 0xa5, len);

    emmc.blocks_read++;
    x->lba++;
    if (x->counted && --x->left == 0) {
        x->active = false;
        emmc.state = EMMC_TRAN;
    }
    return st;
}

static enum bus_data_status card_write(struct bus_card *card, const void *buf, size_t len)
{
    struct xfer *x = &emmc.xfer;
    (void)card;

    if (!emmc.powered)
        return BUS_DATA_TIMEOUT;
    progress();
    if (emmc.state != EMMC_RCV || !x->active || x->read)
        return BUS_DATA_TIMEOUT;

    if (is_busy())
        bench_violation(JESD84 "6.15.3", "Data block sent while DAT0 is busy");

    if (!check_data_bus())
        return BUS_DATA_CRC;
    if (len != x->block_size) {
        bench_violation(JESD84 "6.6.8.1",
                        "CMD%u: the host writes %zu byte blocks, the device takes %zu",
                        x->cmd,
                        len,
                        x->block_size);
        return BUS_DATA_CRC;
    }

    if (emmc.fail_write != BUS_DATA_OK) {
        enum bus_data_status st = emmc.fail_write;

        emmc.fail_write = BUS_DATA_OK;
        /*
         * 6.6.8.1: a bad block and the rest of the command are dropped. A
         * failed transfer goes back to tran (6.6), a multiple block write
         * stays in rcv until CMD12.
         */
        x->active = false;
        if (x->cmd != 25)
            emmc.state = EMMC_TRAN;
        return st;
    }

    if (emmc.power_loss_blocks == 0) {
        emmc.powered = false;
        return BUS_DATA_TIMEOUT;
    }
    if (emmc.power_loss_blocks > 0)
        emmc.power_loss_blocks--;

    if (x->src == SRC_RPMB) {
        memcpy(emmc.rpmb_request, buf, BLOCK_SIZE);
        emmc.rpmb_request_valid = true;
    } else {
        if (x->lba >= part_blocks(x->part)) {
            emmc.status |= ST_ADDRESS_OUT_OF_RANGE;
            x->active = false;
            return BUS_DATA_OK;
        }
        bench_store_write(&emmc.store[x->part], x->lba, buf);
    }

    emmc.blocks_written++;
    x->lba++;
    set_busy(emmc.cfg.write_busy_us, EMMC_TRAN);
    if (x->counted && --x->left == 0) {
        x->active = false;
        emmc.state = EMMC_PRG;
    }
    return BUS_DATA_OK;
}

static bool card_busy(struct bus_card *card)
{
    (void)card;

    if (!emmc.powered)
        return false;
    progress();
    return is_busy();
}

static void card_set_bus(struct bus_card *card, const struct bus_cfg *cfg)
{
    (void)card;
    emmc.bus_cfg = *cfg;
}

static bool card_present(struct bus_card *card)
{
    (void)card;
    return true; /* Soldered down */
}

static const struct bus_card_ops ops = {
    .cmd = card_cmd,
    .read = card_read,
    .write = card_write,
    .busy = card_busy,
    .set_bus = card_set_bus,
    .present = card_present,
};

void emmc_model_default_config(struct emmc_model_config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->user_sectors = 16 * 1024 * 1024; /* 8 GiB */
    cfg->boot_size_mult = 32; /* 4 MiB */
    cfg->rpmb_size_mult = 4; /* 512 KiB */
    cfg->device_type = DT_HS400_18 | DT_HS200_18 | DT_DDR52_18_3V | DT_HS52 | DT_HS26;
    cfg->driver_strength = 0x1f;
    cfg->strobe_support = 1;
    cfg->generic_cmd6_time = 25;
    cfg->partition_switch_time = 3;
    cfg->power_off_long_time = 100;
    cfg->vccq_mv = 1800;
    cfg->op_cond_busy = 3;
    cfg->switch_busy_us = 1000;
    cfg->write_busy_us = 50;
    cfg->unreliable_cmd13_in_timing_switch = true;
}

struct bus_card *emmc_model_init(const struct emmc_model_config *cfg)
{
    emmc_model_free();

    emmc.bus.ops = &ops;
    emmc.cfg = *cfg;
    for (int p = 0; p < EMMC_PART_COUNT; p++) {
        if (part_blocks(p) > 0)
            bench_store_init(&emmc.store[p], part_blocks(p), BLOCK_SIZE);
    }
    build_cid();
    build_csd();
    build_ext_csd();

    emmc.powered = true;
    emmc.fail_cmd_idx = -1;
    emmc.fail_switch_index = -1;
    emmc.status_error_idx = -1;
    emmc.power_loss_blocks = -1;
    go_idle();
    return &emmc.bus;
}

void emmc_model_free(void)
{
    for (int p = 0; p < EMMC_PART_COUNT; p++) {
        if (emmc.store[p].chunks != NULL)
            bench_store_free(&emmc.store[p]);
    }
    memset(&emmc, 0, sizeof(emmc));
}

void emmc_model_power_cycle(void)
{
    if (emmc.powered && emmc.ext_csd[EXT_CSD_POWER_OFF_NOTIFICATION] == PON_POWERED_ON)
        bench_violation(JESD84 "6.6.36",
                        "Powered off after POWERED_ON without a power off notification");
    if (emmc.powered && is_busy())
        bench_violation(JESD84 "6.6", "Powered off while the device is busy");

    emmc.powered = true;
    emmc.power_loss_blocks = -1;
    go_idle();
    reset_modes(true);
}

void emmc_model_rom_handover(uint8_t timing, uint8_t bus_width)
{
    emmc.state = EMMC_TRAN;
    emmc.csd_read = true;
    emmc.ext_csd[EXT_CSD_HS_TIMING] = timing;
    emmc.ext_csd[EXT_CSD_BUS_WIDTH] = bus_width;
}

enum emmc_state emmc_model_state(void)
{
    progress();
    return emmc.state;
}

const uint8_t *emmc_model_ext_csd(void)
{
    return emmc.ext_csd;
}

void emmc_model_set_ext_csd(unsigned int index, uint8_t value)
{
    emmc.ext_csd[index] = value;
}

bool emmc_model_powered(void)
{
    return emmc.powered;
}

unsigned int emmc_model_cmd_count(uint8_t idx)
{
    return emmc.cmd_count[idx & 63];
}

unsigned int emmc_model_blocks_read(void)
{
    return emmc.blocks_read;
}

unsigned int emmc_model_blocks_written(void)
{
    return emmc.blocks_written;
}

void emmc_model_read(enum emmc_part part, uint64_t lba, void *buf, size_t blocks)
{
    for (size_t i = 0; i < blocks; i++)
        bench_store_read(&emmc.store[part], lba + i, (uint8_t *)buf + i * BLOCK_SIZE);
}

void emmc_model_write(enum emmc_part part, uint64_t lba, const void *buf, size_t blocks)
{
    for (size_t i = 0; i < blocks; i++)
        bench_store_write(&emmc.store[part], lba + i, (const uint8_t *)buf + i * BLOCK_SIZE);
}

void emmc_model_fail_cmd(uint8_t idx, enum emmc_fault fault)
{
    emmc.fail_cmd_idx = idx;
    emmc.fail_cmd_kind = fault;
}

void emmc_model_fail_switch(uint8_t index)
{
    emmc.fail_switch_index = index;
}

void emmc_model_fail_read(enum bus_data_status status)
{
    emmc.fail_read = status;
}

void emmc_model_fail_write(enum bus_data_status status)
{
    emmc.fail_write = status;
}

void emmc_model_status_error(uint8_t idx, uint32_t bits)
{
    emmc.status_error_idx = idx;
    emmc.status_error_bits = bits;
}

void emmc_model_power_loss_after(int blocks)
{
    emmc.power_loss_blocks = blocks;
}
