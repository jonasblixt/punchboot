/**
 * Punch BOOT
 *
 * Copyright (C) 2024 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/usb/imx8m_phy.h>
#include <pb/delay.h>
#include <pb/mmio.h>
#include <pb/pb.h>

#define DWC3_PHY_CTRL0             0x0
#define DWC3_PHY_CTRL0_REF_SSP_EN  BIT(2)

#define DWC3_PHY_CTRL1             0x4
#define DWC3_PHY_CTRL1_RESET       BIT(0)
#define DWC3_PHY_CTRL1_COMMONONN   BIT(1)
#define DWC3_PHY_CTRL1_ATERESET    BIT(3)
#define DWC3_PHY_CTRL1_VDATSRCENB0 BIT(19)
#define DWC3_PHY_CTRL1_VDATDETENB0 BIT(20)

#define DWC3_PHY_CTRL2             0x8
#define DWC3_PHY_CTRL2_TXENABLEN0  BIT(8)

int imx8m_usb_phy_init(uintptr_t base)
{
    mmio_clrsetbits_32(base + DWC3_PHY_CTRL1,
                       DWC3_PHY_CTRL1_VDATSRCENB0 | DWC3_PHY_CTRL1_VDATDETENB0 |
                           DWC3_PHY_CTRL1_COMMONONN,
                       DWC3_PHY_CTRL1_RESET | DWC3_PHY_CTRL1_ATERESET);

    mmio_clrsetbits_32(base + DWC3_PHY_CTRL0, 0, DWC3_PHY_CTRL0_REF_SSP_EN);
    mmio_clrsetbits_32(base + DWC3_PHY_CTRL2, 0, DWC3_PHY_CTRL2_TXENABLEN0);
    mmio_clrsetbits_32(base + DWC3_PHY_CTRL1, DWC3_PHY_CTRL1_RESET | DWC3_PHY_CTRL1_ATERESET, 0);

    return PB_OK;
}
