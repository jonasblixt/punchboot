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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

/*
 * A table of PHY CSRs, packed: the PHY's values are 16 bits, and its tables
 * are mostly runs of consecutive CSRs. 'runs' is 16-bit words, run after run:
 *
 *   csr >> 16, csr & 0xffff, n, then n values (none without 'values')
 *
 * for the n CSRs from csr on. A table without values names the CSRs that
 * are read back after the training. scripts/gen-imx8m-dram-timing.py
 * writes them from NXP's DDR tool's output.
 *
 * Or, instead of the runs, 'pairs': (CSR, value) pairs as the tool writes
 * them, for a timing of struct dram_timing_info's layout.
 */
struct dram_phy_table {
    const uint16_t *runs;
    unsigned int runs_len; /* Words in 'runs' */
    const struct dram_cfg_param *pairs;
    unsigned int num; /* CSRs in the table */
    bool values; /* false: the CSRs only */
};

/* A table of runs, from a const uint16_t array */
#define DRAM_PHY_RUNS(_runs, _num, _values)             \
    {                                                   \
        .runs = (_runs),                                \
        .runs_len = sizeof(_runs) / sizeof((_runs)[0]), \
        .pairs = NULL,                                  \
        .num = (_num),                                  \
        .values = (_values),                            \
    }

/* Walks a struct dram_phy_table, CSR by CSR */
struct dram_phy_iter {
    const struct dram_phy_table *table;
    unsigned int pos; /* The next word or pair */
    unsigned int left; /* CSRs left in the current run */
    uint32_t csr; /* The next CSR of the run */
};

static inline void dram_phy_iter_init(struct dram_phy_iter *it, const struct dram_phy_table *t)
{
    it->table = t;
    it->pos = 0;
    it->left = 0;
    it->csr = 0;
}

/* The next CSR and its value (0 in a table without values); false at the end */
static inline bool dram_phy_iter_next(struct dram_phy_iter *it, uint32_t *csr, uint32_t *val)
{
    const struct dram_phy_table *t = it->table;

    if (t->pairs != NULL) {
        if (it->pos >= t->num)
            return false;
        *csr = t->pairs[it->pos].reg;
        *val = t->pairs[it->pos].val;
        it->pos++;
        return true;
    }

    while (it->left == 0) {
        if (it->pos + 3 > t->runs_len)
            return false;
        it->csr = ((uint32_t)t->runs[it->pos] << 16) | t->runs[it->pos + 1];
        it->left = t->runs[it->pos + 2];
        it->pos += 3;
    }

    if (t->values && it->pos >= t->runs_len)
        return false;

    *csr = it->csr++;
    *val = t->values ? t->runs[it->pos++] : 0;
    it->left--;
    return true;
}

/* One training run: data rate, firmware and message block */
struct dram_fsp_msg {
    unsigned int drate; /* MT/s */
    enum fw_type fw_type;
    struct dram_cfg_param *fsp_cfg;
    unsigned int fsp_cfg_num;
};

#endif // INCLUDE_DRIVERS_DDR_DRAM_CFG_H
