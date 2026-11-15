/**
 * Punch BOOT
 *
 * Copyright (C) 2023, 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The imx8mevk's DRAM rates, in both stages (board.md 7, D-088). Its timing,
 * lpddr4_timing.c, is the SPL's only.
 */

#include <pb/pb.h>
#include <plat/imx8m/dram.h>

/* DRAM: all set points of the timing, for DDR frequency scaling in Linux */
static const unsigned int imx8mevk_dram_mts[] = { 3200, 400, 100 };

struct dram_rates dram_rates = {
    .mts = imx8mevk_dram_mts,
    .num = ARRAY_SIZE(imx8mevk_dram_mts),
};
