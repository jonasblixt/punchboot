/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The pico8ml's LPDDR4 timings, one per density: lpddr4_timing_<n>gb.c,
 * NXP's DDR tool's output as TechNexion's U-Boot has it, never edited
 * (imx8m-ddr.md)
 */

#ifndef BOARD_PICO8ML_LPDDR4_TIMING_H
#define BOARD_PICO8ML_LPDDR4_TIMING_H

#include <plat/imx8m/dram.h>

extern struct dram_timing_info dram_timing_1gb;
extern struct dram_timing_info dram_timing_2gb;
extern struct dram_timing_info dram_timing_3gb;
extern struct dram_timing_info dram_timing_4gb;

#endif /* BOARD_PICO8ML_LPDDR4_TIMING_H */
