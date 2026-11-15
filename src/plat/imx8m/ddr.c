/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 * Copyright 2018-2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * i.MX8M DRAM init, common to the i.MX8M SoCs: the sequence around the
 * uMCTL2 controller (drivers/ddr/umctl2.c) and the DWC PHY with firmware
 * training (drivers/ddr/dwc_ddrphy.c). Resets and clocks that differ per
 * SoC are in ddr_<soc>.c (ddr_soc.h).
 *
 * The order of the steps outside the training (4) is that of NXP's
 * BSD-3-Clause i.MX8M DRAM code in TF-A (plat/imx/imx8m/ddr/
 * dram_retention.c, dram_exit_retention(), and dram.c), which re-runs this
 * part of the init after retention, and so are the reset release values
 * and the DDR_SS_GPR0 condition. Some register values differ from TF-A's,
 * see drivers/ddr/umctl2.c. Sequence:
 *
 *  1. Reset and power up the DDR subsystem, set the DRAM clock.
 *  2. Program the controller from the board table, release the resets
 *     and put the (not yet initialized) SDRAM in self refresh.
 *  3. Program the PHY from the board table.
 *  4. For each training run (frequency set point, 1D/2D) in the board's
 *     fsp_msg table: set the clock, load the firmware and its message
 *     block, run the PMU and wait for its result.
 *  5. Load the PHY Init Engine (PIE) image, start the DFI init and leave
 *     self refresh.
 *  6. Save the timing data and the trained PHY CSRs for TF-A.
 *
 * The register trace of imx8m_ddr_init() is checked by
 * tests/unit/test_imx8m_ddr.c.
 */

#include <pb/mmio.h>
#include <pb/pb.h>
#include <plat/imx8m/dram.h>
#include <plat/imx8m/imx8m.h>
#include <string.h>

#include "ddr_soc.h"

extern uint32_t _binary_blobs_lpddr4_pmu_train_1d_imem_bin_start;
extern uint32_t _binary_blobs_lpddr4_pmu_train_1d_dmem_bin_start;
extern uint32_t _binary_blobs_lpddr4_pmu_train_2d_imem_bin_start;
extern uint32_t _binary_blobs_lpddr4_pmu_train_2d_dmem_bin_start;
extern uint32_t _binary_blobs_lpddr4_pmu_train_1d_imem_bin_end;
extern uint32_t _binary_blobs_lpddr4_pmu_train_1d_dmem_bin_end;
extern uint32_t _binary_blobs_lpddr4_pmu_train_2d_imem_bin_end;
extern uint32_t _binary_blobs_lpddr4_pmu_train_2d_dmem_bin_end;

#define __umctl2    __attribute__((section(".umctl2")))

#define RCR_DOMAINS (SRC_DDRC_RCR_DOM_EN | SRC_DDRC_RCR_DOMAIN_ALL)

/* Training firmware images, linked in from the board's blobs */
static const struct dwc_ddrphy_fw fw_1d = {
    .imem = &_binary_blobs_lpddr4_pmu_train_1d_imem_bin_start,
    .imem_end = &_binary_blobs_lpddr4_pmu_train_1d_imem_bin_end,
    .dmem = &_binary_blobs_lpddr4_pmu_train_1d_dmem_bin_start,
    .dmem_end = &_binary_blobs_lpddr4_pmu_train_1d_dmem_bin_end,
};

static const struct dwc_ddrphy_fw fw_2d = {
    .imem = &_binary_blobs_lpddr4_pmu_train_2d_imem_bin_start,
    .imem_end = &_binary_blobs_lpddr4_pmu_train_2d_imem_bin_end,
    .dmem = &_binary_blobs_lpddr4_pmu_train_2d_dmem_bin_start,
    .dmem_end = &_binary_blobs_lpddr4_pmu_train_2d_dmem_bin_end,
};

static const struct dwc_ddrphy phy = {
    .base = IMX8M_DDRPHY_BASE,
    .csr_stride = IMX8M_DDRPHY_CSR_STRIDE,
    .fw = { [FW_1D_IMAGE] = &fw_1d, [FW_2D_IMAGE] = &fw_2d },
};

__umctl2 struct dram_timing_save dram_timing_info_copy;

/* Append table 'src' at 'pos' and point 'table' to it */
static void copy_cfg(struct dram_cfg_param **pos,
                     struct dram_cfg_param **table,
                     const struct dram_cfg_param *src,
                     unsigned int n)
{
    *table = *pos;
    for (unsigned int i = 0; i < n; i++)
        *(*pos)++ = src[i];
}

