/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * i.MX8M DRAM init: board timing data and the copy handed to TF-A.
 */

#ifndef INCLUDE_PLAT_IMX8M_DRAM_H_
#define INCLUDE_PLAT_IMX8M_DRAM_H_

#include <drivers/ddr/dram_cfg.h>
#include <drivers/ddr/dwc_ddrphy.h>
#include <drivers/ddr/lpddr4.h>
#include <plat/imx8m/ddrc.h>

#define IMX8M_DDRPHY_BASE       0x3c000000
#define IMX8M_DDRPHY_CSR_STRIDE 4

/*
 * Everything needed to bring up the DRAM, and after training also the
 * trained PHY CSRs. imx8m_ddr_init() copies this to the start of DRAM
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

int imx8m_ddr_init(void);

#endif // INCLUDE_PLAT_IMX8M_DRAM_H_
