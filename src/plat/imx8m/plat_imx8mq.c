/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * i.MX8MQ clocks: the last clock gate and the FRAC/SSCG PLLs, moved here
 * unchanged from plat.c.
 */

#include <pb/mmio.h>
#include <pb/pb.h>
#include <plat/imx8m/imx8m.h>

#include "plat.h"

void imx8m_soc_clock_init(void)
{
    mmio_write_32(CCM_CCGR_SET(CCGR_IOMUX4), CCGR_CLK_ON_MASK);

    /* Configure main clocks */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(ARM_A53_CLK_ROOT), CLK_ROOT_ON);

    /* Configure PLL's */
    /* bypass the clock */
    mmio_clrsetbits_32(ARM_PLL_CFG0, 0, FRAC_PLL_BYPASS_MASK);

    /* Set CPU core clock to 1 GHz */
    mmio_write_32(ARM_PLL_CFG1, FRAC_PLL_INT_DIV_CTL_VAL(49));

    mmio_write_32(ARM_PLL_CFG0,
                  (FRAC_PLL_CLKE_MASK | FRAC_PLL_REFCLK_SEL_OSC_25M | FRAC_PLL_LOCK_SEL_MASK |
                   FRAC_PLL_NEWDIV_VAL_MASK | FRAC_PLL_REFCLK_DIV_VAL(4) |
                   FRAC_PLL_OUTPUT_DIV_VAL(0) | FRAC_PLL_BYPASS_MASK));

    /* unbypass the clock */
    mmio_clrsetbits_32(ARM_PLL_CFG0, FRAC_PLL_BYPASS_MASK, 0);

    while (!(mmio_read_32(ARM_PLL_CFG0) & FRAC_PLL_LOCK_MASK))
        ;

    mmio_clrsetbits_32(ARM_PLL_CFG0, FRAC_PLL_NEWDIV_VAL_MASK, 0);

    mmio_clrsetbits_32(SYS_PLL1_CFG0,
                       0,
                       SSCG_PLL_CLKE_MASK | SSCG_PLL_DIV2_CLKE_MASK | SSCG_PLL_DIV3_CLKE_MASK |
                           SSCG_PLL_DIV4_CLKE_MASK | SSCG_PLL_DIV5_CLKE_MASK |
                           SSCG_PLL_DIV6_CLKE_MASK | SSCG_PLL_DIV8_CLKE_MASK |
                           SSCG_PLL_DIV10_CLKE_MASK | SSCG_PLL_DIV20_CLKE_MASK);

    mmio_clrsetbits_32(SYS_PLL2_CFG0,
                       0,
                       SSCG_PLL_CLKE_MASK | SSCG_PLL_DIV2_CLKE_MASK | SSCG_PLL_DIV3_CLKE_MASK |
                           SSCG_PLL_DIV4_CLKE_MASK | SSCG_PLL_DIV5_CLKE_MASK |
                           SSCG_PLL_DIV6_CLKE_MASK | SSCG_PLL_DIV8_CLKE_MASK |
                           SSCG_PLL_DIV10_CLKE_MASK | SSCG_PLL_DIV20_CLKE_MASK);
}
