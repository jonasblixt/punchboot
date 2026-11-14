/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
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

#ifdef CONFIG_IMX8M_CM_USB1
int imx8m_soc_usb1_power_up(void)
{
    /* Enable and ungate USB clocks */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(USB_BUS_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(USB_CORE_REF_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(USB_PHY_REF_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CCGR_SET(CCGR_USB_CTRL1), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_USB_PHY1), CCGR_CLK_ON_MASK);

    return PB_OK;
}
#endif

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

const char *imx8m_soc_name(void)
{
    return "i.MX8MQ";
}

/* RM 5.1.8.28. The B0 and B1 read the same; U-Boot tells them apart by a
 * ROM word and an OCOTP magic, not read here */
uint32_t imx8m_soc_digprog(void)
{
    return mmio_read_32(DIGPROG);
}

/* The ARM PLL that imx8m_soc_clock_init() sets, a fractional PLL from the
 * 25 MHz oscillator (RM Fig. 5-9): REF / (REFCLK_DIV + 1) * 8 *
 * (INT_DIV + 1) / (2 * (OUTPUT_DIV + 1)), the fraction left out */
uint32_t imx8m_soc_cpu_hz(void)
{
    uint32_t cfg0 = mmio_read_32(ARM_PLL_CFG0);
    uint32_t cfg1 = mmio_read_32(ARM_PLL_CFG1);
    uint32_t ref_div = ((cfg0 & FRAC_PLL_REFCLK_DIV_VAL_MASK) >> FRAC_PLL_REFCLK_DIV_VAL_SHIFT) + 1;
    uint32_t out_div = 2 * ((cfg0 & FRAC_PLL_OUTPUT_DIV_VAL_MASK) + 1);
    uint32_t int_div = (cfg1 & FRAC_PLL_INT_DIV_CTL_MASK) + 1;

    if (cfg0 & FRAC_PLL_BYPASS_MASK)
        return 0;

    /* In kHz: 32 bits hold 25 MHz * 8 * 128 only so */
    return 25000u / ref_div * 8 * int_div / out_div * 1000;
}
