/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
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

/*
 * Read back the trained PHY CSRs, and copy all timing data to the start of
 * DRAM for TF-A (see struct dram_timing_info).
 */
static int dram_save_timing(void)
{
    struct dram_timing_info *dt = &dram_timing_info_copy.info;
    struct dram_cfg_param *pos = dram_timing_info_copy.cfg;
    size_t n = (size_t)dram_timing.ddrc_cfg_num + dram_timing.ddrphy_cfg_num +
               dram_timing.ddrphy_trained_csr_num + dram_timing.ddrphy_pie_num;

    if (n > ARRAY_SIZE(dram_timing_info_copy.cfg)) {
        LOG_ERR("DRAM timing does not fit in the TF-A copy (%zu entries)", n);
        return -PB_ERR_MEM;
    }

    dwc_ddrphy_read_csrs(&phy, dram_timing.ddrphy_trained_csr, dram_timing.ddrphy_trained_csr_num);

    dt->ddrc_cfg_num = dram_timing.ddrc_cfg_num;
    dt->ddrphy_cfg_num = dram_timing.ddrphy_cfg_num;
    dt->ddrphy_trained_csr_num = dram_timing.ddrphy_trained_csr_num;
    dt->ddrphy_pie_num = dram_timing.ddrphy_pie_num;
    memcpy(dt->fsp_table, dram_timing.fsp_table, sizeof(dt->fsp_table));

    copy_cfg(&pos, &dt->ddrc_cfg, dram_timing.ddrc_cfg, dram_timing.ddrc_cfg_num);
    copy_cfg(&pos, &dt->ddrphy_cfg, dram_timing.ddrphy_cfg, dram_timing.ddrphy_cfg_num);
    copy_cfg(&pos,
             &dt->ddrphy_trained_csr,
             dram_timing.ddrphy_trained_csr,
             dram_timing.ddrphy_trained_csr_num);
    copy_cfg(&pos, &dt->ddrphy_pie, dram_timing.ddrphy_pie, dram_timing.ddrphy_pie_num);

    return PB_OK;
}

int imx8m_ddr_init(void)
{
    const struct dram_timing_info *t = &dram_timing;
    const uintptr_t ddrc = DDRC_IPS_BASE_ADDR(0);
    unsigned int drate = t->fsp_msg[0].drate;
    int rc;

    /* 1. Hold the controller and PHY in reset, power up, clock */
    imx8m_ddr_soc_reset();

    mmio_write_32(GPC_PGC_CPU_0_1_MAPPING, 0x0000ffff);
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
    dwc_ddrphy_write_cfg(&phy, t->ddrphy_cfg, t->ddrphy_cfg_num);

    /* 4. Training runs */
    for (unsigned int i = 0; i < t->fsp_msg_num; i++) {
        const struct dram_fsp_msg *fsp = &t->fsp_msg[i];

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
    dwc_ddrphy_write_cfg(&phy, t->ddrphy_pie, t->ddrphy_pie_num);
    dwc_ddrphy_wait_calibration(&phy);

    LOG_DBG("ddrphy calibration done");

    umctl2_dfi_init(ddrc);
    umctl2_dfi_init_done(ddrc);
    umctl2_enable_port(ddrc, 0);

    /* 6. Save settings and training results for TF-A */
    return dram_save_timing();
}
