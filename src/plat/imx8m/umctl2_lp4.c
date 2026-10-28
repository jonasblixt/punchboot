/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * i.MX8M LPDDR4 init: uMCTL2 controller (umctl2.h) and DWC LPDDR4 PHY
 * with firmware training (ddrphy.h). The sequence follows NXP's reference
 * code:
 *
 *  1. Reset and power up the DDR subsystem, start the DRAM PLL.
 *  2. Program the controller from the board table, release the resets
 *     and put the (not yet initialized) SDRAM in self refresh.
 *  3. Program the PHY from the board table.
 *  4. For each training run (frequency set point, 1D/2D) in the board's
 *     fsp_msg table: set the PLL, load the firmware and its message block,
 *     run the PMU and wait for its result.
 *  5. Load the PHY Init Engine (PIE) image, start the DFI init and leave
 *     self refresh.
 *  6. Save the timing data and the trained PHY CSRs for TF-A.
 *
 * The register trace of umctl2_init() is checked by
 * tests/unit/test_imx8m_ddr.c.
 */

#include <pb/delay.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <plat/imx8m/imx8m.h>
#include <stdio.h>
#include <string.h>

#include <plat/imx8m/dram.h>

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

/* STAT in self refresh entered through PWRCTL.selfref_sw, as in NXP's code */
#define STAT_SELFREF_MASK \
    (STAT_SELFREF_STATE_MASK | STAT_SELFREF_TYPE_MASK | STAT_OPERATING_MODE_MASK)
#define STAT_SELFREF_BY_SW                                                  \
    (UMCTL2_VAL(STAT_SELFREF_STATE, 2) | UMCTL2_VAL(STAT_SELFREF_TYPE, 2) | \
     UMCTL2_VAL(STAT_OPERATING_MODE, 3))

/* PMU: held in reset, stalled, running */
#define MICRORESET_HOLD  (MICRORESET_RESETTOMICRO | MICRORESET_STALLTOMICRO)
#define MICRORESET_STALL MICRORESET_STALLTOMICRO
#define MICRORESET_RUN   0

/* Training firmware images, linked in from the board's blobs */
struct fw_image {
    const uint32_t *imem;
    const uint32_t *imem_end;
    const uint32_t *dmem;
    const uint32_t *dmem_end;
};

static const struct fw_image fw_images[] = {
    [FW_1D_IMAGE] = {
        .imem = &_binary_blobs_lpddr4_pmu_train_1d_imem_bin_start,
        .imem_end = &_binary_blobs_lpddr4_pmu_train_1d_imem_bin_end,
        .dmem = &_binary_blobs_lpddr4_pmu_train_1d_dmem_bin_start,
        .dmem_end = &_binary_blobs_lpddr4_pmu_train_1d_dmem_bin_end,
    },
    [FW_2D_IMAGE] = {
        .imem = &_binary_blobs_lpddr4_pmu_train_2d_imem_bin_start,
        .imem_end = &_binary_blobs_lpddr4_pmu_train_2d_imem_bin_end,
        .dmem = &_binary_blobs_lpddr4_pmu_train_2d_dmem_bin_start,
        .dmem_end = &_binary_blobs_lpddr4_pmu_train_2d_dmem_bin_end,
    },
};

/* DRAM PLL (SSCG, 25 MHz reference) settings per data rate */
struct dram_pll_setting {
    unsigned int drate; /* MT/s */
    uint8_t ref_divr2;
    uint8_t feedback_div_f1;
    uint8_t feedback_div_f2;
    uint8_t output_div;
};

static const struct dram_pll_setting dram_pll_settings[] = {
    { .drate = 3200,
      .ref_divr2 = 29,
      .feedback_div_f1 = 39,
      .feedback_div_f2 = 11,
      .output_div = 0 },
    { .drate = 667, .ref_divr2 = 30, .feedback_div_f1 = 45, .feedback_div_f2 = 8, .output_div = 3 },
};

