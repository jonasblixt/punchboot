/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * i.MX8M LPDDR4 init: board timing data and the copy handed to TF-A.
 */

#ifndef INCLUDE_PLAT_IMX8M_DRAM_H_
#define INCLUDE_PLAT_IMX8M_DRAM_H_

#include "ddrphy.h"
#include "lpddr4.h"
#include "umctl2.h"

enum fw_type {
    FW_1D_IMAGE,
    FW_2D_IMAGE,
};

/* A register (controller address or PHY CSR number) and its value */
struct dram_cfg_param {
    unsigned int reg;
    unsigned int val;
};

/* One training run: data rate, firmware and message block */
struct dram_fsp_msg {
    unsigned int drate; /* MT/s */
    enum fw_type fw_type;
    struct dram_cfg_param *fsp_cfg;
    unsigned int fsp_cfg_num;
};

/*
 * Everything needed to bring up the DRAM, and after training also the
 * trained PHY CSRs. umctl2_init() copies this to the start of DRAM
 * (.umctl2), where TF-A's i.MX8M DRAM driver picks it up for low power
 * modes and frequency changes. The layout must match TF-A's
 * struct dram_timing_info, don't change it.
 */
struct dram_timing_info {
    /* umctl2 config */
    struct dram_cfg_param *ddrc_cfg;
    unsigned int ddrc_cfg_num;
    /* ddrphy config */
    struct dram_cfg_param *ddrphy_cfg;
    unsigned int ddrphy_cfg_num;
    /* ddr fsp train info */
    struct dram_fsp_msg *fsp_msg;
    unsigned int fsp_msg_num;
    /* ddr phy trained CSR */
    struct dram_cfg_param *ddrphy_trained_csr;
    unsigned int ddrphy_trained_csr_num;
    /* ddr phy PIE */
    struct dram_cfg_param *ddrphy_pie;
    unsigned int ddrphy_pie_num;
    /* initialized fsp table */
    unsigned int fsp_table[4];
};

/* The board's timing, see src/board/imx8mevk/lpddr4_timing.c */
extern struct dram_timing_info dram_timing;

int umctl2_init(void);

#endif // INCLUDE_PLAT_IMX8M_DRAM_H_
