/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The pico8ml's LPDDR4 timings, one per density (lpddr4_timing_<n>gb.c)
 */

#ifndef BOARD_PICO8ML_LPDDR4_TIMING_H
#define BOARD_PICO8ML_LPDDR4_TIMING_H

#include <plat/imx8m/dram.h>

/* The tables the four have the same (lpddr4_timing_common.c) */
#define PICO8ML_DDRPHY_CFG_NUM 161
extern struct dram_cfg_param pico8ml_ddrphy_cfg[PICO8ML_DDRPHY_CFG_NUM];
#define PICO8ML_DDRPHY_TRAINED_CSR_NUM 719
extern struct dram_cfg_param pico8ml_ddrphy_trained_csr[PICO8ML_DDRPHY_TRAINED_CSR_NUM];
#define PICO8ML_PHY_PIE_NUM 580
extern struct dram_cfg_param pico8ml_phy_pie[PICO8ML_PHY_PIE_NUM];

extern struct dram_timing_info dram_timing_1gb;
extern struct dram_timing_info dram_timing_2gb;
extern struct dram_timing_info dram_timing_3gb;
extern struct dram_timing_info dram_timing_4gb;

#endif /* BOARD_PICO8ML_LPDDR4_TIMING_H */
