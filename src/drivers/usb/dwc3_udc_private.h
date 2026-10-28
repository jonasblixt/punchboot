/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Synopsys DWC_usb3 device controller: registers, fields, events and TRBs.
 * Offsets are from the controller base. The field layouts are those of the
 * i.MX 8M Plus RM (11.2.5.1 register descriptions, 11.2.2.3 TRBs, 11.2.2.5
 * device programming model, 11.2.2.7 endpoint commands); only the fields
 * this driver uses are defined.
 */

#ifndef INCLUDE_DRIVERS_USB_DWC3_UDC_PRIVATE_H
#define INCLUDE_DRIVERS_USB_DWC3_UDC_PRIVATE_H

#include <pb/utils_def.h>
#include <stdint.h>

#define DWC3_FIELD(x, shift, mask)     (((uint32_t)(x) << (shift)) & (mask))
#define DWC3_FIELD_GET(v, shift, mask) (((v) & (mask)) >> (shift))

/* Capability length / HCI version, the first register of the xHCI view */
#define DWC3_CAPLENGTH                 0x0000
#define CAPLENGTH_CAPLENGTH_MASK       0xff
#define CAPLENGTH_HCIVERSION_SHIFT     16
#define CAPLENGTH_HCIVERSION_MASK      (0xffffu << CAPLENGTH_HCIVERSION_SHIFT)

/* Global core control */
#define DWC3_GCTL                      0xC110
#define GCTL_BYPSSETADDR               BIT_32(17)
#define GCTL_PRTCAPDIR_SHIFT           12
#define GCTL_PRTCAPDIR_MASK            (0x3 << GCTL_PRTCAPDIR_SHIFT)
#define GCTL_PRTCAPDIR(x)              DWC3_FIELD(x, GCTL_PRTCAPDIR_SHIFT, GCTL_PRTCAPDIR_MASK)
#define GCTL_PRTCAPDIR_HOST            1
#define GCTL_PRTCAPDIR_DEVICE          2
#define GCTL_CORESOFTRESET             BIT_32(11)
#define GCTL_RAMCLKSEL_SHIFT           6
#define GCTL_RAMCLKSEL_MASK            (0x3 << GCTL_RAMCLKSEL_SHIFT)
#define GCTL_RAMCLKSEL(x)              DWC3_FIELD(x, GCTL_RAMCLKSEL_SHIFT, GCTL_RAMCLKSEL_MASK)
#define GCTL_RAMCLKSEL_BUS_CLK         0
#define GCTL_SCALEDOWN_SHIFT           4
#define GCTL_SCALEDOWN_MASK            (0x3 << GCTL_SCALEDOWN_SHIFT)
#define GCTL_DSBLCLKGTNG               BIT_32(0)

/* Global USB2 PHY configuration (port 0) */
#define DWC3_GUSB2PHYCFG               0xC200
#define GUSB2PHYCFG_PHYSOFTRST         BIT_32(31)
#define GUSB2PHYCFG_SUSPENDUSB20       BIT_32(6) /* Must be 0 while endpoint commands are issued */

/* Global USB3 PIPE control (port 0) */
#define DWC3_GUSB3PIPECTL              0xC2C0
#define GUSB3PIPECTL_PHYSOFTRST        BIT_32(31)

/* Event buffer 0 */
#define DWC3_GEVNTADRLO                0xC400
#define DWC3_GEVNTADRHI                0xC404
#define DWC3_GEVNTSIZ                  0xC408
#define GEVNTSIZ_EVNTINTRPTMASK        BIT_32(31) /* No interrupt line, the driver polls */
#define GEVNTSIZ_EVENTSIZ_MASK         0xffff
#define DWC3_GEVNTCOUNT                0xC40C
#define GEVNTCOUNT_EVNTCOUNT_MASK      0xffff /* Bytes of events pending / processed */

/* Device configuration */
#define DWC3_DCFG                      0xC700
#define DCFG_DEVADDR_SHIFT             3
#define DCFG_DEVADDR_MASK              (0x7f << DCFG_DEVADDR_SHIFT)
#define DCFG_DEVADDR(x)                DWC3_FIELD(x, DCFG_DEVADDR_SHIFT, DCFG_DEVADDR_MASK)
#define DCFG_DEVSPD_SHIFT              0
#define DCFG_DEVSPD_MASK               (0x7 << DCFG_DEVSPD_SHIFT)
#define DCFG_DEVSPD(x)                 DWC3_FIELD(x, DCFG_DEVSPD_SHIFT, DCFG_DEVSPD_MASK)

/* Speeds in DCFG.DEVSPD and DSTS.CONNECTSPD */
#define DWC3_SPEED_HIGH                0
#define DWC3_SPEED_FULL                1
#define DWC3_SPEED_SUPER               4

