/**
 * Punch BOOT
 *
 * Copyright (C) 2023, 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The imx8mpevk's DRAM rates, in both stages (board.md 7, D-088). Its timing,
 * lpddr4_timing.c, is the SPL's only.
 */

#include <pb/pb.h>
#include <plat/imx8m/dram.h>

/*
 * DRAM: only the 4000 MT/s set point is trained. The timing also has 400
 * and 100 MT/s for DDR frequency scaling in Linux, which this board doesn't
 * use, and their training takes 215 ms of the boot.
 */
static const unsigned int imx8mpevk_dram_mts[] = { 4000 };

struct dram_rates dram_rates = {
    .mts = imx8mpevk_dram_mts,
    .num = ARRAY_SIZE(imx8mpevk_dram_mts),
};
