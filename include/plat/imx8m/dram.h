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
    struct dram_cfg_param *ddrc_cfg; /* Controller registers, absolute addresses */
    unsigned int ddrc_cfg_num;
    struct dram_cfg_param *ddrphy_cfg; /* PHY CSRs written before training */
    unsigned int ddrphy_cfg_num;
    struct dram_fsp_msg *fsp_msg; /* Training runs, in order (not read by TF-A) */
    unsigned int fsp_msg_num;
    struct dram_cfg_param *ddrphy_trained_csr; /* PHY CSRs to read back after training */
    unsigned int ddrphy_trained_csr_num;
    struct dram_cfg_param *ddrphy_pie; /* PHY Init Engine image */
    unsigned int ddrphy_pie_num;
    unsigned int fsp_table[4]; /* Data rate (MT/s) of each frequency set point, 0: unused */
};

/*
 * The copy for TF-A: struct dram_timing_info followed by the tables it
 * points to, in the 128 KiB .umctl2 region at the start of DRAM (link.lds).
 */
#define IMX8M_DRAM_TIMING_SAVE_SIZE (128 * 1024)

struct dram_timing_save {
    struct dram_timing_info info;
    struct dram_cfg_param cfg[(IMX8M_DRAM_TIMING_SAVE_SIZE - sizeof(struct dram_timing_info)) /
                              sizeof(struct dram_cfg_param)];
};

extern struct dram_timing_save dram_timing_info_copy;

/* The board's timing, see src/board/imx8mevk/lpddr4_timing.c */
extern struct dram_timing_info dram_timing;

/*
 * The data rates (MT/s) the board trains, from its board code: the first
 * 'num' set points of dram_timing.fsp_table, in that order, since TF-A
 * uses a set point's index as the controller and PHY state. The training
 * runs of the others are skipped and TF-A gets 0 for them, so it never
 * switches to them (DDR frequency scaling in Linux uses the set points).
 */
struct dram_rates {
    const unsigned int *mts;
    unsigned int num;
};

extern struct dram_rates dram_rates;

int imx8m_ddr_init(void);

#endif // INCLUDE_PLAT_IMX8M_DRAM_H_
