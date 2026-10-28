/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * An SD memory card on the bus (mmc_bus.h): SDSC, SDHC and SDXC, default
 * speed, high speed and the UHS-I modes, written from the SD Physical Layer
 * Specification as the author knows it. The specification was not at hand,
 * so violations name the command or the rule ("SD PLS ACMD41") instead of a
 * section number.
 *
 * What it covers:
 *
 *  - card states and the commands each one accepts, illegal commands get
 *    no response and set ILLEGAL_COMMAND
 *  - CMD8 interface condition, ACMD41 with HCS and S18R, the power up busy
 *    time, CMD11 and the signal voltage switch sequence
 *  - CMD2, CMD3 (the card publishes its RCA, R6), CMD9 with CSD version 1.0
 *    (SDSC) and 2.0 (SDHC/SDXC)
 *  - application commands: CMD55, ACMD6, ACMD13, ACMD22, ACMD23, ACMD41,
 *    ACMD51 (SCR)
 *  - CMD6 SWITCH_FUNC in check and switch mode for the access mode group,
 *    the 64 byte status
 *  - the clock limit of each bus speed mode, the identification clock until
 *    CMD3, the bus width and data rate of every data block
 *  - CMD19 tuning in SDR50 and SDR104
 *  - block reads and writes, CMD23 only when the SCR says it is supported,
 *    CMD12, byte addressing on SDSC
 *  - card detect, power through VDD
 *
 * Not covered: SPI mode, erase, write protection, lock/unlock, security,
 * extension registers, UHS-II, speed class control.
 */

#ifndef TESTS_UNIT_MODELS_SD_MODEL_H
#define TESTS_UNIT_MODELS_SD_MODEL_H

#include "mmc_bus.h"

enum sd_state {
    SD_IDLE,
    SD_READY,
    SD_IDENT,
    SD_STBY,
    SD_TRAN,
    SD_DATA,
    SD_RCV,
    SD_PRG,
    SD_DIS,
    SD_INA = 16,
};

/* Access mode, CMD6 function group 1 */
enum sd_speed {
    SD_DEFAULT_SPEED, /* SDR12, 25 MHz */
    SD_HIGH_SPEED, /* SDR25, 50 MHz */
    SD_SDR50,
    SD_SDR104,
    SD_DDR50,
};

struct sd_model_config {
    uint64_t sectors; /* More than 4 GiB: SDHC (up to 32 GiB) or SDXC */
    bool v1; /* Physical layer 1.x: no CMD8, SDSC only */
    bool uhs; /* Supports 1.8 V signaling and the UHS-I modes */
    bool cmd23; /* SCR CMD_SUPPORT: SET_BLOCK_COUNT */
    unsigned int op_cond_busy; /* ACMD41s answered busy */
    unsigned int write_busy_us;
    uint16_t rca; /* The RCA the card publishes with CMD3 */
};

void sd_model_default_config(struct sd_model_config *cfg);

/* Creates the card, inserted but not powered */
struct bus_card *sd_model_init(const struct sd_model_config *cfg);
void sd_model_free(void);

void sd_model_insert(bool inserted);

enum sd_state sd_model_state(void);
enum sd_speed sd_model_speed(void);
unsigned int sd_model_bus_width(void); /* ACMD6 */
bool sd_model_1v8(void); /* Signaling at 1.8 V after CMD11 */
unsigned int sd_model_cmd_count(uint8_t idx); /* Including ACMDs */
unsigned int sd_model_acmd_count(uint8_t idx);

void sd_model_read(uint64_t lba, void *buf, size_t blocks);
void sd_model_write(uint64_t lba, const void *buf, size_t blocks);

/* Faults, each once */
void sd_model_fail_read(enum bus_data_status status);
void sd_model_fail_write(enum bus_data_status status);

#endif // TESTS_UNIT_MODELS_SD_MODEL_H
