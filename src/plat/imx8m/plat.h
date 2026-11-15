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

/* Command mode's USB port (platform.md 3.10): USB1 on both SoCs, or the
 * i.MX8M Plus's USB2. Its ROM's serial downloader uses either (8MP RM 6.1.7),
 * the 8MQ's USB1 only (8MQ RM 6.1.2.4) */
#ifdef CONFIG_IMX8M_CM_USB2
#define IMX8M_CM_USB_BASE     IMX8M_USB2_BASE
#define IMX8M_CM_USB_PHY_BASE IMX8M_USB2_PHY_BASE
#else
#define IMX8M_CM_USB_BASE     IMX8M_USB1_BASE
#define IMX8M_CM_USB_PHY_BASE IMX8M_USB1_PHY_BASE
#endif

/* The clocks and power of command mode's USB port, before its PHY and its
 * controller are set up (plat_<soc>.c) */
int imx8m_soc_cm_usb_power_up(void);

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

/* USB1 for the SPL's fastboot (spl/spl.c, D-089): the controller is
 * registered with the stack, not started */
int imx8m_spl_usb_init(void);

/* WDOG1 while the SPL's fastboot runs (spl/spl.c): armed and kicked where
 * the build has the watchdog (CONFIG_ENABLE_WATCHDOG), nothing otherwise */
void imx8m_spl_wdog_start(unsigned int timeout_s);
void imx8m_spl_wdog_kick(void);

/* A reset from the SPL, through WDOG1 (spl/spl.c) */
void imx8m_spl_reset(void);

#endif // PLAT_IMX8M_PLAT_H_