/* Device control */
#define DWC3_DCTL                      0xC704
#define DCTL_RUN_STOP                  BIT_32(31)
#define DCTL_CSFTRST                   BIT_32(30)
#define DCTL_ULSTCHNGREQ_SHIFT         5
#define DCTL_ULSTCHNGREQ_MASK          (0xf << DCTL_ULSTCHNGREQ_SHIFT)
#define DCTL_ULSTCHNGREQ(x)            DWC3_FIELD(x, DCTL_ULSTCHNGREQ_SHIFT, DCTL_ULSTCHNGREQ_MASK)
#define DCTL_ULSTCHNGREQ_RX_DET        5 /* After a disconnect (RM 11.2.2.5.1.7) */

/* Device event enable */
#define DWC3_DEVTEN                    0xC708
#define DEVTEN_ERRTICERREVTEN          BIT_32(9)
#define DEVTEN_ULSTCNGEN               BIT_32(3)
#define DEVTEN_CONNECTDONEEVTEN        BIT_32(2)
#define DEVTEN_USBRSTEVTEN             BIT_32(1)
#define DEVTEN_DISSCONNEVTEN           BIT_32(0)

/* Device status */
#define DWC3_DSTS                      0xC70C
#define DSTS_DEVCTRLHLT                BIT_32(22)
#define DSTS_CONNECTSPD_SHIFT          0
#define DSTS_CONNECTSPD_MASK           (0x7 << DSTS_CONNECTSPD_SHIFT)

/* Active USB endpoint enable: one bit per physical endpoint */
#define DWC3_DALEPENA                  0xC720
#define DALEPENA_EP(n)                 BIT_32(n)

/* Physical endpoint n command and its parameters */
#define DWC3_DEPCMDPAR2(n)             (0xC800 + 0x10 * (n))
#define DWC3_DEPCMDPAR1(n)             (0xC804 + 0x10 * (n))
#define DWC3_DEPCMDPAR0(n)             (0xC808 + 0x10 * (n))
#define DWC3_DEPCMD(n)                 (0xC80C + 0x10 * (n))
#define DEPCMD_COMMANDPARAM_SHIFT      16
#define DEPCMD_COMMANDPARAM_MASK       (0xffffu << DEPCMD_COMMANDPARAM_SHIFT)
#define DEPCMD_COMMANDPARAM(x)  DWC3_FIELD(x, DEPCMD_COMMANDPARAM_SHIFT, DEPCMD_COMMANDPARAM_MASK)
/* After Start Transfer: the transfer resource index, for Update/End Transfer */
#define DEPCMD_XFERRSCIDX_SHIFT 16
#define DEPCMD_XFERRSCIDX_MASK  (0x7f << DEPCMD_XFERRSCIDX_SHIFT)
#define DEPCMD_CMDSTATUS_SHIFT  12
#define DEPCMD_CMDSTATUS_MASK   (0xf << DEPCMD_CMDSTATUS_SHIFT)
#define DEPCMD_HIPRI_FORCERM    BIT_32(11)
#define DEPCMD_CMDACT           BIT_32(10)
#define DEPCMD_CMDIOC           BIT_32(8)
#define DEPCMD_CMDTYP_MASK      0xf

#define DEPCMD_DEPCFG           1
#define DEPCMD_DEPXFERCFG       2
#define DEPCMD_DEPSSTALL        4
#define DEPCMD_DEPCSTALL        5
#define DEPCMD_DEPSTRTXFER      6
#define DEPCMD_DEPENDXFER       8
#define DEPCMD_DEPSTARTCFG      9

/* DEPSTARTCFG transfer resource index: after power on, and at SetConfiguration */
#define DEPSTARTCFG_XFERRSCIDX_INIT   0
#define DEPSTARTCFG_XFERRSCIDX_CONFIG 2

/* DEPCFG parameter 0 */
#define DEPCFG_P0_CONFIG_ACT_SHIFT    30
#define DEPCFG_P0_CONFIG_ACT_MASK     (0x3u << DEPCFG_P0_CONFIG_ACT_SHIFT)
#define DEPCFG_P0_CONFIG_ACT(x) DWC3_FIELD(x, DEPCFG_P0_CONFIG_ACT_SHIFT, DEPCFG_P0_CONFIG_ACT_MASK)
#define DEPCFG_CONFIG_ACT_INIT  0
#define DEPCFG_CONFIG_ACT_MODIFY 2
#define DEPCFG_P0_BURST_SZ_SHIFT 22
#define DEPCFG_P0_BURST_SZ_MASK  (0xf << DEPCFG_P0_BURST_SZ_SHIFT)
#define DEPCFG_P0_BURST_SZ(x)    DWC3_FIELD(x, DEPCFG_P0_BURST_SZ_SHIFT, DEPCFG_P0_BURST_SZ_MASK)
#define DEPCFG_P0_FIFO_NO_SHIFT  17
#define DEPCFG_P0_FIFO_NO_MASK   (0x1f << DEPCFG_P0_FIFO_NO_SHIFT)
#define DEPCFG_P0_FIFO_NO(x)     DWC3_FIELD(x, DEPCFG_P0_FIFO_NO_SHIFT, DEPCFG_P0_FIFO_NO_MASK)
#define DEPCFG_P0_MPS_SHIFT      3
#define DEPCFG_P0_MPS_MASK       (0x7ff << DEPCFG_P0_MPS_SHIFT)
#define DEPCFG_P0_MPS(x)         DWC3_FIELD(x, DEPCFG_P0_MPS_SHIFT, DEPCFG_P0_MPS_MASK)
#define DEPCFG_P0_EP_TYPE_SHIFT  1
#define DEPCFG_P0_EP_TYPE_MASK   (0x3 << DEPCFG_P0_EP_TYPE_SHIFT)
#define DEPCFG_P0_EP_TYPE(x)     DWC3_FIELD(x, DEPCFG_P0_EP_TYPE_SHIFT, DEPCFG_P0_EP_TYPE_MASK)
/* EPType: the same encoding as bmAttributes[1:0] in an endpoint descriptor */
#define DEPCFG_EP_TYPE_CONTROL   0
#define DEPCFG_EP_TYPE_MASK      0x3