__umctl2 struct dram_timing_info dram_timing_info_copy;

static void write_cfg(const struct dram_cfg_param *cfg, unsigned int n)
{
    for (unsigned int i = 0; i < n; i++)
        mmio_write_32(cfg[i].reg, cfg[i].val);
}

static void ddrphy_write_cfg(const struct dram_cfg_param *cfg, unsigned int n)
{
    for (unsigned int i = 0; i < n; i++)
        dwc_ddrphy_apb_wr(cfg[i].reg, cfg[i].val);
}

/* Write a firmware image to PMU memory at 'csr', two CSRs per word, and read it back */
static int ddrphy_load_mem(uint32_t csr, const uint32_t *start, const uint32_t *end)
{
    uintptr_t to = DDRPHY_CSR_ADDR(csr);

    for (const uint32_t *p = start; p < end; p++) {
        mmio_write_16(to, *p & 0xffff);
        mmio_write_16(to + 4, *p >> 16);
        to += 8;
    }

    to = DDRPHY_CSR_ADDR(csr);

    for (const uint32_t *p = start; p < end; p++) {
        uint32_t word = mmio_read_16(to) | ((uint32_t)mmio_read_16(to + 4) << 16);

        if (word != *p)
            return -PB_ERR_IO;
        to += 8;
    }

    return PB_OK;
}

static int ddrphy_load_training_firmware(enum fw_type type)
{
    const struct fw_image *fw = &fw_images[type];
    int rc;

    LOG_INFO("Loading image from %p, sz %lu bytes",
             (void *)fw->imem,
             (unsigned long)((uintptr_t)fw->imem_end - (uintptr_t)fw->imem));

    rc = ddrphy_load_mem(DDRPHY_IMEM, fw->imem, fw->imem_end);
    if (rc == PB_OK)
        rc = ddrphy_load_mem(DDRPHY_DMEM, fw->dmem, fw->dmem_end);

    return rc;
}

/* Mailbox: wait for a message from the PMU, read it, then ack it */
static void ddrphy_wait_mail(void)
{
    while (dwc_ddrphy_apb_rd(DDRPHY_UCTSHADOWREGS) & UCTSHADOWREGS_UCTWRITEPROTSHADOW)
        ;
}

static void ddrphy_ack_mail(void)
{
    dwc_ddrphy_apb_wr(DDRPHY_DCTWRITEPROT, DCTWRITEPROT_ACK);

    while (!(dwc_ddrphy_apb_rd(DDRPHY_UCTSHADOWREGS) & UCTSHADOWREGS_UCTWRITEPROTSHADOW))
        ;

    dwc_ddrphy_apb_wr(DDRPHY_DCTWRITEPROT, DCTWRITEPROT_RELEASE);
}

/* A major message: 16 bits */
static uint32_t ddrphy_get_mail(void)
{
    uint32_t mail;

    ddrphy_wait_mail();
    mail = dwc_ddrphy_apb_rd(DDRPHY_UCTWRITEONLYSHADOW);
    ddrphy_ack_mail();

    return mail;
}

/* A word of a streaming message: 32 bits */
static uint32_t ddrphy_get_stream_word(void)
{
    uint32_t lo, hi;

    ddrphy_wait_mail();
    lo = dwc_ddrphy_apb_rd(DDRPHY_UCTWRITEONLYSHADOW);
    hi = dwc_ddrphy_apb_rd(DDRPHY_UCTDATWRITEONLYSHADOW);
    ddrphy_ack_mail();

    return (hi << 16) | lo;
}

/*
 * A streaming message is a string index (into a string table that comes
 * with the firmware) whose low 16 bits are the number of arguments that
 * follow.
 */
static void ddrphy_decode_streaming_message(void)
{
    uint32_t string_index = ddrphy_get_stream_word();

    for (unsigned int i = 0; i < (string_index & 0xffff); i++) {
        uint32_t arg = ddrphy_get_stream_word();
        LOG_DBG("    arg[%u] = 0x%x", i, arg);
    }
}

