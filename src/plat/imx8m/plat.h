/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
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

#endif // PLAT_IMX8M_PLAT_H_
