/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BOARD_PICO8ML_DRAM_H
#define BOARD_PICO8ML_DRAM_H

/* The DRAM's density in GiB, read from its straps; 0 when they name none */
unsigned int pico8ml_dram_gib(void);

#endif // BOARD_PICO8ML_DRAM_H
