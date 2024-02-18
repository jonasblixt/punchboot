/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_DRIVERS_USB_DWC3_UDC_PRIVATE_H
#define INCLUDE_DRIVERS_USB_DWC3_UDC_PRIVATE_H

#include <pb/utils_def.h>
#include <stdint.h>

#define DWC3_CAPLENGTH               0x0000
#define DWC3_GSTS                    0xC118

#define DWC3_DCFG                    0xC700
#define DCFG_IGMSTRMPP               BIT(23)
#define DCFG_LPMCAP                  BIT(22)
#define DCFG_NUMP_SHIFT              (17)
#define DCFG_NUMP_MASK               (0x1f << DCFG_NUMP_SHIFT)
#define DCFG_NUMP(x)                 ((x << DCFG_NUMP_SHIFT) & DCFG_NUMP_MASK)
#define DCFG_INTRNUM_SHIFT           (12)
#define DCFG_INTRNUM_MASK            (0x1f << DCFG_INTRNUM_SHIFT)
#define DCFG_INTRNUM(x)              ((x << DCFG_INTRNUM_SHIFT) & DCFG_INTRNUM_MASK)
#define DCFG_DEVADDR_SHIFT           (3)
#define DCFG_DEVADDR_MASK            (0x7f << DCFG_DEVADDR_SHIFT)
#define DCFG_DEVADDR(x)              ((x << DCFG_DEVADDR_SHIFT) & DCFG_DEVADDR_MASK)
#define DCFG_DEVSPD_SHIFT            (0)
#define DCFG_DEVSPD_MASK             (0x7 << DCFG_DEVSPD_SHIFT)
#define DCFG_DEVSPD(x)               ((x << DCFG_DEVSPD_SHIFT) & DCFG_DEVSPD_MASK)

#define DWC3_DCTL                    0xC704
#define DWC3_DSTS                    0xC70C
#define DWC3_GUSB2PHYCFG             0xC200
#define GUSB2PHYCFG_PHYSOFTRST       BIT(31)
#define GUSB2PHYCFG_SUSPENDUSB2      BIT(6)

#define DWC3_GUSB3PIPECTL            0xC2C0
#define GUSB3PIPECTL_PHYSOFTRST      BIT(31)

#define DWC3_GEVNTADRLO              0xC400
#define DWC3_GEVNTADRHI              0xC404
#define DWC3_GEVNTSIZ                0xC408
#define DWC3_GEVNTCOUNT              0xC40C

#define DWC3_DEVTEN                  0xC708
#define DEVTEN_VENDEVTSTRCVDEN       BIT(12)
#define DEVTEN_ERRTICERREVTEN        BIT(9)
#define DEVTEN_SOFTEVTEN             BIT(7)
#define DEVTEN_U3L2L1SUSPEN          BIT(6)
#define DEVTEN_WKUPEVTEN             BIT(4)
#define DEVTEN_ULSTCNGEN             BIT(3)
#define DEVTEN_CONNECTDONEEVTEN      BIT(2)
#define DEVTEN_USBRSTEVTEN           BIT(1)
#define DEVTEN_DISSCONNEVTEN         BIT(0)

#define DWC3_DALEPENA                0xC720
#define DALEPENA_EP3_IN              BIT(7)
#define DALEPENA_EP3_OUT             BIT(6)
#define DALEPENA_EP2_IN              BIT(5)
#define DALEPENA_EP2_OUT             BIT(4)
#define DALEPENA_EP1_IN              BIT(3)
#define DALEPENA_EP1_OUT             BIT(2)
#define DALEPENA_EP0_IN              BIT(1)
#define DALEPENA_EP0_OUT             BIT(0)

#define DWC3_GCTL                    0xC110
#define GCTL_BYPSSETADDR             BIT(17)
#define GCTL_CORESOFTRESET           BIT(11)
#define GCTL_PRTCAPDIR_SHIFT         (12)
#define GCTL_PRTCAPDIR_MASK          (0x3 << GCTL_PRTCAPDIR_SHIFT)
#define GCTL_PRTCAPDIR(x)            ((x << GCTL_PRTCAPDIR_SHIFT) & GCTL_PRTCAPDIR_MASK)
#define GCTL_DSBLCLKGTNG             BIT(0)

#define DWC3_GTXFIFOSIZ_0            0xC300
#define DWC3_GRXFIFOSIZ_0            0xC380

/* Device Endpoint command */
#define DWC3_DEPCMD(x)               (0xC80C + (x * 0x10))
#define DEPCMD_COMMANDPARAM_SHIFT    (16)
#define DEPCMD_COMMANDPARAM_MASK     (0xffff << DEPCMD_COMMANDPARAM_SHIFT)
#define DEPCMD_COMMANDPARAM(x)       ((x << DEPCMD_COMMANDPARAM_SHIFT) & DEPCMD_COMMANDPARAM_MASK)
#define DEPCMD_CMDSTATUS_SHIFT       (12)
#define DEPCMD_CMDSTATUS_MASK        (0xf << DEPCMD_CMDSTATUS_SHIFT)
#define DEPCMD_CMDSTATUS(x)          ((x << DEPCMD_CMDSTATUS_SHIFT) & DEPCMD_CMDSTATUS_MASK)
#define DEPCMD_HIPRI_FORCERM         BIT(11)
#define DEPCMD_CMDACT                BIT(10)
#define DEPCMD_CMDIOC                BIT(8)
#define DEPCMD_CMDTYP_SHIFT          (0)
#define DEPCMD_CMDTYP_MASK           (0xf << DEPCMD_CMDTYP_SHIFT)
#define DEPCMD_CMDTYP(x)             ((x << DEPCMD_CMDTYP_SHIFT) & DEPCMD_CMDTYP_MASK)

