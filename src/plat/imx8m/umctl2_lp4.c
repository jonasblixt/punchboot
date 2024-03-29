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

#include "umctl2.h"
#include <board/lp4_timing.h>

#define IMEM_OFFSET_ADDR         0x00050000
#define DMEM_OFFSET_ADDR         0x00054000
#define DDR_TRAIN_CODE_BASE_ADDR IP2APB_DDRPHY_IPS_BASE_ADDR(0)

extern uint32_t _data_region_end;
extern uint32_t _binary_blobs_lpddr4_pmu_train_1d_imem_bin_start;
extern uint32_t _binary_blobs_lpddr4_pmu_train_1d_dmem_bin_start;
extern uint32_t _binary_blobs_lpddr4_pmu_train_2d_imem_bin_start;
extern uint32_t _binary_blobs_lpddr4_pmu_train_2d_dmem_bin_start;
extern uint32_t _binary_blobs_lpddr4_pmu_train_1d_imem_bin_end;
extern uint32_t _binary_blobs_lpddr4_pmu_train_1d_dmem_bin_end;
extern uint32_t _binary_blobs_lpddr4_pmu_train_2d_imem_bin_end;
extern uint32_t _binary_blobs_lpddr4_pmu_train_2d_dmem_bin_end;

#define __umctl2 __attribute__((section(".umctl2")))

__umctl2 struct dram_timing_info dram_timing_info_copy;

static uint32_t umctl2_load_training_firmware(enum fw_type type)
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
    pr_to32 = DDR_TRAIN_CODE_BASE_ADDR + 4 * IMEM_OFFSET_ADDR;

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
    pr_to32 = DDR_TRAIN_CODE_BASE_ADDR + 4 * DMEM_OFFSET_ADDR;

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
    pr_to32 = DDR_TRAIN_CODE_BASE_ADDR + 4 * IMEM_OFFSET_ADDR;

    for (i = 0x0; i < imem_sz;) {
        tmp32 = (mmio_read_16(pr_to32) & 0x0000ffff);
        pr_to32 += 4;
        tmp32 += ((mmio_read_16(pr_to32) & 0x0000ffff) << 16);

        if (tmp32 != mmio_read_32(pr_from32))
            return PB_ERR;

        pr_from32 += 4;
        pr_to32 += 4;
        i += 4;
    }

    pr_from32 = dmem_start;
    pr_to32 = DDR_TRAIN_CODE_BASE_ADDR + 4 * DMEM_OFFSET_ADDR;

    for (i = 0x0; i < dmem_sz;) {
        tmp32 = (mmio_read_16(pr_to32) & 0x0000ffff);
        pr_to32 += 4;
        tmp32 += ((mmio_read_16(pr_to32) & 0x0000ffff) << 16);

        if (tmp32 != mmio_read_32(pr_from32))
            return PB_ERR;

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
        reg = mmio_read_32((IP2APB_DDRPHY_IPS_BASE_ADDR(0) + 4 * 0xd0004));
    } while (reg & 0x1);
}

static void umctl2_ack_pmu_message_recieve(void)
{
    volatile unsigned int reg;

    mmio_write_32(IP2APB_DDRPHY_IPS_BASE_ADDR(0) + 4 * 0xd0031, 0x0);

    do {
        reg = mmio_read_32(IP2APB_DDRPHY_IPS_BASE_ADDR(0) + 4 * 0xd0004);
    } while (!(reg & 0x1));

    mmio_write_32(IP2APB_DDRPHY_IPS_BASE_ADDR(0) + 4 * 0xd0031, 0x1);
}

static unsigned int umctl2_get_mail(void)
{
    volatile unsigned int reg;
    umctl2_poll_pmu_message_ready();

    reg = mmio_read_32(IP2APB_DDRPHY_IPS_BASE_ADDR(0) + 4 * 0xd0032);

    umctl2_ack_pmu_message_recieve();

    return reg;
}

