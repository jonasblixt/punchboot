/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_IMX8M_PLAT_H_
#define PLAT_IMX8M_PLAT_H_

#include <stdint.h>

/* The SoC's clock gates and PLLs, after the common gates (plat_<soc>.c) */
void imx8m_soc_clock_init(void);

/* USB1's clocks and power, before its PHY and its controller are set up
 * (plat_<soc>.c) */
int imx8m_soc_usb1_power_up(void);

/* The SoC's name, its DIGPROG and the A53's clock in Hz, 0 when it can't be
 * read (plat_<soc>.c, for plat_get_info()) */
const char *imx8m_soc_name(void);
uint32_t imx8m_soc_digprog(void);
uint32_t imx8m_soc_cpu_hz(void);

/* The TZASC (tzasc.c, D-088): the SPL enables it before DRAM is used and
 * opens the board's DRAM in region 1; pb proper checks both */
int imx8m_tzasc_enable(void);
int imx8m_tzasc_open_dram(void);
int imx8m_tzasc_check(void);

/* The SPL's DRAM bring-up (spl/spl.c): the clocks, the TZASC and the DRAM,
 * in plat_init()'s order before D-088 */
int imx8m_spl_dram_init(void);

#endif // PLAT_IMX8M_PLAT_H_
