/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/delay.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <plat/imx8m/imx8m.h>
#include <stdio.h>
#include <string.h>

#include "dram.h"
#include <board/lp4_timing.h>

extern uint32_t _data_region_end;
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

/* DFIMISC with dfi_frequency f and ctl_idle_en, as NXP's code uses it */
#define DFIMISC_FREQ(f)  (UMCTL2_VAL(DFIMISC_DFI_FREQUENCY, f) | DFIMISC_CTL_IDLE_EN)

__umctl2 struct dram_timing_info dram_timing_info_copy;

static int umctl2_load_training_firmware(enum fw_type type)
{
    uint32_t tmp32, i;
    unsigned long pr_to32, pr_from32;

    unsigned long imem_start = 0;
    unsigned long dmem_start = 0;
    unsigned long imem_sz = 0;
    unsigned long dmem_sz = 0;

    if (type == FW_1D_IMAGE) {
        imem_start = (unsigned long)&_binary_blobs_lpddr4_pmu_train_1d_imem_bin_start;
        dmem_start = (unsigned long)&_binary_blobs_lpddr4_pmu_train_1d_dmem_bin_start;
        imem_sz = (unsigned long)&_binary_blobs_lpddr4_pmu_train_1d_imem_bin_end;
        imem_sz -= (unsigned long)&_binary_blobs_lpddr4_pmu_train_1d_imem_bin_start;
        dmem_sz = (unsigned long)&_binary_blobs_lpddr4_pmu_train_1d_dmem_bin_end;
        dmem_sz -= (unsigned long)&_binary_blobs_lpddr4_pmu_train_1d_dmem_bin_start;
    } else {
        imem_start = (unsigned long)&_binary_blobs_lpddr4_pmu_train_2d_imem_bin_start;
        dmem_start = (unsigned long)&_binary_blobs_lpddr4_pmu_train_2d_dmem_bin_start;
        imem_sz = (unsigned long)&_binary_blobs_lpddr4_pmu_train_2d_imem_bin_end;
        imem_sz -= (unsigned long)&_binary_blobs_lpddr4_pmu_train_2d_imem_bin_start;
        dmem_sz = (unsigned long)&_binary_blobs_lpddr4_pmu_train_2d_dmem_bin_end;
        dmem_sz -= (unsigned long)&_binary_blobs_lpddr4_pmu_train_2d_dmem_bin_start;
    }

    LOG_INFO("Loading image from %lx, sz %lu bytes", imem_start, imem_sz);

    pr_from32 = imem_start;
    pr_to32 = DDRPHY_CSR_ADDR(DDRPHY_IMEM);

    for (i = 0x0; i < imem_sz;) {
        tmp32 = mmio_read_32(pr_from32);
        mmio_write_16(pr_to32, tmp32 & 0x0000ffff);
        pr_to32 += 4;
        mmio_write_16(pr_to32, (tmp32 >> 16) & 0x0000ffff);
        pr_to32 += 4;
        pr_from32 += 4;
        i += 4;
    }

    pr_from32 = dmem_start;
    pr_to32 = DDRPHY_CSR_ADDR(DDRPHY_DMEM);

    for (i = 0x0; i < dmem_sz;) {
        tmp32 = mmio_read_32(pr_from32);
        mmio_write_16(pr_to32, tmp32 & 0x0000ffff);
        pr_to32 += 4;
        mmio_write_16(pr_to32, (tmp32 >> 16) & 0x0000ffff);
        pr_to32 += 4;
        pr_from32 += 4;
        i += 4;
    }

    pr_from32 = imem_start;
    pr_to32 = DDRPHY_CSR_ADDR(DDRPHY_IMEM);

    for (i = 0x0; i < imem_sz;) {
        tmp32 = (mmio_read_16(pr_to32) & 0x0000ffff);
        pr_to32 += 4;
        tmp32 += ((mmio_read_16(pr_to32) & 0x0000ffff) << 16);

        if (tmp32 != mmio_read_32(pr_from32))
            return -PB_ERR_IO;

        pr_from32 += 4;
        pr_to32 += 4;
        i += 4;
    }

    pr_from32 = dmem_start;
    pr_to32 = DDRPHY_CSR_ADDR(DDRPHY_DMEM);

    for (i = 0x0; i < dmem_sz;) {
        tmp32 = (mmio_read_16(pr_to32) & 0x0000ffff);
        pr_to32 += 4;
        tmp32 += ((mmio_read_16(pr_to32) & 0x0000ffff) << 16);

        if (tmp32 != mmio_read_32(pr_from32))
            return -PB_ERR_IO;

        pr_from32 += 4;
        pr_to32 += 4;
        i += 4;
    }

    return PB_OK;
}

