/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_INCLUDE_IMX8X_IMX8X_H
#define PLAT_INCLUDE_IMX8X_IMX8X_H

#include <plat/imx8x/sci/sci_ipc.h>
#include <plat/imx8x/sci/sci.h>
#include <plat/imx8x/imx8qx_pads.h>
#include <platform_defs.h>

#define IMX8X_FUSE_ROW(__r, __d) \
        {.bank = __r , .word = 0, .description = __d, .status = FUSE_VALID}

#define IMX8X_FUSE_ROW_VAL(__r, __d, __v) \
        {.bank = __r, .word = 0, .description = __d, \
         .default_value = __v, .status = FUSE_VALID}

#define IMX8X_FUSE_END { .status = FUSE_INVALID }

#define PADRING_IFMUX_EN_SHIFT		31
#define PADRING_IFMUX_EN_MASK		(1U << PADRING_IFMUX_EN_SHIFT)
#define PADRING_GP_EN_SHIFT		30
#define PADRING_GP_EN_MASK		(1U << PADRING_GP_EN_SHIFT)
#define PADRING_IFMUX_SHIFT		27
#define PADRING_IFMUX_MASK		(0x7U << PADRING_IFMUX_SHIFT)
#define PADRING_CONFIG_SHIFT		25
#define PADRING_CONFIG_MASK		(0x3U << PADRING_CONFIG_SHIFT)
#define PADRING_LPCONFIG_SHIFT		23
#define PADRING_LPCONFIG_MASK		(0x3U << PADRING_LPCONFIG_SHIFT)
#define PADRING_PULL_SHIFT		5
#define PADRING_PULL_MASK		(0x3U << PADRING_PULL_SHIFT)
#define PADRING_DSE_SHIFT		0
#define PADRING_DSE_MASK		(0x7U << PADRING_DSE_SHIFT)

#define UART_PAD_CTRL    (PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | \
            (SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | \
            (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
            (SC_PAD_28FDSOI_DSE_DV_LOW << PADRING_DSE_SHIFT) | \
            (SC_PAD_28FDSOI_PS_PD << PADRING_PULL_SHIFT))

#define GPIO_PAD_CTRL   (PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | \
                         (SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | \
                         (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
                         (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | \
                         (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define IMX8X_PAD_MUX(val) ((val << PADRING_IFMUX_SHIFT) & PADRING_IFMUX_MASK)


#define IMX_CAAM_BASE 0x31430000
#define IMX_EHCI_BASE 0x5b0d0000
#define IMX_USBDCD_BASE 0x5b100800
#define IMX_GPT_BASE 0x5D140000
#define SC_IPC_BASE  0x5d1b0000
#define IMX_GPT_PR 24

struct imx8x_platform
{
    sc_ipc_t ipc;
    uint32_t soc_id;
    uint32_t soc_rev;
};

int board_init(struct imx8x_platform *plat);

#endif  // PLAT_INCLUDE_IMX8X_IMX8X_H