/* Append table 't' at 'pos' and point 'table' to it: its CSRs and values,
 * 0 in a table without values */
static void copy_phy(struct dram_cfg_param **pos,
                     struct dram_cfg_param **table,
                     const struct dram_phy_table *t)
{
    struct dram_phy_iter it;
    uint32_t csr;
    uint32_t val;

    *table = *pos;
    dram_phy_iter_init(&it, t);

    while (dram_phy_iter_next(&it, &csr, &val)) {
        (*pos)->reg = csr;
        (*pos)->val = val;
        (*pos)++;
    }
}

/* Write a PHY table's CSRs */
static void write_phy(const struct dram_phy_table *t)
{
    struct dram_phy_iter it;
    uint32_t csr;
    uint32_t val;

    dram_phy_iter_init(&it, t);

    while (dram_phy_iter_next(&it, &csr, &val))
        dwc_ddrphy_write(&phy, csr, val);
}

/*
 * Read back the trained PHY CSRs, and copy all timing data to the start of
 * DRAM for TF-A (see struct dram_timing_info).
 */
static int dram_save_timing(const struct imx8m_dram_timing *t)
{
    struct dram_timing_info *dt = &dram_timing_info_copy.info;
    struct dram_cfg_param *pos = dram_timing_info_copy.cfg;
    size_t n =
        (size_t)t->ddrc_cfg_num + t->ddrphy_cfg.num + t->ddrphy_trained_csr.num + t->ddrphy_pie.num;

    if (n > ARRAY_SIZE(dram_timing_info_copy.cfg)) {
        LOG_ERR("DRAM timing does not fit in the TF-A copy (%zu entries)", n);
        return -PB_ERR_MEM;
    }

    dt->ddrc_cfg_num = t->ddrc_cfg_num;
    dt->ddrphy_cfg_num = t->ddrphy_cfg.num;
    dt->ddrphy_trained_csr_num = t->ddrphy_trained_csr.num;
    dt->ddrphy_pie_num = t->ddrphy_pie.num;
    /* Only the set points that were trained */
    memset(dt->fsp_table, 0, sizeof(dt->fsp_table));
    memcpy(dt->fsp_table, dram_rates.mts, dram_rates.num * sizeof(dt->fsp_table[0]));

    copy_cfg(&pos, &dt->ddrc_cfg, t->ddrc_cfg, t->ddrc_cfg_num);
    copy_phy(&pos, &dt->ddrphy_cfg, &t->ddrphy_cfg);
    copy_phy(&pos, &dt->ddrphy_trained_csr, &t->ddrphy_trained_csr);
    copy_phy(&pos, &dt->ddrphy_pie, &t->ddrphy_pie);

    /* The trained values, read into the copy, with the PHY's CSRs given to
     * the APB for it */
    dwc_ddrphy_read_csrs(&phy, dt->ddrphy_trained_csr, dt->ddrphy_trained_csr_num);

    return PB_OK;
}

/* The board's rates are the first set points of its timing, see struct dram_rates */
static int dram_rates_check(const struct imx8m_dram_timing *t)
{
    if (dram_rates.num == 0 || dram_rates.num > ARRAY_SIZE(t->fsp_table)) {
        LOG_ERR("%u DRAM rates, 1 to %zu", dram_rates.num, ARRAY_SIZE(t->fsp_table));
        return -PB_ERR_PARAM;
    }

    for (unsigned int i = 0; i < dram_rates.num; i++) {
        if (dram_rates.mts[i] != t->fsp_table[i]) {
            LOG_ERR("DRAM rate %u is %u MT/s, set point %u of the timing is %u MT/s",
                    i,
                    dram_rates.mts[i],
                    i,
                    t->fsp_table[i]);
            return -PB_ERR_PARAM;
        }
    }
    return PB_OK;
}

static bool dram_rate_trained(unsigned int drate)
{
    for (unsigned int i = 0; i < dram_rates.num; i++) {
        if (dram_rates.mts[i] == drate)
            return true;
    }
    return false;
}

/*
 * The board's timing (dram.h): its imx8m_dram_timing, or its dram_timing in
 * the tool's layout (boards before the script), unless it chooses among
 * several, e.g. by the straps of its DRAM's density
 */
#pragma weak imx8m_dram_timing
#pragma weak dram_timing