static void umctl2_poll_pmu_message_ready(void)
{
    volatile unsigned int reg;

    do {
        reg = dwc_ddrphy_apb_rd(DDRPHY_UCTSHADOWREGS);
    } while (reg & UCTSHADOWREGS_UCTWRITEPROTSHADOW);
}

static void umctl2_ack_pmu_message_recieve(void)
{
    volatile unsigned int reg;

    dwc_ddrphy_apb_wr(DDRPHY_DCTWRITEPROT, DCTWRITEPROT_ACK);

    do {
        reg = dwc_ddrphy_apb_rd(DDRPHY_UCTSHADOWREGS);
    } while (!(reg & UCTSHADOWREGS_UCTWRITEPROTSHADOW));

    dwc_ddrphy_apb_wr(DDRPHY_DCTWRITEPROT, DCTWRITEPROT_RELEASE);
}

static unsigned int umctl2_get_mail(void)
{
    volatile unsigned int reg;
    umctl2_poll_pmu_message_ready();

    reg = dwc_ddrphy_apb_rd(DDRPHY_UCTWRITEONLYSHADOW);

    umctl2_ack_pmu_message_recieve();

    return reg;
}

static unsigned int umctl2_get_stream_message(void)
{
    volatile unsigned int reg, reg2;

    umctl2_poll_pmu_message_ready();

    reg = dwc_ddrphy_apb_rd(DDRPHY_UCTWRITEONLYSHADOW);

    reg2 = dwc_ddrphy_apb_rd(DDRPHY_UCTDATWRITEONLYSHADOW);

    reg2 = (reg2 << 16) | reg;

    umctl2_ack_pmu_message_recieve();

    return reg2;
}

static void umctl2_decode_streaming_message(void)
{
    unsigned int string_index, arg;
    unsigned int i = 0;

    string_index = umctl2_get_stream_message();

    while (i < (string_index & 0xffff)) {
        arg = umctl2_get_stream_message();
        LOG_DBG("    arg[%d] = 0x%x", i, arg);
        i++;
    }
}

static int umctl2_wait_ddrphy_training_complete(void)
{
    volatile unsigned int mail;

    while (1) {
        mail = umctl2_get_mail();

        if (mail == DDRPHY_MAIL_STREAMING_MSG) {
            umctl2_decode_streaming_message();
        } else if (mail == DDRPHY_MAIL_TRAINING_DONE) {
            LOG_INFO("Training PASS");
            return PB_OK;
        } else if (mail == DDRPHY_MAIL_TRAINING_FAILED) {
            LOG_ERR("Training FAILED");
            return -PB_ERR_IO;
        }
    }
}

static void dram_pll_init(uint32_t data_rate)
{
    volatile uint32_t val;

    /* Bypass */
    mmio_clrsetbits_32(DRAM_PLL_CFG0, 0, SSCG_PLL_BYPASS1_MASK);
    mmio_clrsetbits_32(DRAM_PLL_CFG0, 0, SSCG_PLL_BYPASS2_MASK);

    switch (data_rate) {
    case 3200: {
        val = mmio_read_32(DRAM_PLL_CFG2);
        val &= ~(SSCG_PLL_OUTPUT_DIV_VAL_MASK | SSCG_PLL_FEEDBACK_DIV_F2_MASK |
                 SSCG_PLL_FEEDBACK_DIV_F1_MASK | SSCG_PLL_REF_DIVR2_MASK);
        val |= SSCG_PLL_OUTPUT_DIV_VAL(0);
        val |= SSCG_PLL_FEEDBACK_DIV_F2_VAL(11);
        val |= SSCG_PLL_FEEDBACK_DIV_F1_VAL(39);
        val |= SSCG_PLL_REF_DIVR2_VAL(29);
        mmio_write_32(DRAM_PLL_CFG2, val);
    } break;
    case 667: {
        val = mmio_read_32(DRAM_PLL_CFG2);
        val &= ~(SSCG_PLL_OUTPUT_DIV_VAL_MASK | SSCG_PLL_FEEDBACK_DIV_F2_MASK |
                 SSCG_PLL_FEEDBACK_DIV_F1_MASK | SSCG_PLL_REF_DIVR2_MASK);
        val |= SSCG_PLL_OUTPUT_DIV_VAL(3);
        val |= SSCG_PLL_FEEDBACK_DIV_F2_VAL(8);
        val |= SSCG_PLL_FEEDBACK_DIV_F1_VAL(45);
        val |= SSCG_PLL_REF_DIVR2_VAL(30);
        mmio_write_32(DRAM_PLL_CFG2, val);
    } break;
    default:
        break;
    }

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
}

