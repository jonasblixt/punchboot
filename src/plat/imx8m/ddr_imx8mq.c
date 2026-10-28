/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 * Copyright 2018-2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * i.MX8MQ parts of the DRAM init: reset sequence and the DRAM clock.
 *
 * The reset hold sequence is the i.MX8MQ one in NXP's BSD-3-Clause TF-A
 * code (plat/imx/imx8m/ddr/dram_retention.c, dram_exit_retention()). Note
 * that TF-A holds SRC_DDRC2_RCR with 0x8F000003 where this writes
 * 0x8F00000F, as NXP's U-Boot does; not changed without hardware to test
 * on.
 *
 * The DRAM clock settings are those of NXP's DDR tool (v4.2.0) for the
 * i.MX8MQ EVK: above 667 MT/s the SSCG DRAM PLL drives the DRAM clock, below
 * that the PHY runs in bypass mode with the clock from DRAM_ALT_CLK_ROOT.
 * For 3200 MT/s the tool's register programming aid gives the PLL setting
 * 0x00ece580 (800 MHz), which is what the dividers below produce; its
 * stress test script writes 0x780, another divider set for 800 MHz.
 */

#include <pb/delay.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <plat/imx8m/imx8m.h>

#include "ddr_soc.h"

#define RCR_DOMAINS         (SRC_DDRC_RCR_DOM_EN | SRC_DDRC_RCR_DOMAIN_ALL)

#define CLK_ROOT_MUX_DIV    (CLK_ROOT_SRC_MUX_MASK | CLK_ROOT_PRE_DIV_MASK)

/* DRAM_SEL_CFG: the DRAM clock comes from DRAM_ALT_CLK_ROOT (PHY bypass) */
#define DRAM_SEL_CFG_BYPASS CLK_ROOT_SOURCE_SEL(1)

/* DRAM_APB_CLK_ROOT: sys_pll1_800m / 4 = 200 MHz with the PLL, sys_pll1_40m / 2 = 20 MHz in bypass
 */
#define DRAM_APB_CLK_PLL    (CLK_ROOT_SOURCE_SEL(4) | CLK_ROOT_PRE_DIV(CLK_ROOT_PRE_DIV4))
#define DRAM_APB_CLK_BYPASS (CLK_ROOT_SOURCE_SEL(2) | CLK_ROOT_PRE_DIV(CLK_ROOT_PRE_DIV2))

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

/* PHY bypass: DRAM_ALT_CLK_ROOT runs at the data rate in MHz (twice the DRAM clock) */
struct dram_bypass_setting {
    unsigned int drate; /* MT/s */
    uint32_t alt_clk_root;
};

static const struct dram_bypass_setting dram_bypass_settings[] = {
    /* sys_pll1_400m / 1 */
    { .drate = 400, .alt_clk_root = CLK_ROOT_SOURCE_SEL(5) | CLK_ROOT_PRE_DIV(CLK_ROOT_PRE_DIV1) },
    /* sys_pll1_100m / 1 */
    { .drate = 100, .alt_clk_root = CLK_ROOT_SOURCE_SEL(2) | CLK_ROOT_PRE_DIV(CLK_ROOT_PRE_DIV1) },
};

void imx8m_ddr_soc_reset(void)
{
    mmio_write_32(SRC_DDRC2_RCR_ADDR, RCR_DOMAINS | SRC_DDRC_RCR_ALL_RESETS);
    mmio_write_32(SRC_DDRC_RCR_ADDR, RCR_DOMAINS | SRC_DDRC_RCR_ALL_RESETS);
    mmio_write_32(SRC_DDRC2_RCR_ADDR, RCR_DOMAINS);
}

/* Set the mux and pre divider of a clock root, through its CLR and SET registers */
static void clk_root_set(unsigned int root, uint32_t mux_div)
{
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_CLR(root), CLK_ROOT_MUX_DIV);
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(root), mux_div);
}

static void dram_pll_init(const struct dram_pll_setting *pll)
{
    uint32_t val;

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
}

int imx8m_ddr_soc_set_clock(unsigned int drate)
{
    for (unsigned int i = 0; i < ARRAY_SIZE(dram_pll_settings); i++) {
        if (dram_pll_settings[i].drate == drate) {
            clk_root_set(DRAM_APB_CLK_ROOT, DRAM_APB_CLK_PLL);
            dram_pll_init(&dram_pll_settings[i]);
            mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_CLR(DRAM_SEL_CFG), DRAM_SEL_CFG_BYPASS);
            return PB_OK;
        }
    }

    for (unsigned int i = 0; i < ARRAY_SIZE(dram_bypass_settings); i++) {
        if (dram_bypass_settings[i].drate == drate) {
            clk_root_set(DRAM_ALT_CLK_ROOT, dram_bypass_settings[i].alt_clk_root);
            clk_root_set(DRAM_APB_CLK_ROOT, DRAM_APB_CLK_BYPASS);
            mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(DRAM_SEL_CFG), DRAM_SEL_CFG_BYPASS);
            return PB_OK;
        }
    }

    LOG_ERR("No DRAM clock setting for %u MT/s", drate);
    return -PB_ERR_NOT_SUPPORTED;
}
