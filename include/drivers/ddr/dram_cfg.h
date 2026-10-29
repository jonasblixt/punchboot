/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Types for DRAM timing tables, shared by the DDR controller and PHY
 * drivers and the board timing files.
 */

#ifndef INCLUDE_DRIVERS_DDR_DRAM_CFG_H
#define INCLUDE_DRIVERS_DDR_DRAM_CFG_H

/* Training firmware image */
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

#endif // INCLUDE_DRIVERS_DDR_DRAM_CFG_H