static int ddrphy_wait_training_complete(void)
{
    while (1) {
        uint32_t mail = ddrphy_get_mail();

        if (mail == DDRPHY_MAIL_STREAMING_MSG) {
            ddrphy_decode_streaming_message();
        } else if (mail == DDRPHY_MAIL_TRAINING_DONE) {
            LOG_INFO("Training PASS");
            return PB_OK;
        } else if (mail == DDRPHY_MAIL_TRAINING_FAILED) {
            LOG_ERR("Training FAILED");
            return -PB_ERR_IO;
        }
    }
}

/* One training run: firmware, message block, run the PMU, wait for the result */
static int ddrphy_train(const struct dram_fsp_msg *fsp)
{
    int rc;

    LOG_DBG("Training %u MT/s, %s", fsp->drate, fsp->fw_type == FW_2D_IMAGE ? "2D" : "1D");

    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_APB);

    rc = ddrphy_load_training_firmware(fsp->fw_type);
    if (rc != PB_OK) {
        LOG_ERR("Training firmware verify failed");
        return rc;
    }

    /* Message block (DMEM) */
    ddrphy_write_cfg(fsp->fsp_cfg, fsp->fsp_cfg_num);

    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_PMU);
    dwc_ddrphy_apb_wr(DDRPHY_MICRORESET, MICRORESET_HOLD);
    dwc_ddrphy_apb_wr(DDRPHY_MICRORESET, MICRORESET_STALL);
    dwc_ddrphy_apb_wr(DDRPHY_MICRORESET, MICRORESET_RUN);

    rc = ddrphy_wait_training_complete();
    if (rc != PB_OK)
        return rc;

    /* Halt the microcontroller */
    dwc_ddrphy_apb_wr(DDRPHY_MICRORESET, MICRORESET_STALL);

    /* The message block results could be read here, with APB access */
    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_APB);
    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_PMU);

    return PB_OK;
}

static int dram_pll_init(unsigned int drate)
{
    const struct dram_pll_setting *pll = NULL;
    uint32_t val;

    for (unsigned int i = 0; i < ARRAY_SIZE(dram_pll_settings); i++) {
        if (dram_pll_settings[i].drate == drate)
            pll = &dram_pll_settings[i];
    }

    if (pll == NULL) {
        LOG_ERR("No DRAM PLL setting for %u MT/s", drate);
        return -PB_ERR_NOT_SUPPORTED;
    }

    /* Bypass */
    mmio_clrsetbits_32(DRAM_PLL_CFG0, 0, SSCG_PLL_BYPASS1_MASK);
    mmio_clrsetbits_32(DRAM_PLL_CFG0, 0, SSCG_PLL_BYPASS2_MASK);

    val = mmio_read_32(DRAM_PLL_CFG2);
    val &= ~(SSCG_PLL_OUTPUT_DIV_VAL_MASK | SSCG_PLL_FEEDBACK_DIV_F2_MASK |
             SSCG_PLL_FEEDBACK_DIV_F1_MASK | SSCG_PLL_REF_DIVR2_MASK);
    val |= SSCG_PLL_OUTPUT_DIV_VAL(pll->output_div);
    val |= SSCG_PLL_FEEDBACK_DIV_F2_VAL(pll->feedback_div_f2);
    val |= SSCG_PLL_FEEDBACK_DIV_F1_VAL(pll->feedback_div_f1);
    val |= SSCG_PLL_REF_DIVR2_VAL(pll->ref_divr2);
    mmio_write_32(DRAM_PLL_CFG2, val);

    /* Clear power down bit */
    mmio_clrsetbits_32(DRAM_PLL_CFG0, SSCG_PLL_PD_MASK, 0);
    /* Enable the DRAM PLL output */
    mmio_clrsetbits_32(DRAM_PLL_CFG0, 0, SSCG_PLL_DRAM_PLL_CLKE_MASK);

    /* Clear bypass */
    mmio_clrsetbits_32(DRAM_PLL_CFG0, SSCG_PLL_BYPASS1_MASK, 0);
    pb_delay_ms(1);
    mmio_clrsetbits_32(DRAM_PLL_CFG0, SSCG_PLL_BYPASS2_MASK, 0);

    /* Wait lock */
    while (!(mmio_read_32(DRAM_PLL_CFG0) & SSCG_PLL_LOCK_MASK))
        ;

    return PB_OK;
}