#define CMDTYP_DEPCFG                1
#define CMDTYP_DEPXFERCFG            2
#define CMDTYP_DEPGETSTATE           3
#define CMDTYP_DEPSSTALL             4
#define CMDTYP_DEPCSTALL             5
#define CMDTYP_DEPSTRTXFER           6
#define CMDTYP_DEPUPDXFER            7
#define CMDTYP_DEPENDXFER            8
#define CMDTYP_DEPSTARTCFG           9

#define DWC3_DEPCMDPAR0(x)           (0xC808 + (x * 0x10))
#define DWC3_DEPCMDPAR1(x)           (0xC804 + (x * 0x10))
#define DWC3_DEPCMDPAR2(x)           (0xC800 + (x * 0x10))

/* Parameters for CMDTYP_DEPCFG */
#define DEPCFG_P0_CONFIG_ACT_SHIFT   (30)
#define DEPCFG_P0_CONFIG_ACT_MASK    (0x3 << DEPCFG_P0_CONFIG_ACT_SHIFT)
#define DEPCFG_P0_CONFIG_ACT(x)      ((x << DEPCFG_P0_CONFIG_ACT_SHIFT) & DEPCFG_P0_CONFIG_ACT_MASK)
#define DEPCFG_P0_BURST_SZ_SHIFT     (22)
#define DEPCFG_P0_BURST_SZ_MASK      (0xf << DEPCFG_P0_BURST_SZ_SHIFT)
#define DEPCFG_P0_BURST_SZ(x)        ((x << DEPCFG_P0_BURST_SZ_SHIFT) & DEPCFG_P0_BURST_SZ_MASK)
#define DEPCFG_P0_FIFO_NO_SHIFT      (17)
#define DEPCFG_P0_FIFO_NO_MASK       (0x1f << DEPCFG_P0_FIFO_NO_SHIFT)
#define DEPCFG_P0_FIFO_NO(x)         ((x << DEPCFG_P0_FIFO_NO_SHIFT) & DEPCFG_P0_FIFO_NO_MASK)
#define DEPCFG_P0_MPS_SHIFT          (3)
#define DEPCFG_P0_MPS_MASK           (0x7ff << DEPCFG_P0_MPS_SHIFT)
#define DEPCFG_P0_MPS(x)             ((x << DEPCFG_P0_MPS_SHIFT) & DEPCFG_P0_MPS_MASK)
#define DEPCFG_P0_EP_TYPE_SHIFT      (1)
#define DEPCFG_P0_EP_TYPE_MASK       (0x3 << DEPCFG_P0_EP_TYPE_SHIFT)
#define DEPCFG_P0_EP_TYPE(x)         ((x << DEPCFG_P0_EP_TYPE_SHIFT) & DEPCFG_P0_EP_TYPE_MASK)

#define DEPCFG_P1_FIFO_BASED         BIT(31)
#define DEPCFG_P1_BULK_BASED         BIT(30)
#define DEPCFG_P1_USB_EP_NO_SHIFT    (25)
#define DEPCFG_P1_USB_EP_NO_MASK     (0x1f << DEPCFG_P1_USB_EP_NO_SHIFT)
#define DEPCFG_P1_USB_EP_NO(x)       ((x << DEPCFG_P1_USB_EP_NO_SHIFT) & DEPCFG_P1_USB_EP_NO_MASK)
#define DEPCFG_P1_STRM_CAP           BIT(24)
#define DEPCFG_P1_BINTERVAL_M1_SHIFT (16)
#define DEPCFG_P1_BINTERVAL_M1_MASK  (0xff << DEPCFG_P1_BINTERVAL_M1_SHIFT)
#define DEPCFG_P1_BINTERVAL_M1(x) \
    ((x << DEPCFG_P1_BINTERVAL_M1_SHIFT) & DEPCFG_P1_BINTERVAL_M1_MASK)
#define DEPCFG_P1_EBC                   BIT(15)
#define DEPCFG_P1_EVT_STREAM_EN         BIT(13)
#define DEPCFG_P1_EVT_XFERNOTREADY_EN   BIT(10)
#define DEPCFG_P1_EVT_XFERINPROGRESS_EN BIT(9)
#define DEPCFG_P1_EVT_XFERCOMPLETE      BIT(8)
#define DEPCFG_P1_INTRNUM_SHIFT         (0)
#define DEPCFG_P1_INTRNUM_MASK          (0x1f << DEPCFG_P1_INTRNUM_SHIFT)
#define DEPCFG_P1_INTRNUM(x)            ((x << DEPCFG_P1_INTRNUM_SHIFT) & DEPCFG_P1_INTRNUM_MASK)

#define DWC3_GCTL_CORESOFTRESET         (1 << 11)
#define DWC3_GCTL_SCALEDOWN(n)          ((n) << 4)
#define DWC3_GCTL_SCALEDOWN_MASK        DWC3_GCTL_SCALEDOWN(3)

#endif
