/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * An mmc_hal that drives a card model the way a correct host controller
 * does, for testing mmc_core on its own:
 *
 *  - request() sends the command with the response type the core asks for,
 *    checks the response CRC and index like a controller with CCCEN and
 *    CICEN set, and waits for DAT0 busy to end after an R1b response and
 *    after written data
 *  - R2 responses are returned as register bits [127:8] in resp[3..0] with
 *    bits [7:0] cleared, as the uSDHC returns them
 *  - with 'stop', and after a data error in a multiple block transfer, it
 *    sends CMD12 after the data and fails if the response has an error
 *    bit set (not ADDRESS_OUT_OF_RANGE after a read, JESD84-B51 6.8.3)
 */

#ifndef TESTS_UNIT_MODELS_HAL_MODEL_H
#define TESTS_UNIT_MODELS_HAL_MODEL_H

#include <drivers/mmc/mmc_core.h>

#include "mmc_bus.h"

const struct mmc_hal *hal_model_init(struct bus_card *card, size_t max_chunk_bytes);

/* The bus as the HAL drives it */
const struct bus_cfg *hal_model_bus(void);

/*
 * The delay taps (128) that sample data correctly above 52 MHz, outside
 * the window blocks arrive with CRC errors. Default: all; lo > hi: none.
 * Kept across hal_model_init().
 */
void hal_model_set_window(int lo, int hi);
/* Taps inside the window that fail too, lo > hi: none (default) */
void hal_model_set_gap(int lo, int hi);
unsigned int hal_model_tap(void);

#endif // TESTS_UNIT_MODELS_HAL_MODEL_H