/* DFIMISC with dfi_frequency matching MSTR2.target_frequency, and ctl_idle_en */
static uint32_t ddrc_dfimisc(void)
{
    uint32_t freq = UMCTL2_GET(MSTR2_TARGET_FREQUENCY, mmio_read_32(DDRC_MSTR2(0)));

    return UMCTL2_VAL(DFIMISC_DFI_FREQUENCY, freq) | DFIMISC_CTL_IDLE_EN;
}

/* Write a quasi-dynamic register, see SWCTL in umctl2.h */
static void ddrc_write_quasi_dynamic(uintptr_t reg, uint32_t val)
{
    mmio_write_32(DDRC_SWCTL(0), 0);
    mmio_write_32(reg, val);
    mmio_write_32(DDRC_SWCTL(0), SWCTL_SW_DONE);
}

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
static void dram_save_timing(void)
{
    struct dram_timing_info *dt = &dram_timing_info_copy;
    struct dram_cfg_param *pos = (struct dram_cfg_param *)(dt + 1);

    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_APB);
    dwc_ddrphy_apb_wr(DDRPHY_UCCLKHCLKENABLES, UCCLKHCLKENABLES_HCLKEN | UCCLKHCLKENABLES_UCCLKEN);

    for (unsigned int n = 0; n < dram_timing.ddrphy_trained_csr_num; n++) {
        dram_timing.ddrphy_trained_csr[n].val =
            dwc_ddrphy_apb_rd(dram_timing.ddrphy_trained_csr[n].reg);
    }

    dwc_ddrphy_apb_wr(DDRPHY_UCCLKHCLKENABLES, UCCLKHCLKENABLES_HCLKEN);
    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_PMU);

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
}

