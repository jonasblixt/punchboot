/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * i.MX8M Plus PLLs: integer/fractional PLLs (PLL14xx) in CCM_ANALOG, a
 * different register layout than the i.MX8MQ's FRAC/SSCG PLLs in clock.h
 * (RM 5.1.8). The clock roots and gates in clock.h are the same on both.
 */

#ifndef INCLUDE_PLAT_IMX8M_CLOCK_IMX8MP_H
#define INCLUDE_PLAT_IMX8M_CLOCK_IMX8MP_H

#define IMX8MP_DRAM_PLL_GNRL_CTL          (0x30360050)
#define IMX8MP_DRAM_PLL_FDIV_CTL0         (0x30360054)
#define IMX8MP_DRAM_PLL_FDIV_CTL1         (0x30360058)
#define IMX8MP_SYS_PLL1_GEN_CTRL          (0x30360094)
#define IMX8MP_SYS_PLL2_GEN_CTRL          (0x30360104)

/* GNRL_CTL/GEN_CTRL */
#define PLL14XX_LOCK                      BIT_32(31)
#define PLL14XX_RST                       BIT_32(9) /* Active low */
#define PLL14XX_BYPASS                    BIT_32(4)

/* FDIV_CTL0: FOUT = FIN * main / (pre * 2^post), FIN 24 MHz */
#define PLL14XX_MAIN_DIV(n)               (((n) & 0x3ff) << 12)
#define PLL14XX_PRE_DIV(n)                (((n) & 0x3f) << 4)
#define PLL14XX_POST_DIV(n)               ((n) & 0x7)

/* DRAM_SEL_CFG (clock.h) is called DRAM_CLK_ROOT_SEL on the i.MX8M Plus, the same slice */

/* Clock roots and gates the i.MX8MQ doesn't have (RM 5.1.4.4, 5.1.4.6) */
#define IMX8MP_HSIO_AXI_CLK_ROOT          7
/* Not in the RM's clock root table (Table 5-1), named in Table 6-4 */
#define IMX8MP_USB_CORE_REF_CLK_ROOT      98
#define IMX8MP_USB_PHY_REF_CLK_ROOT       99
#define IMX8MP_USDHC2_CLK_ROOT            89
#define IMX8MP_USDHC3_CLK_ROOT            121
#define IMX8MP_CCGR_USDHC2                82
#define IMX8MP_CCGR_USDHC3                94
#define IMX8MP_CCGR_USB                   77
#define IMX8MP_CCGR_USB_PHY               79
#define IMX8MP_CCGR_HSIO                  92

/* HSIO BLK_CTRL (RM 11.1.3.1) */
#define IMX8MP_HSIO_BLK_CTRL_GPR_REG0     (0x32F10000)
#define HSIO_GPR_REG0_USB_CLOCK_MODULE_EN BIT_32(1)
#define HSIO_GPR_REG0_USB_PHY_REF_CLK_SEL BIT_32(6) /* 0: 24 MHz oscillator */

/* GPC power domains (RM 5.2.10), not the i.MX8MQ layout in mm.h */
#define IMX8MP_GPC_PU_PGC_SW_PUP_REQ      (0x303A00D8)
#define IMX8MP_GPC_PU_PWRHSK              (0x303A0190)
#define IMX8MP_GPC_PGC_CPU_A53_MAPPING    (0x303A01CC)
#define GPC_PU_USB1_PHY                   BIT_32(2) /* PUP_REQ */
#define GPC_PU_HSIOMIX                    BIT_32(17)
#define GPC_A53_MAPPING_USB1_PHY          BIT_32(4)
#define GPC_A53_MAPPING_HSIOMIX           BIT_32(19)
#define GPC_PWRHSK_NOC2HSIO_PWRDNREQN     BIT_32(12)
#define GPC_PWRHSK_NOC2HSIO_PWRDNACKN     BIT_32(28)

#endif