__attribute__((weak)) const struct imx8m_dram_timing *imx8m_board_dram_timing(void)
{
    static struct imx8m_dram_timing tool;

    if (&imx8m_dram_timing != NULL)
        return &imx8m_dram_timing;

    if (&dram_timing == NULL)
        return NULL;

    tool = (struct imx8m_dram_timing){
        .ddrc_cfg = dram_timing.ddrc_cfg,
        .ddrc_cfg_num = dram_timing.ddrc_cfg_num,
        .ddrphy_cfg = { .pairs = dram_timing.ddrphy_cfg,
                        .num = dram_timing.ddrphy_cfg_num,
                        .values = true },
        .fsp_msg = dram_timing.fsp_msg,
        .fsp_msg_num = dram_timing.fsp_msg_num,
        .ddrphy_trained_csr = { .pairs = dram_timing.ddrphy_trained_csr,
                                .num = dram_timing.ddrphy_trained_csr_num },
        .ddrphy_pie = { .pairs = dram_timing.ddrphy_pie,
                        .num = dram_timing.ddrphy_pie_num,
                        .values = true },
    };
    memcpy(tool.fsp_table, dram_timing.fsp_table, sizeof(tool.fsp_table));
    return &tool;
}

int imx8m_ddr_init(void)
{
    const struct imx8m_dram_timing *t = imx8m_board_dram_timing();
    const uintptr_t ddrc = DDRC_IPS_BASE_ADDR(0);
    unsigned int drate;
    int rc;

    if (t == NULL) {
        LOG_ERR("The board has no DRAM timing");
        return -PB_ERR_PARAM;
    }

    drate = t->fsp_msg[0].drate;

    rc = dram_rates_check(t);
    if (rc != PB_OK)
        return rc;

    /* 1. Hold the controller and PHY in reset, power up, clock */
    imx8m_ddr_soc_reset();

    mmio_write_32(GPC_PGC_CPU_0_1_MAPPING, GPC_PGC_CPU_0_1_MAPPING_ALL);
    mmio_clrsetbits_32(GPC_PU_PGC_SW_PUP_REQ, 0, GPC_PU_PGC_SW_PUP_REQ_DDR1);

    rc = imx8m_ddr_soc_set_clock(drate);
    if (rc != PB_OK)
        return rc;

    /* 2. Release the APB reset and PHY power ok, so the controller can be configured */
    mmio_write_32(SRC_DDRC_RCR_ADDR,
                  RCR_DOMAINS | SRC_DDRC_RCR_PHY_RESET | SRC_DDRC_RCR_CORE_RESET_N);

    umctl2_write_cfg(t->ddrc_cfg, t->ddrc_cfg_num);

    /* Release the core reset, then the PHY reset */
    mmio_write_32(SRC_DDRC_RCR_ADDR, RCR_DOMAINS | SRC_DDRC_RCR_PHY_RESET);
    mmio_write_32(SRC_DDRC_RCR_ADDR, RCR_DOMAINS);

    umctl2_enter_selfref_sw(ddrc);

    if (mmio_read_32(ddrc + UMCTL2_MSTR) & MSTR_LPDDR4)
        mmio_write_32(DDRC_DDR_SS_GPR0, DDRC_DDR_SS_GPR0_LPDDR4);

    umctl2_write_quasi_dynamic(ddrc, UMCTL2_DFIMISC, umctl2_dfimisc(ddrc));

    /* 3. PHY configuration */
    write_phy(&t->ddrphy_cfg);

    /* 4. Training runs */
    for (unsigned int i = 0; i < t->fsp_msg_num; i++) {
        const struct dram_fsp_msg *fsp = &t->fsp_msg[i];

        if (!dram_rate_trained(fsp->drate)) {
            LOG_DBG("Not training %u MT/s, the board doesn't use it", fsp->drate);
            continue;
        }

        if (fsp->drate != drate) {
            drate = fsp->drate;
            rc = imx8m_ddr_soc_set_clock(drate);
            if (rc != PB_OK)
                return rc;
        }

        rc = dwc_ddrphy_train(&phy, fsp);
        if (rc != PB_OK)
            return rc;
    }

    /* 5. PHY Init Engine, then wait for the impedance calibration */
    write_phy(&t->ddrphy_pie);
    dwc_ddrphy_wait_calibration(&phy);

    LOG_DBG("ddrphy calibration done");

    umctl2_dfi_init(ddrc);
    umctl2_dfi_init_done(ddrc);
    umctl2_enable_port(ddrc, 0);

    /* 6. Save settings and training results for TF-A */
    return dram_save_timing(t);
}
