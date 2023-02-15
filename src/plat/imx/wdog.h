/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_IMX_WDOG_H_
#define PLAT_IMX_WDOG_H_

#include <pb/pb.h>
#include <pb/io.h>

#define WDOG_WCR  0x0000
#define WDOG_WSR  0x0002
#define WDOG_WRSR 0x0004
#define WDOG_WICR 0x0006
#define WDOG_WMCR 0x0008

int imx_wdog_init(__iomem base, unsigned int delay);
int imx_wdog_kick(void);
int imx_wdog_reset_now(void);

#endif  // PLAT_IMX_WDOG_H_