int umctl2_init(void)
{
    const struct dram_timing_info *t = &dram_timing;
    unsigned int drate = t->fsp_msg[0].drate;
    int rc;

    /* 1. Hold the controller and PHY in reset */
    mmio_write_32(SRC_DDRC2_RCR_ADDR, RCR_DOMAINS | SRC_DDRC_RCR_ALL_RESETS);
    mmio_write_32(SRC_DDRC_RCR_ADDR, RCR_DOMAINS | SRC_DDRC_RCR_ALL_RESETS);
    mmio_write_32(SRC_DDRC2_RCR_ADDR, RCR_DOMAINS);

    /* Power up the DDR domain */
    mmio_write_32(GPC_PGC_CPU_0_1_MAPPING, 0x0000ffff);
    mmio_clrsetbits_32(GPC_PU_PGC_SW_PUP_REQ, 0, GPC_PU_PGC_SW_PUP_REQ_DDR1);

    rc = dram_pll_init(drate);
    if (rc != PB_OK)
        return rc;

    /* 2. Release the APB reset and PHY power ok, so the controller can be configured */
    mmio_write_32(SRC_DDRC_RCR_ADDR,
                  RCR_DOMAINS | SRC_DDRC_RCR_PHY_RESET | SRC_DDRC_RCR_CORE_RESET_N);

    write_cfg(t->ddrc_cfg, t->ddrc_cfg_num);

    /* Release the core reset, then the PHY reset */
    mmio_write_32(SRC_DDRC_RCR_ADDR, RCR_DOMAINS | SRC_DDRC_RCR_PHY_RESET);
    mmio_write_32(SRC_DDRC_RCR_ADDR, RCR_DOMAINS);

    /* Allow reads/writes, but keep the SDRAM in self refresh until trained */
    mmio_write_32(DDRC_DBG1(0), 0);
    mmio_write_32(DDRC_RFSHCTL3(0),
                  UMCTL2_VAL(RFSHCTL3_REFRESH_MODE, 1) | RFSHCTL3_DIS_AUTO_REFRESH);
    mmio_write_32(DDRC_PWRCTL(0),
                  PWRCTL_DIS_CAM_DRAIN_SELFREF | PWRCTL_SELFREF_SW |
                      PWRCTL_EN_DFI_DRAM_CLK_DISABLE);

    while ((mmio_read_32(DDRC_STAT(0)) & STAT_SELFREF_MASK) != STAT_SELFREF_BY_SW)
        ;

    mmio_write_32(DDRC_DDR_SS_GPR0, DDRC_DDR_SS_GPR0_LPDDR4);

    ddrc_write_quasi_dynamic(DDRC_DFIMISC(0), ddrc_dfimisc());

    /* 3. PHY configuration */
    ddrphy_write_cfg(t->ddrphy_cfg, t->ddrphy_cfg_num);

    /* 4. Training runs */
    for (unsigned int i = 0; i < t->fsp_msg_num; i++) {
        const struct dram_fsp_msg *fsp = &t->fsp_msg[i];

        if (fsp->drate != drate) {
            drate = fsp->drate;
            rc = dram_pll_init(drate);
            if (rc != PB_OK)
                return rc;
        }

        rc = ddrphy_train(fsp);
        if (rc != PB_OK)
            return rc;
    }

    /* 5. PHY Init Engine, then wait for the impedance calibration */
    ddrphy_write_cfg(t->ddrphy_pie, t->ddrphy_pie_num);

    while (dwc_ddrphy_apb_rd(DDRPHY_CALBUSY) & CALBUSY_BUSY)
        ;

    LOG_DBG("ddrphy calibration done");

    /* DFI init */
    ddrc_write_quasi_dynamic(DDRC_DFIMISC(0), ddrc_dfimisc() | DFIMISC_DFI_INIT_START);

    while (!(mmio_read_32(DDRC_DFISTAT(0)) & DFISTAT_DFI_INIT_COMPLETE))
        ;

    /* Stop the DFI init, set dfi_init_complete_en and leave self refresh */
    mmio_write_32(DDRC_SWCTL(0), 0);
    mmio_write_32(DDRC_DFIMISC(0), ddrc_dfimisc());
    mmio_write_32(DDRC_DFIMISC(0), ddrc_dfimisc() | DFIMISC_DFI_INIT_COMPLETE_EN);
    mmio_write_32(DDRC_PWRCTL(0), PWRCTL_EN_DFI_DRAM_CLK_DISABLE);
    mmio_write_32(DDRC_SWCTL(0), SWCTL_SW_DONE);

    mmio_write_32(DDRC_DFIPHYMSTR(0), DFIPHYMSTR_DFI_PHYMSTR_EN);

    while (!(mmio_read_32(DDRC_SWSTAT(0)) & SWSTAT_SW_DONE_ACK))
        ;

    mmio_write_32(DDRC_DFIPHYMSTR(0), DFIPHYMSTR_DFI_PHYMSTR_EN);

    /* Wait for STAT.operating_mode normal (bits [1:0], as in NXP's code) */
    while ((mmio_read_32(DDRC_STAT(0)) & 0x3) != STAT_OPERATING_MODE_NORMAL)
        ;

    /* Enable auto refresh and port 0 */
    mmio_write_32(DDRC_RFSHCTL3(0), UMCTL2_VAL(RFSHCTL3_REFRESH_MODE, 1));
    mmio_write_32(DDRC_PCTRL_0(0), PCTRL_PORT_EN);

    /* 6. Save settings and training results for TF-A */
    dram_save_timing();

    return PB_OK;
}
