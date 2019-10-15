/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_IMX8M_INCLUDE_PLAT_DEFS_H_
#define PLAT_IMX8M_INCLUDE_PLAT_DEFS_H_

#include <plat/imx/gpt.h>
#include <plat/imx/usdhc.h>
#include <plat/imx/caam.h>
#include <plat/imx/dwc3.h>
#include <plat/imx/imx_uart.h>
#include <plat/imx/wdog.h>
#include <plat/imx/ocotp.h>

#define PB_BOOTPART_OFFSET 66

struct pb_platform_setup
{
    struct gp_timer tmr0;
    struct usdhc_device usdhc0;
    struct imx_uart_device uart0;
    struct dwc3_device usb0;
    struct ocotp_dev ocotp;
    struct imx_wdog_device wdog;
};

#endif  // PLAT_IMX8M_INCLUDE_PLAT_DEFS_H_
