/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_INCLUDE_IMX8X_IMX8X_H
#define PLAT_INCLUDE_IMX8X_IMX8X_H

#include <pb/rot.h>
#include <pb/slc.h>
#include <plat/imx8x/mm.h>
#ifdef CONFIG_PLAT_IMX8XL
#include <plat/imx8x/imx8dxl_pads.h>
#elif CONFIG_PLAT_IMX8X
#include <plat/imx8x/imx8qxp_pads.h>
#endif
/* The SCFW's services a board uses (platform.md 7, D-056): its pads and the
 * clocks and power of its peripherals (pad, pm), the boot button and the boot
 * type (misc), the SECO (seco). The builds, the SoC and the temperature are in
 * the log at boot (plat_init()). A board includes this header, never the
 * services' by their path */
#include <plat/imx8x/sci/ipc.h>
#include <plat/imx8x/sci/svc/misc/api.h>
#include <plat/imx8x/sci/svc/pad/api.h>
#include <plat/imx8x/sci/svc/pm/api.h>
#include <plat/imx8x/sci/svc/seco/api.h>
#include <stddef.h>

/* SC_C_ID, sc_misc_get_control(SC_R_SYSTEM): the SoC id and revision */
#define IMX8X_SC_ID_SOC(id)    ((id) & 0x1f)
#define IMX8X_SC_ID_REV(id)    (((id) >> 5) & 0xf)

/* TF-A's BL31 at the start of the RAM, BOARD_RAM_BASE; pb follows it. pb's
 * reset stub is linked there (link.lds) and has run. SoC memory a board may
 * list as an image data region (platform.md 5.3) */
#define IMX8X_ATF_BASE         (0x80000000)
#define IMX8X_ATF_SIZE         (0x20000)

#define PADRING_IFMUX_EN_SHIFT 31
#define PADRING_IFMUX_EN_MASK  (1U << PADRING_IFMUX_EN_SHIFT)
#define PADRING_GP_EN_SHIFT    30
#define PADRING_GP_EN_MASK     (1U << PADRING_GP_EN_SHIFT)
#define PADRING_IFMUX_SHIFT    27
#define PADRING_IFMUX_MASK     (0x7U << PADRING_IFMUX_SHIFT)
#define PADRING_CONFIG_SHIFT   25
#define PADRING_CONFIG_MASK    (0x3U << PADRING_CONFIG_SHIFT)
#define PADRING_LPCONFIG_SHIFT 23
#define PADRING_LPCONFIG_MASK  (0x3U << PADRING_LPCONFIG_SHIFT)
#define PADRING_PULL_SHIFT     5
#define PADRING_PULL_MASK      (0x3U << PADRING_PULL_SHIFT)
#define PADRING_DSE_SHIFT      0
#define PADRING_DSE_MASK       (0x7U << PADRING_DSE_SHIFT)

#define IMX8X_PAD_MUX(val)     ((val << PADRING_IFMUX_SHIFT) & PADRING_IFMUX_MASK)

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

/*
 * The boot reason (plat_get_boot_reason(), platform.md 3.4): raw is the
 * SCFW's reset reason, SC_PM_RESET_REASON_* (sci/svc/pm/api.h), as
 * plat_early_init() got it from sc_pm_reset_reason(). The SCFW gives one
 * reason, so there is no priority, and pb has nothing to clear. The kind of
 * each, with the SCFW's name for it as the name:
 *
 *   POR      POR (0)
 *   DEBUG    JTAG (1)
 *   SW       SW (2)
 *   WDOG     WDOG (3, a partition's), SCFW_WDOG (9), ROM_WDOG (10)
 *   FAULT    LOCKUP (4), SNVS (5), TEMP (6), UECC (8), SECO (11), SCFW_FAULT (12)
 *   UNKNOWN  MSI (7), V2X_DEBUG (13), BOOT_STAGE (14): no kind fits. Also a
 *            reason the SCFW adds later, named "Unknown"
 *
 * When the SCFW gives no reason the call returns -PB_ERR_IO, with UNKNOWN and
 * a raw value of 0.
 */
struct imx8x_platform {
    sc_ipc_t ipc;
    uint32_t soc_id;
    uint32_t soc_rev;
};

/** The SCFW IPC handle and the SoC id, for the board */
const struct imx8x_platform *imx8x_plat(void);
sc_ipc_t imx8x_ipc(void);

/** A pad and its SCFW setting, mux and electrical (sc_pad_set()) */
struct imx8x_pad {
    sc_pad_t pad;
    uint32_t ctrl;
};

/** Set up the pads in table order */
int imx8x_pads_setup(const struct imx8x_pad *pads, size_t length);
int imx8x_revoke_key(const struct rot_key *key);
int imx8x_read_key_status(const struct rot_key *key);
slc_t imx8x_slc_read_status(void);
/** Fuses the SRK hash of the build, srk_fuse (pb/srk_fuse.h) */
int imx8x_slc_set_configuration(void);
int imx8x_slc_set_configuration_locked(void);
void imx8x_rot_helpers_init(sc_ipc_t ipc_);
void imx8x_slc_helpers_init(sc_ipc_t ipc_);

#endif // PLAT_INCLUDE_IMX8X_IMX8X_H
