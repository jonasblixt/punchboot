/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The MMC/SD bus between a host model and a card model.
 *
 * A host model is either a fake mmc_hal (tests of mmc_core alone) or a
 * register model of a host controller (tests of a controller driver and
 * mmc_core together). It drives the bus through these calls the way the
 * controller drives the CMD and DAT lines; the card model decides what the
 * card answers and checks the host against the standard (see bench.h).
 */

#ifndef TESTS_UNIT_MODELS_MMC_BUS_H
#define TESTS_UNIT_MODELS_MMC_BUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The response the host waits for, as a controller encodes it */
enum bus_rsp_type {
    BUS_RSP_NONE,
    BUS_RSP_48, /* R1, R3, R4, R5, R6, R7 */
    BUS_RSP_48_BUSY, /* R1b: 48 bits, then busy on DAT0 */
    BUS_RSP_136, /* R2 */
};

struct bus_cmd {
    uint8_t idx;
    uint32_t arg;
    enum bus_rsp_type rsp; /* What the host waits for */
};

struct bus_rsp {
    bool timeout; /* No response: illegal command, wrong state, no card */
    bool crc_error; /* The response arrived but its CRC did not match */
    uint8_t idx; /* Index field, 0x3f for R2 and R3 */
    /*
     * 48 bits: r[0] holds bits [39:8] (status, OCR, ...).
     * 136 bits: r[0..3] hold register bits [31:0] .. [127:96], with the
     * register's CRC7 in [7:1] and bit 0 set.
     */
    uint32_t r[4];
};

enum bus_data_status {
    BUS_DATA_OK,
    BUS_DATA_CRC, /* Read: CRC16 mismatch. Write: negative CRC status token */
    BUS_DATA_TIMEOUT, /* Read: no start bit. Write: no CRC status token */
};

/* How the host drives the bus */
struct bus_cfg {
    unsigned int clk_hz; /* 0: clock off */
    unsigned int width; /* 1, 4 or 8 data lines */
    bool ddr; /* Data on both clock edges */
    unsigned int vdd_mv; /* Card supply, 0: off (SD cards) */
    unsigned int vccq_mv; /* Signal voltage */
};

struct bus_card;

struct bus_card_ops {
    void (*cmd)(struct bus_card *card, const struct bus_cmd *cmd, struct bus_rsp *rsp);
    /* One data block from the card */
    enum bus_data_status (*read)(struct bus_card *card, void *buf, size_t len);
    /* One data block to the card */
    enum bus_data_status (*write)(struct bus_card *card, const void *buf, size_t len);
    /* DAT0 held low by the card */
    bool (*busy)(struct bus_card *card);
    void (*set_bus)(struct bus_card *card, const struct bus_cfg *cfg);
    /* Card detect pin: a card is in the slot */
    bool (*present)(struct bus_card *card);
};

struct bus_card {
    const struct bus_card_ops *ops;
};

#endif // TESTS_UNIT_MODELS_MMC_BUS_H