static unsigned int umctl2_get_stream_message(void)
{
    volatile unsigned int reg, reg2;

    umctl2_poll_pmu_message_ready();

    reg = mmio_read_32(IP2APB_DDRPHY_IPS_BASE_ADDR(0) + 4 * 0xd0032);

    reg2 = mmio_read_32(IP2APB_DDRPHY_IPS_BASE_ADDR(0) + 4 * 0xd0034);

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

static void umctl2_wait_ddrphy_training_complete(void)
{
    volatile unsigned int mail;

    while (1) {
        mail = umctl2_get_mail();

        if (mail == 0x08) {
            umctl2_decode_streaming_message();
        } else if (mail == 0x07) {
            LOG_INFO("Training PASS");
            break;
        } else if (mail == 0xff) {
            LOG_ERR("Training FAILED");
            break;
        }
    }
}

static void dram_pll_init(uint32_t data_rate)
{
    volatile uint32_t val;

    /* Bypass */
    mmio_clrsetbits_32(0x30360060, 0, SSCG_PLL_BYPASS1_MASK);
    mmio_clrsetbits_32(0x30360060, 0, SSCG_PLL_BYPASS2_MASK);

    switch (data_rate) {
    case 3200: {
        val = mmio_read_32(0x30360068);
        val &= ~(SSCG_PLL_OUTPUT_DIV_VAL_MASK | SSCG_PLL_FEEDBACK_DIV_F2_MASK |
                 SSCG_PLL_FEEDBACK_DIV_F1_MASK | SSCG_PLL_REF_DIVR2_MASK);
        val |= SSCG_PLL_OUTPUT_DIV_VAL(0);
        val |= SSCG_PLL_FEEDBACK_DIV_F2_VAL(11);
        val |= SSCG_PLL_FEEDBACK_DIV_F1_VAL(39);
        val |= SSCG_PLL_REF_DIVR2_VAL(29);
        mmio_write_32(0x30360068, val);
    } break;
    case 667: {
        val = mmio_read_32(0x30360068);
        val &= ~(SSCG_PLL_OUTPUT_DIV_VAL_MASK | SSCG_PLL_FEEDBACK_DIV_F2_MASK |
                 SSCG_PLL_FEEDBACK_DIV_F1_MASK | SSCG_PLL_REF_DIVR2_MASK);
        val |= SSCG_PLL_OUTPUT_DIV_VAL(3);
        val |= SSCG_PLL_FEEDBACK_DIV_F2_VAL(8);
        val |= SSCG_PLL_FEEDBACK_DIV_F1_VAL(45);
        val |= SSCG_PLL_REF_DIVR2_VAL(30);
        mmio_write_32(0x30360068, val);
    } break;
    default:
        break;
    }

    /* Clear power down bit */
    mmio_clrsetbits_32(0x30360060, SSCG_PLL_PD_MASK, 0);
    /* Eanble ARM_PLL/SYS_PLL  */
    mmio_clrsetbits_32(0x30360060, 0, SSCG_PLL_DRAM_PLL_CLKE_MASK);

    /* Clear bypass */
    mmio_clrsetbits_32(0x30360060, SSCG_PLL_BYPASS1_MASK, 0);
    pb_delay_ms(1);
    mmio_clrsetbits_32(0x30360060, SSCG_PLL_BYPASS2_MASK, 0);

    /* Wait lock */
    while (!(mmio_read_32(0x30360060) & SSCG_PLL_LOCK_MASK))
        ;
}

int umctl2_init(void)
{
    volatile uint32_t tmp;

    mmio_write_32(SRC_DDRC_RCR_ADDR + 0x04, 0x8F00000F);
    mmio_write_32(SRC_DDRC_RCR_ADDR, 0x8F00000F);
    mmio_write_32(SRC_DDRC_RCR_ADDR + 0x04, 0x8F000000);

    mmio_write_32(0x303A00EC, 0x0000ffff); /* PGC_CPU_MAPPING */
    mmio_clrsetbits_32(0x303A00F8, 0, (1 << 5)); /* PU_PGC_SW_PUP_REQ */

    dram_pll_init(3200);

    mmio_write_32(SRC_DDRC_RCR_ADDR, 0x8F000006);

    /* Load phy timing config */

    for (uint32_t n = 0; n < ARRAY_SIZE(umctl2_ddrc_cfg); n++) {
        mmio_write_32(umctl2_ddrc_cfg[n].reg, umctl2_ddrc_cfg[n].val);
    }

    mmio_write_32(SRC_DDRC_RCR_ADDR, 0x8F000004);
    mmio_write_32(SRC_DDRC_RCR_ADDR, 0x8F000000);

    mmio_write_32(DDRC_DBG1(0), 0x00);
    mmio_write_32(DDRC_RFSHCTL3(0), 0x11);
    mmio_write_32(DDRC_PWRCTL(0), 0xa8);

    do {
        tmp = mmio_read_32(DDRC_STAT(0));
    } while ((tmp & 0x33f) != 0x223);

    mmio_write_32(DDRC_DDR_SS_GPR0, 0x01); /* LPDDR4 mode */

    mmio_write_32(DDRC_SWCTL(0), 0x00);

    tmp = mmio_read_32(DDRC_MSTR2(0));

    if (tmp == 0x2)
        mmio_write_32(DDRC_DFIMISC(0), 0x210);
    else if (tmp == 0x1)
        mmio_write_32(DDRC_DFIMISC(0), 0x110);
    else
        mmio_write_32(DDRC_DFIMISC(0), 0x10);

    mmio_write_32(DDRC_SWCTL(0), 0x01);

    for (uint32_t n = 0; n < ARRAY_SIZE(lpddr4_ddrphy_cfg); n++) {
        dwc_ddrphy_apb_wr(lpddr4_ddrphy_cfg[n].reg, lpddr4_ddrphy_cfg[n].val);
    }

    /* load the dram training firmware image FSP 0 3200 Mt/s --------------- */

    dwc_ddrphy_apb_wr(0xd0000, 0x0);

    umctl2_load_training_firmware(FW_1D_IMAGE);

    for (uint32_t n = 0; n < ARRAY_SIZE(lpddr4_fsp0_cfg); n++) {
        dwc_ddrphy_apb_wr(lpddr4_fsp0_cfg[n].reg, lpddr4_fsp0_cfg[n].val);
    }

    dwc_ddrphy_apb_wr(0xd0000, 0x1);
    dwc_ddrphy_apb_wr(0xd0099, 0x9);
    dwc_ddrphy_apb_wr(0xd0099, 0x1);
    dwc_ddrphy_apb_wr(0xd0099, 0x0);

    /* Wait for the training firmware to complete */
    umctl2_wait_ddrphy_training_complete();

    /* Halt the microcontroller. */
    dwc_ddrphy_apb_wr(0xd0099, 0x1);

    /* Read the Message Block results */
    dwc_ddrphy_apb_wr(0xd0000, 0x0);
    dwc_ddrphy_apb_wr(0xd0000, 0x1);

    /* End of FSP 0 -------------------------------------------------------- */

    /* load the dram training firmware image FSP 1 667 Mt/s ---------------- */
    dram_pll_init(667);
    dwc_ddrphy_apb_wr(0xd0000, 0x0);

    umctl2_load_training_firmware(FW_1D_IMAGE);

    for (uint32_t n = 0; n < ARRAY_SIZE(lpddr4_fsp1_cfg); n++) {
        dwc_ddrphy_apb_wr(lpddr4_fsp1_cfg[n].reg, lpddr4_fsp1_cfg[n].val);
    }

    dwc_ddrphy_apb_wr(0xd0000, 0x1);
    dwc_ddrphy_apb_wr(0xd0099, 0x9);
    dwc_ddrphy_apb_wr(0xd0099, 0x1);
    dwc_ddrphy_apb_wr(0xd0099, 0x0);

    /* Wait for the training firmware to complete */
    umctl2_wait_ddrphy_training_complete();

    /* Halt the microcontroller. */
    dwc_ddrphy_apb_wr(0xd0099, 0x1);

    /* Read the Message Block results */
    dwc_ddrphy_apb_wr(0xd0000, 0x0);
    dwc_ddrphy_apb_wr(0xd0000, 0x1);

    /* End of FSP 1 -------------------------------------------------------- */

    /* load the dram training firmware image FSP 0 2D 3200 Mt/s ------------ */
    dram_pll_init(3200);
    dwc_ddrphy_apb_wr(0xd0000, 0x0);

    umctl2_load_training_firmware(FW_2D_IMAGE);

    for (uint32_t n = 0; n < ARRAY_SIZE(lpddr4_fsp0_2d_cfg); n++) {
        dwc_ddrphy_apb_wr(lpddr4_fsp0_2d_cfg[n].reg, lpddr4_fsp0_2d_cfg[n].val);
    }

    dwc_ddrphy_apb_wr(0xd0000, 0x1);
    dwc_ddrphy_apb_wr(0xd0099, 0x9);
    dwc_ddrphy_apb_wr(0xd0099, 0x1);
    dwc_ddrphy_apb_wr(0xd0099, 0x0);

    /* Wait for the training firmware to complete */
    umctl2_wait_ddrphy_training_complete();

    /* Halt the microcontroller. */
    dwc_ddrphy_apb_wr(0xd0099, 0x1);

    /* Read the Message Block results */
    dwc_ddrphy_apb_wr(0xd0000, 0x0);
    dwc_ddrphy_apb_wr(0xd0000, 0x1);

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
        tmp = mmio_read_32(DDRPHY_CalBusy(0));
    } while ((tmp & 0x1));

    LOG_DBG("ddrphy calibration done");

    mmio_write_32(DDRC_SWCTL(0), 0x00);

    tmp = mmio_read_32(DDRC_MSTR2(0));

    if (tmp == 0x2)
        mmio_write_32(DDRC_DFIMISC(0), 0x230);
    else if (tmp == 0x1)
        mmio_write_32(DDRC_DFIMISC(0), 0x130);
    else
        mmio_write_32(DDRC_DFIMISC(0), 0x030);

    mmio_write_32(DDRC_SWCTL(0), 0x01);

    /* step18 wait DFISTAT.dfi_init_complete to 1 */
    do {
        tmp = mmio_read_32(DDRC_DFISTAT(0));
    } while ((tmp & 0x1) == 0x0);

    mmio_write_32(DDRC_SWCTL(0), 0x00);

    tmp = mmio_read_32(DDRC_MSTR2(0));

    if (tmp == 0x2) {
        mmio_write_32(DDRC_DFIMISC(0), 0x210);
        /* set DFIMISC.dfi_init_complete_en again */
        mmio_write_32(DDRC_DFIMISC(0), 0x211);
    } else if (tmp == 0x1) {
        mmio_write_32(DDRC_DFIMISC(0), 0x110);
        /* set DFIMISC.dfi_init_complete_en again */
        mmio_write_32(DDRC_DFIMISC(0), 0x111);
    } else {
        /* clear DFIMISC.dfi_init_complete_en */
        mmio_write_32(DDRC_DFIMISC(0), 0x10);
        /* set DFIMISC.dfi_init_complete_en again */
        mmio_write_32(DDRC_DFIMISC(0), 0x11);
    }

    /* step23 [5]selfref_sw=0; */
    mmio_write_32(DDRC_PWRCTL(0), 0x08);
    /* step24 sw_done=1 */
    mmio_write_32(DDRC_SWCTL(0), 0x01);

    mmio_write_32(DDRC_DFIPHYMSTR(0), 1);
    /* step25 wait SWSTAT.sw_done_ack to 1 */
    do {
        tmp = mmio_read_32(DDRC_SWSTAT(0));
    } while ((tmp & 0x1) == 0x0);

    mmio_write_32(DDRC_DFIPHYMSTR(0), 0x01);

    /* wait STAT.operating_mode([1:0] for ddr3) to normal state */
    do {
        tmp = mmio_read_32(DDRC_STAT(0));
    } while ((tmp & 0x3) != 0x1);

    /* step26 */
    mmio_write_32(DDRC_RFSHCTL3(0), 0x10);

    /* enable port 0 */
    mmio_write_32(DDRC_PCTRL_0(0), 0x01);

    /* Copy all settings and results to 0x40000000. This is needed for
     * ATF and run time changes of transfer speed / voltage */

    dwc_ddrphy_apb_wr(0xd0000, 0x0);
    dwc_ddrphy_apb_wr(0xc0080, 0x3);

    for (uint32_t n = 0; n < dram_timing.ddrphy_trained_csr_num; n++) {
        dram_timing.ddrphy_trained_csr[n].val =
            dwc_ddrphy_apb_rd(dram_timing.ddrphy_trained_csr[n].reg);
    }

    dwc_ddrphy_apb_wr(0xc0080, 0x2);
    dwc_ddrphy_apb_wr(0xd0000, 0x1);

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
