/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * i.MX8MQ parts of the DRAM init: reset sequence and the SSCG DRAM PLL.
 */

#include <pb/delay.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <plat/imx8m/imx8m.h>

#include "ddr_soc.h"

#define RCR_DOMAINS (SRC_DDRC_RCR_DOM_EN | SRC_DDRC_RCR_DOMAIN_ALL)

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

void imx8m_ddr_soc_reset(void)
{
    mmio_write_32(SRC_DDRC2_RCR_ADDR, RCR_DOMAINS | SRC_DDRC_RCR_ALL_RESETS);
    mmio_write_32(SRC_DDRC_RCR_ADDR, RCR_DOMAINS | SRC_DDRC_RCR_ALL_RESETS);
    mmio_write_32(SRC_DDRC2_RCR_ADDR, RCR_DOMAINS);
}

int imx8m_ddr_soc_set_clock(unsigned int drate)
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
