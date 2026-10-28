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
 *  - send_cmd() sends the command with the response type the core asks for,
 *    checks the response CRC and index like a controller with CCCEN and
 *    CICEN set, and waits for DAT0 busy to end after an R1b response
 *  - R2 responses are returned as register bits [127:8] in result[3..0] with
 *    bits [7:0] cleared, as the uSDHC returns them
 *  - prepare() records the buffer, read() and write() move the data in
 *    blocks of min(length, 512) bytes
 *  - multiple block transfers not preceded by CMD23 get a CMD12 after the
 *    data, like the uSDHC's auto CMD12, and read() or write() fails if its
 *    response has an error bit set
 */

#ifndef TESTS_UNIT_MODELS_HAL_MODEL_H
#define TESTS_UNIT_MODELS_HAL_MODEL_H

#include <drivers/mmc/mmc_core.h>

#include "mmc_bus.h"

const struct mmc_hal *hal_model_init(struct bus_card *card, size_t max_chunk_bytes);

/* The bus as the HAL drives it */
const struct bus_cfg *hal_model_bus(void);

#endif // TESTS_UNIT_MODELS_HAL_MODEL_H