/* DEPCFG parameter 1 */
#define DEPCFG_P1_USB_EP_NO_SHIFT \
    25 /* [29:26] endpoint number, [25] direction: the physical number */
#define DEPCFG_P1_USB_EP_NO_MASK  (0x1f << DEPCFG_P1_USB_EP_NO_SHIFT)
#define DEPCFG_P1_USB_EP_NO(x)    DWC3_FIELD(x, DEPCFG_P1_USB_EP_NO_SHIFT, DEPCFG_P1_USB_EP_NO_MASK)
#define DEPCFG_P1_XFERNOTREADY_EN BIT_32(10)
#define DEPCFG_P1_XFERINPROGRESS_EN   BIT_32(9)
#define DEPCFG_P1_XFERCOMPLETE_EN     BIT_32(8)

/* DEPXFERCFG parameter 0: one transfer resource per endpoint */
#define DEPXFERCFG_P0_NUMXFERRES      1

/* Events (RM 11.2.2.5.2.2). Bit 0: 1 = device event (DEVT), 0 = endpoint event (DEPEVT) */
#define DWC3_EVT_DEVICE               BIT_32(0)

#define DEVT_TYPE_SHIFT               8
#define DEVT_TYPE_MASK                (0xf << DEVT_TYPE_SHIFT)
#define DEVT_DISCONNECT               0
#define DEVT_USBRST                   1
#define DEVT_CONNECTDONE              2
#define DEVT_ULSTCHNG                 3
#define DEVT_ERRTICERR                9
#define DEVT_EVNTOVERFLOW             11

#define DEPEVT_EP_SHIFT               1
#define DEPEVT_EP_MASK                (0x1f << DEPEVT_EP_SHIFT)
#define DEPEVT_TYPE_SHIFT             6
#define DEPEVT_TYPE_MASK              (0xf << DEPEVT_TYPE_SHIFT)
#define DEPEVT_XFERCOMPLETE           1
#define DEPEVT_XFERINPROGRESS         2
#define DEPEVT_XFERNOTREADY           3
#define DEPEVT_EPCMDCMPLT             7
#define DEPEVT_STATUS_SHIFT           12
#define DEPEVT_STATUS_MASK            (0xf << DEPEVT_STATUS_SHIFT)
/* XferNotReady on a control endpoint: the stage the host asked for */
#define DEPEVT_STATUS_CTRL_STAGE_MASK 0x3
#define DEPEVT_CTRL_STAGE_DATA        1
#define DEPEVT_CTRL_STAGE_STATUS      2

/* TRB (RM 11.2.2.3): 16 bytes, 16-byte aligned */
#define TRB_SIZE_BUFSIZ_MASK          0xffffff
#define TRB_SIZE_TRBSTS_SHIFT         28
#define TRB_SIZE_TRBSTS_MASK          (0xfu << TRB_SIZE_TRBSTS_SHIFT)
#define TRB_TRBSTS_SETUP_PENDING      2

#define TRB_CTRL_IOC                  BIT_32(11)
#define TRB_CTRL_ISP_IMI              BIT_32(10)
#define TRB_CTRL_TRBCTL_SHIFT         4
#define TRB_CTRL_TRBCTL_MASK          (0x3f << TRB_CTRL_TRBCTL_SHIFT)
#define TRB_CTRL_TRBCTL(x)            DWC3_FIELD(x, TRB_CTRL_TRBCTL_SHIFT, TRB_CTRL_TRBCTL_MASK)
#define TRB_CTRL_CSP                  BIT_32(3)
#define TRB_CTRL_CHN                  BIT_32(2)
#define TRB_CTRL_LST                  BIT_32(1)
#define TRB_CTRL_HWO                  BIT_32(0)

#define TRBCTL_NORMAL                 1
#define TRBCTL_CONTROL_SETUP          2
#define TRBCTL_CONTROL_STATUS2        3 /* Status of a control transfer without data stage */
#define TRBCTL_CONTROL_STATUS3        4 /* Status of a control transfer with data stage */
#define TRBCTL_CONTROL_DATA           5 /* First TRB of a data stage */

#endif
