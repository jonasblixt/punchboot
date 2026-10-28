/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * i.MX8M Plus parts of the DRAM init: reset sequence and the DRAM clock.
 *
 * The reset hold sequence and the 4000 MT/s DRAM PLL setting are those of
 * NXP's DDR tool (v4.2.0) stress test script for the i.MX8M Plus EVK. The
 * DRAM PLL is a PLL14xx; the script programs it once from reset, here it is
 * also reprogrammed after the bypass runs, so it is bypassed and held in
 * reset first (RM, CCM_ANALOG_DRAM_PLL_GNRL_CTL).
 *
 * As on the i.MX8MQ, at or below 625 MT/s the PHY runs in bypass mode with
 * the clock from DRAM_ALT_CLK_ROOT, selected in DRAM_CLK_ROOT_SEL (slice 48,
 * DRAM_SEL_CFG on the i.MX8MQ). The DRAM_ALT_CLK_ROOT sources are from the
 * RM's clock root table; DRAM_APB_CLK_ROOT uses the same sources as on the
 * i.MX8MQ.
 */

#include <pb/mmio.h>
#include <pb/pb.h>
#include <plat/imx8m/clock_imx8mp.h>
#include <plat/imx8m/imx8m.h>

#include "ddr_soc.h"

#define RCR_DOMAINS         (SRC_DDRC_RCR_DOM_EN | SRC_DDRC_RCR_DOMAIN_ALL)

#define CLK_ROOT_MUX_DIV    (CLK_ROOT_SRC_MUX_MASK | CLK_ROOT_PRE_DIV_MASK)

/* DRAM_CLK_ROOT_SEL: the DRAM clock comes from DRAM_ALT_CLK_ROOT (PHY bypass) */
#define DRAM_SEL_CFG_BYPASS CLK_ROOT_SOURCE_SEL(1)

/* DRAM_APB_CLK_ROOT: SYSTEM_PLL1 / 4 = 200 MHz with the PLL (NXP's script),
 * SYSTEM_PLL1_DIV20 / 2 = 20 MHz in bypass */
#define DRAM_APB_CLK_PLL    (CLK_ROOT_SOURCE_SEL(4) | CLK_ROOT_PRE_DIV(CLK_ROOT_PRE_DIV4))
#define DRAM_APB_CLK_BYPASS (CLK_ROOT_SOURCE_SEL(2) | CLK_ROOT_PRE_DIV(CLK_ROOT_PRE_DIV2))

/* DRAM PLL (PLL14xx, 24 MHz reference) settings per data rate, output = data rate / 4 */
struct dram_pll_setting {
    unsigned int drate; /* MT/s */
    uint32_t fdiv_ctl0;
};

static const struct dram_pll_setting dram_pll_settings[] = {
    /* 24 MHz * 250 / (3 * 2^1) = 1000 MHz, NXP's script: 0xfa031 */
    { .drate = 4000,
      .fdiv_ctl0 = PLL14XX_MAIN_DIV(250) | PLL14XX_PRE_DIV(3) | PLL14XX_POST_DIV(1) },
};

/* PHY bypass: DRAM_ALT_CLK_ROOT runs at the data rate in MHz (twice the DRAM clock) */
struct dram_bypass_setting {
    unsigned int drate; /* MT/s */
    uint32_t alt_clk_root;
};

static const struct dram_bypass_setting dram_bypass_settings[] = {
    /* SYSTEM_PLL1 (800 MHz) / 2 */
    { .drate = 400, .alt_clk_root = CLK_ROOT_SOURCE_SEL(1) | CLK_ROOT_PRE_DIV(CLK_ROOT_PRE_DIV2) },
    /* SYSTEM_PLL1_DIV8 (100 MHz) / 1 */
    { .drate = 100, .alt_clk_root = CLK_ROOT_SOURCE_SEL(2) | CLK_ROOT_PRE_DIV(CLK_ROOT_PRE_DIV1) },
};

void imx8m_ddr_soc_reset(void)
{
    /* The DDRMIX clock gate (DRAM1) is off after reset and the ROM doesn't
     * open it (RM 6.1.4.2): without it the first DDRC access faults */
    mmio_write_32(CCM_CCGR_SET(CCGR_DRAM1), CCGR_CLK_ON_MASK);

    /* All resets and src_system_rst_b, then src_system_rst_b released */
    mmio_write_32(SRC_DDRC_RCR_ADDR, RCR_DOMAINS | SRC_DDRC_RCR_SYSTEM_RST | SRC_DDRC_RCR_ALL_RESETS);
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
    /* Output from the reference while the PLL is held in reset */
    mmio_clrsetbits_32(IMX8MP_DRAM_PLL_GNRL_CTL, 0, PLL14XX_BYPASS);
    mmio_clrsetbits_32(IMX8MP_DRAM_PLL_GNRL_CTL, PLL14XX_RST, 0);

    mmio_write_32(IMX8MP_DRAM_PLL_FDIV_CTL0, pll->fdiv_ctl0);
    mmio_write_32(IMX8MP_DRAM_PLL_FDIV_CTL1, 0); /* No fractional part */

    mmio_clrsetbits_32(IMX8MP_DRAM_PLL_GNRL_CTL, 0, PLL14XX_RST);

    while (!(mmio_read_32(IMX8MP_DRAM_PLL_GNRL_CTL) & PLL14XX_LOCK))
        ;

    mmio_clrsetbits_32(IMX8MP_DRAM_PLL_GNRL_CTL, PLL14XX_BYPASS, 0);
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
