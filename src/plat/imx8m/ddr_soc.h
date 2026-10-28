/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The SoC specific parts of the i.MX8M DRAM init (ddr.c). One
 * implementation per SoC: ddr_imx8mq.c.
 */

#ifndef PLAT_IMX8M_DDR_SOC_H
#define PLAT_IMX8M_DDR_SOC_H

/* Hold the DDR controller and PHY in reset */
void imx8m_ddr_soc_reset(void);

/* Set the DRAM clock (PLL, or bypass mode where the SoC has it) for 'drate' MT/s */
int imx8m_ddr_soc_set_clock(unsigned int drate);

#endif // PLAT_IMX8M_DDR_SOC_H