int umctl2_init(void)
{
    volatile uint32_t tmp;
    int rc;

    /* Hold the controller and PHY in reset */
    mmio_write_32(SRC_DDRC2_RCR_ADDR, RCR_DOMAINS | SRC_DDRC_RCR_ALL_RESETS);
    mmio_write_32(SRC_DDRC_RCR_ADDR, RCR_DOMAINS | SRC_DDRC_RCR_ALL_RESETS);
    mmio_write_32(SRC_DDRC2_RCR_ADDR, RCR_DOMAINS);

    /* Power up the DDR domain */
    mmio_write_32(GPC_PGC_CPU_0_1_MAPPING, 0x0000ffff);
    mmio_clrsetbits_32(GPC_PU_PGC_SW_PUP_REQ, 0, GPC_PU_PGC_SW_PUP_REQ_DDR1);

    dram_pll_init(3200);

    /* Release the APB reset and PHY power ok, so the controller can be configured */
    mmio_write_32(SRC_DDRC_RCR_ADDR,
                  RCR_DOMAINS | SRC_DDRC_RCR_PHY_RESET | SRC_DDRC_RCR_CORE_RESET_N);

    /* Load phy timing config */

    for (uint32_t n = 0; n < ARRAY_SIZE(umctl2_ddrc_cfg); n++) {
        mmio_write_32(umctl2_ddrc_cfg[n].reg, umctl2_ddrc_cfg[n].val);
    }

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

    do {
        tmp = mmio_read_32(DDRC_STAT(0));
    } while ((tmp & STAT_SELFREF_MASK) != STAT_SELFREF_BY_SW);

    mmio_write_32(DDRC_DDR_SS_GPR0, DDRC_DDR_SS_GPR0_LPDDR4);

    mmio_write_32(DDRC_SWCTL(0), 0);

    tmp = mmio_read_32(DDRC_MSTR2(0));

    if (tmp == 0x2)
        mmio_write_32(DDRC_DFIMISC(0), DFIMISC_FREQ(2));
    else if (tmp == 0x1)
        mmio_write_32(DDRC_DFIMISC(0), DFIMISC_FREQ(1));
    else
        mmio_write_32(DDRC_DFIMISC(0), DFIMISC_FREQ(0));

    mmio_write_32(DDRC_SWCTL(0), SWCTL_SW_DONE);

    for (uint32_t n = 0; n < ARRAY_SIZE(lpddr4_ddrphy_cfg); n++) {
        dwc_ddrphy_apb_wr(lpddr4_ddrphy_cfg[n].reg, lpddr4_ddrphy_cfg[n].val);
    }

    /* load the dram training firmware image FSP 0 3200 Mt/s --------------- */

    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_APB);

    rc = umctl2_load_training_firmware(FW_1D_IMAGE);
    if (rc != PB_OK) {
        LOG_ERR("Training firmware verify failed");
        return rc;
    }

    for (uint32_t n = 0; n < ARRAY_SIZE(lpddr4_fsp0_cfg); n++) {
        dwc_ddrphy_apb_wr(lpddr4_fsp0_cfg[n].reg, lpddr4_fsp0_cfg[n].val);
    }

    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_PMU);
    dwc_ddrphy_apb_wr(DDRPHY_MICRORESET, MICRORESET_HOLD);
    dwc_ddrphy_apb_wr(DDRPHY_MICRORESET, MICRORESET_STALL);
    dwc_ddrphy_apb_wr(DDRPHY_MICRORESET, MICRORESET_RUN);

    /* Wait for the training firmware to complete */
    rc = umctl2_wait_ddrphy_training_complete();
    if (rc != PB_OK)
        return rc;

    /* Halt the microcontroller. */
    dwc_ddrphy_apb_wr(DDRPHY_MICRORESET, MICRORESET_STALL);

    /* Read the Message Block results */
    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_APB);
    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_PMU);

    /* End of FSP 0 -------------------------------------------------------- */

    /* load the dram training firmware image FSP 1 667 Mt/s ---------------- */
    dram_pll_init(667);
    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_APB);

    rc = umctl2_load_training_firmware(FW_1D_IMAGE);
    if (rc != PB_OK) {
        LOG_ERR("Training firmware verify failed");
        return rc;
    }

    for (uint32_t n = 0; n < ARRAY_SIZE(lpddr4_fsp1_cfg); n++) {
        dwc_ddrphy_apb_wr(lpddr4_fsp1_cfg[n].reg, lpddr4_fsp1_cfg[n].val);
    }

    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_PMU);
    dwc_ddrphy_apb_wr(DDRPHY_MICRORESET, MICRORESET_HOLD);
    dwc_ddrphy_apb_wr(DDRPHY_MICRORESET, MICRORESET_STALL);
    dwc_ddrphy_apb_wr(DDRPHY_MICRORESET, MICRORESET_RUN);

    /* Wait for the training firmware to complete */
    rc = umctl2_wait_ddrphy_training_complete();
    if (rc != PB_OK)
        return rc;

    /* Halt the microcontroller. */
    dwc_ddrphy_apb_wr(DDRPHY_MICRORESET, MICRORESET_STALL);

    /* Read the Message Block results */
    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_APB);
    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_PMU);

    /* End of FSP 1 -------------------------------------------------------- */

    /* load the dram training firmware image FSP 0 2D 3200 Mt/s ------------ */
    dram_pll_init(3200);
    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_APB);

    rc = umctl2_load_training_firmware(FW_2D_IMAGE);
    if (rc != PB_OK) {
        LOG_ERR("Training firmware verify failed");
        return rc;
    }

    for (uint32_t n = 0; n < ARRAY_SIZE(lpddr4_fsp0_2d_cfg); n++) {
        dwc_ddrphy_apb_wr(lpddr4_fsp0_2d_cfg[n].reg, lpddr4_fsp0_2d_cfg[n].val);
    }

    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_PMU);
    dwc_ddrphy_apb_wr(DDRPHY_MICRORESET, MICRORESET_HOLD);
    dwc_ddrphy_apb_wr(DDRPHY_MICRORESET, MICRORESET_STALL);
    dwc_ddrphy_apb_wr(DDRPHY_MICRORESET, MICRORESET_RUN);

    /* Wait for the training firmware to complete */
    rc = umctl2_wait_ddrphy_training_complete();
    if (rc != PB_OK)
        return rc;

    /* Halt the microcontroller. */
    dwc_ddrphy_apb_wr(DDRPHY_MICRORESET, MICRORESET_STALL);

    /* Read the Message Block results */
    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_APB);
    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_PMU);

    /* End of FSP 0 2D ----------------------------------------------------- */

    /* Load PHY Init Engine Image */
    for (uint32_t n = 0; n < ARRAY_SIZE(lpddr4_phy_pie); n++) {
        dwc_ddrphy_apb_wr(lpddr4_phy_pie[n].reg, lpddr4_phy_pie[n].val);
    }

    /*
     * step14 CalBusy.0 =1, indicates the calibrator is actively
     * calibrating. Wait Calibrating done.
     */

    do {
        tmp = dwc_ddrphy_apb_rd(DDRPHY_CALBUSY);
    } while (tmp & CALBUSY_BUSY);

    LOG_DBG("ddrphy calibration done");

    mmio_write_32(DDRC_SWCTL(0), 0);

    tmp = mmio_read_32(DDRC_MSTR2(0));

    if (tmp == 0x2)
        mmio_write_32(DDRC_DFIMISC(0), DFIMISC_FREQ(2) | DFIMISC_DFI_INIT_START);
    else if (tmp == 0x1)
        mmio_write_32(DDRC_DFIMISC(0), DFIMISC_FREQ(1) | DFIMISC_DFI_INIT_START);
    else
        mmio_write_32(DDRC_DFIMISC(0), DFIMISC_FREQ(0) | DFIMISC_DFI_INIT_START);

    mmio_write_32(DDRC_SWCTL(0), SWCTL_SW_DONE);

    /* step18 wait DFISTAT.dfi_init_complete to 1 */
    do {
        tmp = mmio_read_32(DDRC_DFISTAT(0));
    } while (!(tmp & DFISTAT_DFI_INIT_COMPLETE));

    mmio_write_32(DDRC_SWCTL(0), 0);

    tmp = mmio_read_32(DDRC_MSTR2(0));

    if (tmp == 0x2) {
        mmio_write_32(DDRC_DFIMISC(0), DFIMISC_FREQ(2));
        /* set DFIMISC.dfi_init_complete_en again */
        mmio_write_32(DDRC_DFIMISC(0), DFIMISC_FREQ(2) | DFIMISC_DFI_INIT_COMPLETE_EN);
    } else if (tmp == 0x1) {
        mmio_write_32(DDRC_DFIMISC(0), DFIMISC_FREQ(1));
        /* set DFIMISC.dfi_init_complete_en again */
        mmio_write_32(DDRC_DFIMISC(0), DFIMISC_FREQ(1) | DFIMISC_DFI_INIT_COMPLETE_EN);
    } else {
        /* clear DFIMISC.dfi_init_complete_en */
        mmio_write_32(DDRC_DFIMISC(0), DFIMISC_FREQ(0));
        /* set DFIMISC.dfi_init_complete_en again */
        mmio_write_32(DDRC_DFIMISC(0), DFIMISC_FREQ(0) | DFIMISC_DFI_INIT_COMPLETE_EN);
    }

    /* step23 [5]selfref_sw=0; */
    mmio_write_32(DDRC_PWRCTL(0), PWRCTL_EN_DFI_DRAM_CLK_DISABLE);
    /* step24 sw_done=1 */
    mmio_write_32(DDRC_SWCTL(0), SWCTL_SW_DONE);

    mmio_write_32(DDRC_DFIPHYMSTR(0), DFIPHYMSTR_DFI_PHYMSTR_EN);
    /* step25 wait SWSTAT.sw_done_ack to 1 */
    do {
        tmp = mmio_read_32(DDRC_SWSTAT(0));
    } while (!(tmp & SWSTAT_SW_DONE_ACK));

    mmio_write_32(DDRC_DFIPHYMSTR(0), DFIPHYMSTR_DFI_PHYMSTR_EN);

    /* wait STAT.operating_mode to normal state (bits [1:0], as in NXP's code) */
    do {
        tmp = mmio_read_32(DDRC_STAT(0));
    } while ((tmp & 0x3) != STAT_OPERATING_MODE_NORMAL);

    /* step26 */
    mmio_write_32(DDRC_RFSHCTL3(0), UMCTL2_VAL(RFSHCTL3_REFRESH_MODE, 1));

    /* enable port 0 */
    mmio_write_32(DDRC_PCTRL_0(0), PCTRL_PORT_EN);

    /* Copy all settings and results to 0x40000000. This is needed for
     * ATF and run time changes of transfer speed / voltage */

    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_APB);
    dwc_ddrphy_apb_wr(DDRPHY_UCCLKHCLKENABLES, UCCLKHCLKENABLES_HCLKEN | UCCLKHCLKENABLES_UCCLKEN);

    for (uint32_t n = 0; n < dram_timing.ddrphy_trained_csr_num; n++) {
        dram_timing.ddrphy_trained_csr[n].val =
            dwc_ddrphy_apb_rd(dram_timing.ddrphy_trained_csr[n].reg);
    }

    dwc_ddrphy_apb_wr(DDRPHY_UCCLKHCLKENABLES, UCCLKHCLKENABLES_HCLKEN);
    dwc_ddrphy_apb_wr(DDRPHY_MICROCONTMUXSEL, MICROCONTMUXSEL_PMU);

    struct dram_timing_info *dt = (struct dram_timing_info *)&dram_timing_info_copy;

    struct dram_cfg_param *c = (struct dram_cfg_param *)(((uint8_t *)&dram_timing_info_copy) +
                                                         sizeof(struct dram_timing_info));

    dt->ddrc_cfg_num = dram_timing.ddrc_cfg_num;
    dt->ddrphy_cfg_num = dram_timing.ddrphy_cfg_num;
    dt->ddrphy_trained_csr_num = dram_timing.ddrphy_trained_csr_num;
    dt->ddrphy_pie_num = dram_timing.ddrphy_pie_num;

    for (uint32_t n = 0; n < 4; n++) {
        dt->fsp_table[n] = dram_timing.fsp_table[n];
    }

    dt->ddrc_cfg = c;

    for (uint32_t n = 0; n < dram_timing.ddrc_cfg_num; n++) {
        c->reg = dram_timing.ddrc_cfg[n].reg;
        c->val = dram_timing.ddrc_cfg[n].val;
        c++;
    }

    dt->ddrphy_cfg = c;

    for (uint32_t n = 0; n < dram_timing.ddrphy_cfg_num; n++) {
        c->reg = dram_timing.ddrphy_cfg[n].reg;
        c->val = dram_timing.ddrphy_cfg[n].val;
        c++;
    }

    dt->ddrphy_trained_csr = c;

    for (uint32_t n = 0; n < dram_timing.ddrphy_trained_csr_num; n++) {
        c->reg = dram_timing.ddrphy_trained_csr[n].reg;
        c->val = dram_timing.ddrphy_trained_csr[n].val;
        c++;
    }

    dt->ddrphy_pie = c;

    for (uint32_t n = 0; n < dram_timing.ddrphy_pie_num; n++) {
        c->reg = dram_timing.ddrphy_pie[n].reg;
        c->val = dram_timing.ddrphy_pie[n].val;
        c++;
    }

    return PB_OK;
}
