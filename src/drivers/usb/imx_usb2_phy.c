/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/usb/imx_usb2_phy.h>
#include <pb/mmio.h>
#include <pb/pb.h>

/* USBPHY_PWDn */
#define IMX_USB2PHY_PWD            0x00

/* USBPHY_CTRLn */
#define IMX_USB2PHY_CTRL           0x30
#define CTRL_SFTRST                BIT(31)
#define CTRL_CLKGATE               BIT(30)

/* USBPHY_PLL_SICn */
#define IMX_USB2PHY_PLL_SIC        0xa0
#define PLL_SIC_PLL_LOCK           BIT(31)
#define PLL_SIC_PLL_DIV_SEL_MASK   (BIT(22) | BIT(23) | BIT(24))
#define PLL_SIC_PLL_DIV_SEL(x)     ((x << 22) & PLL_SIC_PLL_DIV_SEL_MASK)
#define PLL_REG_ENABLE             BIT(21)
#define PLL_ENABLE                 BIT(13)
#define PLL_POWER                  BIT(12)
#define PLL_EN_USB_CLKS            BIT(6)

/* USBPHY_VERSION */
#define IMX_USB2PHY_VERSION        0x80
#define VERSION_MAJ(x)             (uint8_t)((x >> 24) & 0xff)
#define VERSION_MIN(x)             (uint8_t)((x >> 16) & 0xff)
#define VERSION_STP(x)             (uint16_t)(x & 0xffff)

/* USBPHY_USB1_CHRG_DETECTn */
#define IMX_USB2PHY_CHRG_DETECT    0xE0
#define PULLUP_DP                  BIT(2)

/* USBPHY_USB1_CHRG_DET_STAT */
#define IMX_USB2PHY_CHRG_DET_STAT1 0xf0
#define SECDET_DCP                 BIT(4)
#define DP_STATE                   BIT(3)
#define DM_STATE                   BIT(2)
#define CHRG_DETECTED              BIT(1)
#define PLUG_CONTACT               BIT(0)

static uintptr_t base;
static uint8_t ver_maj, ver_min;
static uint16_t ver_stp;

int imx_usb2_phy_init(uintptr_t phy_base)
{
    base = phy_base;

    uint32_t version = mmio_read_32(base + IMX_USB2PHY_VERSION);
    ver_maj = VERSION_MAJ(version);
    ver_min = VERSION_MIN(version);
    ver_stp = VERSION_STP(version);

    LOG_INFO("Detected imx usb2phy, %u.%u.%u", ver_maj, ver_min, ver_stp);

    /* Bring PHY out of reset */
    mmio_clrsetbits_32(base + IMX_USB2PHY_CTRL, CTRL_SFTRST | CTRL_CLKGATE, 0);

    /**
     * Configure PLL
     *
     * The div value (3) translates to divide by 20, i.e.
     *  Fout = Fin * div_select. All supported iMX SoC's have a 24MHz xtal.
     *  Thus Fout = 24MHz * 20 = 480 MHz.
     */

    mmio_write_32(base + IMX_USB2PHY_PLL_SIC,
                  PLL_SIC_PLL_DIV_SEL(3) | PLL_REG_ENABLE | PLL_ENABLE | PLL_POWER |
                      PLL_EN_USB_CLKS);

    /* Clear power down bits */
    mmio_write_32(base + IMX_USB2PHY_PWD, 0);

    return PB_OK;
}
