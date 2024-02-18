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

#include <pb/rot.h>
#include <pb/slc.h>
#include <plat/imx8x/mm.h>
#include <plat/imx8x/pins.h>
#include <plat/imx8x/sci/sci.h>
#include <plat/imx8x/sci/sci_ipc.h>

#define UART_PAD_CTRL                                                                              \
    (PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | (SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | \
     (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) |                                                  \
     (SC_PAD_28FDSOI_DSE_DV_LOW << PADRING_DSE_SHIFT) |                                            \
     (SC_PAD_28FDSOI_PS_PD << PADRING_PULL_SHIFT))

#define GPIO_PAD_CTRL                                                                              \
    (PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | (SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | \
     (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) |                                                  \
     (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) |                                           \
     (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

struct imx8x_platform {
    sc_ipc_t ipc;
    uint32_t soc_id;
    uint32_t soc_rev;
};

int board_init(struct imx8x_platform *plat);
int imx8x_revoke_key(const struct rot_key *key);
int imx8x_read_key_status(const struct rot_key *key);
slc_t imx8x_slc_read_status(void);
int imx8x_slc_set_configuration_locked(void);
void imx8x_rot_helpers_init(sc_ipc_t ipc_);
void imx8x_slc_helpers_init(sc_ipc_t ipc_);

#endif // PLAT_INCLUDE_IMX8X_IMX8X_H
