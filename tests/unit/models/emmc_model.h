/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * An e.MMC 5.1 device on the bus (mmc_bus.h), written from JESD84-B51.
 *
 * The model answers commands, holds data and the EXT_CSD, and logs every
 * host action that the standard does not allow (bench.h), citing the
 * section. What it covers:
 *
 *  - the device states and the commands each one accepts (6.11), illegal
 *    commands get no response and set ILLEGAL_COMMAND (6.8.1)
 *  - card status bits and when they clear (6.13)
 *  - identification: CMD1 busy and access mode (6.4), CMD2, CMD3, CMD9
 *  - the clock limit of each timing mode, and the identification clock
 *    until the CSD has been read (6.6, 6.6.2, 10.x)
 *  - CMD6: access modes, which EXT_CSD fields can be written and how often,
 *    SWITCH_ERROR (6.6.1, 7.4), busy on DAT0 afterwards
 *  - the bus mode rules: HS, DDR52, HS200, HS400 with and without Enhanced
 *    Strobe (6.6.2, 6.6.23, 7.4.65, 7.4.67), and that the host's bus width
 *    and data rate match the device's on every data block
 *  - CMD21 tuning blocks (6.6.5.1)
 *  - block reads and writes, open-ended and with CMD23, CMD12 (6.6.7, 6.6.8)
 *  - partition access and the boot configuration (6.2.5, 7.4.69-71),
 *    commands allowed in the RPMB partition (6.2.2)
 *  - power off notification (6.6.36)
 *
 * Not covered: boot operation, erase/trim/sanitize, write protection,
 * lock/unlock, sleep, command queuing, the RPMB authentication protocol
 * (frames are answered with "key not programmed"), bus testing.
 */

#ifndef TESTS_UNIT_MODELS_EMMC_MODEL_H
#define TESTS_UNIT_MODELS_EMMC_MODEL_H

#include "mmc_bus.h"

/* PARTITION_ACCESS values */
enum emmc_part {
    EMMC_PART_USER,
    EMMC_PART_BOOT1,
    EMMC_PART_BOOT2,
    EMMC_PART_RPMB,
    EMMC_PART_COUNT,
};

/* Device states, as in CURRENT_STATE (Table 68) */
enum emmc_state {
    EMMC_IDLE,
    EMMC_READY,
    EMMC_IDENT,
    EMMC_STBY,
    EMMC_TRAN,
    EMMC_DATA,
    EMMC_RCV,
    EMMC_PRG,
    EMMC_DIS,
    EMMC_BTST,
    EMMC_SLP,
    EMMC_INA = 16, /* Not reported in CURRENT_STATE */
};

/* CSD TRAN_SPEED, 7.3.10 */
#define TRAN_SPEED_20MHZ 0x2a
#define TRAN_SPEED_26MHZ 0x32

struct emmc_model_config {
    uint64_t user_sectors; /* More than 2 GiB: sector addressing (6.4.2) */
    uint8_t boot_size_mult; /* BOOT_SIZE_MULT, 128 KiB units */
    uint8_t rpmb_size_mult; /* RPMB_SIZE_MULT, 128 KiB units */
    uint8_t device_type; /* DEVICE_TYPE [196] */
    uint8_t driver_strength; /* DRIVER_STRENGTH [197] */
    uint8_t strobe_support; /* STROBE_SUPPORT [184] */
    uint8_t generic_cmd6_time; /* 10 ms units, 0: not defined */
    uint8_t partition_switch_time; /* 10 ms units, 0: not defined */
    uint8_t power_off_long_time; /* 10 ms units, 0: not defined */
    uint8_t tran_speed; /* CSD TRAN_SPEED (7.3.10), the backward compatible clock limit */
    /* Nonvolatile settings the device comes with */
    uint8_t partition_config; /* PARTITION_ACCESS is cleared at power on */
    uint8_t boot_bus_conditions;
    uint8_t boot_config_prot;
    unsigned int vccq_mv; /* 1800 or 3300, HS200 and HS400 need 1.8 V */
    unsigned int op_cond_busy; /* CMD1s answered busy after power on */
    unsigned int switch_busy_us; /* DAT0 busy after CMD6 */
    unsigned int write_busy_us; /* DAT0 busy after each written block */
    /*
     * 6.6.2: CMD13 during a timing change can't be relied on. When set,
     * responses during that busy period have bad CRCs.
     */
    bool unreliable_cmd13_in_timing_switch;
    bool allow_otp_writes; /* One-time programmable fields, logged otherwise */
};

enum emmc_fault {
    EMMC_FAULT_NO_RESPONSE,
    EMMC_FAULT_RSP_CRC,
};

void emmc_model_default_config(struct emmc_model_config *cfg);

/* Creates the device, powered on, in Idle */
struct bus_card *emmc_model_init(const struct emmc_model_config *cfg);
void emmc_model_free(void);

/*
 * The host removes power and powers the device up again. Logs a violation
 * if the host promised a power off notification and did not give one
 * (6.6.36).
 */
void emmc_model_power_cycle(void);

/* Puts the device where a boot ROM may leave it: selected, in Transfer state,
 * with the given HS_TIMING and BUS_WIDTH */
void emmc_model_rom_handover(uint8_t hs_timing, uint8_t bus_width);

enum emmc_state emmc_model_state(void);
/* The EXT_CSD as the device holds it, write-only fields included */
const uint8_t *emmc_model_ext_csd(void);
void emmc_model_set_ext_csd(unsigned int index, uint8_t value);
bool emmc_model_powered(void);
/* Number of times command idx was received since emmc_model_init() */
unsigned int emmc_model_cmd_count(uint8_t idx);
/* The bus clock at the first command idx since emmc_model_init(), 0 if none */
unsigned int emmc_model_cmd_first_clk_hz(uint8_t idx);
/* Number of data blocks moved since emmc_model_init() */
unsigned int emmc_model_blocks_read(void);
unsigned int emmc_model_blocks_written(void);

void emmc_model_read(enum emmc_part part, uint64_t lba, void *buf, size_t blocks);
void emmc_model_write(enum emmc_part part, uint64_t lba, const void *buf, size_t blocks);

/* Faults, each taking effect once */
void emmc_model_fail_cmd(uint8_t idx, enum emmc_fault fault);
void emmc_model_fail_switch(uint8_t index); /* Next CMD6 to 'index': SWITCH_ERROR */
void emmc_model_fail_read(enum bus_data_status status); /* Next block read */
void emmc_model_fail_write(enum bus_data_status status); /* Next block written */
/* Execution-mode error bits (Table 68) after the next command 'idx' */
void emmc_model_status_error(uint8_t idx, uint32_t bits);
/* Power is lost after 'blocks' more written blocks, < 0: never */
void emmc_model_power_loss_after(int blocks);

#endif // TESTS_UNIT_MODELS_EMMC_MODEL_H
