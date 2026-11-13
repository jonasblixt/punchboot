/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_DRIVERS_IMX_WDOG_H
#define INCLUDE_DRIVERS_IMX_WDOG_H

#include <pb/utils_def.h>
#include <stdint.h>

/* The registers, 16 bit */
#define WDOG_WCR       0x0000 /* Control */
#define WDOG_WSR       0x0002 /* Service */
#define WDOG_WRSR      0x0004 /* Reset status */
#define WDOG_WICR      0x0006 /* Interrupt control */
#define WDOG_WMCR      0x0008 /* Miscellaneous control */

#define WDOG_WCR_WDBG  BIT(1) /* Suspended in debug mode */
#define WDOG_WCR_WDE   BIT(2) /* Enable */
#define WDOG_WCR_WDT   BIT(3) /* WDOG_B asserted on a timeout */
#define WDOG_WCR_SRS   BIT(4) /* 0: software reset */
#define WDOG_WCR_WDA   BIT(5) /* 0: WDOG_B asserted */
#define WDOG_WCR_SRE   BIT(6) /* Software reset extension */
#define WDOG_WCR_WDW   BIT(7) /* Suspended in wait mode */
#define WDOG_WCR_WT(x) ((x) << 8) /* Timeout, 0.5 s + x * 0.5 s */

/* The service sequence, written to WSR */
#define WDOG_WSR_SEQ1  0x5555
#define WDOG_WSR_SEQ2  0xAAAA

#define WDOG_WRSR_SFTW BIT(0) /* The last reset was a software reset */
#define WDOG_WRSR_TOUT BIT(1) /* ... a timeout */
#define WDOG_WRSR_POR  BIT(4) /* ... a power on reset */

#define WDOG_WMCR_PDE  BIT(0) /* The power-down counter runs */

/*
 * The driver keeps no state: each call takes the WDOG's base address, the
 * platform's constant, so that a reset and a kick work whether
 * imx_wdog_init() armed the watchdog or not (F-234).
 */
int imx_wdog_init(uintptr_t base, unsigned int delay_s);
int imx_wdog_kick(uintptr_t base);

/**
 * Reset the SoC through the WDOG's software reset (WCR.SRS). It doesn't
 * return: it waits for the reset. The WDOG's clock must be on.
 *
 * @param[in] base The WDOG's base address
 */
int imx_wdog_reset_now(uintptr_t base) __attribute__((noreturn));

/**
 * Stop a WDOG's power-down counter. It runs out of every reset and drives
 * WDOG_B low after 16 s unless WMCR.PDE is cleared (6UL RM 57.5.3), whether
 * the watchdog is used or not, so the platform does it for each of its WDOGs.
 * Their clocks must be on.
 *
 * @param[in] wdog_base The WDOG's base address
 */
void imx_wdog_disable_powerdown(uintptr_t wdog_base);

#endif // INCLUDE_DRIVERS_IMX_WDOG_H
