/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * ChipIdea (USB OTG core) device controller: registers, fields, queue heads
 * and transfer descriptors. Offsets are from the core base (USB_UOGn). The
 * names and layouts are those of the i.MX 6UL RM (54.6 register descriptions,
 * 54.4.5 device data structures); only what this driver uses is defined.
 */

#ifndef INCLUDE_DRIVERS_USB_IMX_CI_UDC_PRIVATE_H
#define INCLUDE_DRIVERS_USB_IMX_CI_UDC_PRIVATE_H

#include <pb/utils_def.h>
#include <stdint.h>

#define CI_FIELD(x, shift, mask)     (((uint32_t)(x) << (shift)) & (mask))

/* USB command */
#define CI_USBCMD                    0x140
#define USBCMD_ITC_SHIFT             16
#define USBCMD_ITC_MASK              (0xffu << USBCMD_ITC_SHIFT)
#define USBCMD_ITC(x)                CI_FIELD(x, USBCMD_ITC_SHIFT, USBCMD_ITC_MASK)
#define USBCMD_SUTW                  BIT_32(13) /* Setup tripwire */
#define USBCMD_RST                   BIT_32(1)
#define USBCMD_RS                    BIT_32(0) /* Run/stop, pulls D+ up in device mode */

/* USB status, write one to clear */
#define CI_USBSTS                    0x144
#define USBSTS_URI                   BIT_32(6) /* USB reset received */
#define USBSTS_UEI                   BIT_32(1) /* USB error interrupt */
#define USBSTS_ALL                   0xffffffffu

/* Device address */
#define CI_DEVICEADDR                0x154
#define DEVICEADDR_USBADR_SHIFT      25
#define DEVICEADDR_USBADR_MASK       (0x7fu << DEVICEADDR_USBADR_SHIFT)
#define DEVICEADDR_USBADR(x)         CI_FIELD(x, DEVICEADDR_USBADR_SHIFT, DEVICEADDR_USBADR_MASK)
#define DEVICEADDR_USBADRA           BIT_32(24) /* Take the address after the status stage */

/* Endpoint list address: the dQH array, 2 KiB aligned */
#define CI_ENDPTLISTADDR             0x158

/* Port status and control */
#define CI_PORTSC1                   0x184
#define PORTSC1_PR                   BIT_32(8) /* Port reset */

/* USB device mode */
#define CI_USBMODE                   0x1a8
#define USBMODE_SDIS                 BIT_32(4) /* Stream disable */
#define USBMODE_SLOM                 BIT_32(3) /* Setup lockout off, use USBCMD.SUTW */
#define USBMODE_CM_MASK              0x3
#define USBMODE_CM_DEVICE            2

/* Endpoint setup status: bit N, a setup packet on endpoint N; write one to clear */
#define CI_ENDPTSETUPSTAT            0x1ac
#define ENDPTSETUPSTAT_EP(n)         BIT_32(n)
#define ENDPTSETUPSTAT_ALL           0xffffu

/*
 * Endpoint prime, flush, status and complete share one layout: bit N is the
 * receive (OUT) buffer of endpoint N (PERB, FERB, ERBR, ERCE), bit 16 + N
 * the transmit (IN) buffer (PETB, FETB, ETBR, ETCE).
 */
#define CI_ENDPTPRIME                0x1b0
#define CI_ENDPTFLUSH                0x1b4
#define CI_ENDPTSTAT                 0x1b8
#define CI_ENDPTCOMPLETE             0x1bc
#define ENDPT_RX(n)                  BIT_32(n)
#define ENDPT_TX(n)                  BIT_32(16 + (n))
#define ENDPT_ALL                    0x00ff00ffu

/* Endpoint control, n = 1..7. The upper half is the IN direction, the lower the OUT. */
#define CI_ENDPTCTRL(n)              (0x1c0 + 4 * (n))
#define ENDPTCTRL_TX_MASK            0xffff0000u
#define ENDPTCTRL_TXE                BIT_32(23)
#define ENDPTCTRL_TXR                BIT_32(22) /* Data toggle reset */
#define ENDPTCTRL_TXT_SHIFT          18
#define ENDPTCTRL_TXT_MASK           (0x3u << ENDPTCTRL_TXT_SHIFT)
#define ENDPTCTRL_TXT(x)             CI_FIELD(x, ENDPTCTRL_TXT_SHIFT, ENDPTCTRL_TXT_MASK)
#define ENDPTCTRL_RX_MASK            0x0000ffffu
#define ENDPTCTRL_RXE                BIT_32(7)
#define ENDPTCTRL_RXR                BIT_32(6) /* Data toggle reset */
#define ENDPTCTRL_RXT_SHIFT          2
#define ENDPTCTRL_RXT_MASK           (0x3u << ENDPTCTRL_RXT_SHIFT)
#define ENDPTCTRL_RXT(x)             CI_FIELD(x, ENDPTCTRL_RXT_SHIFT, ENDPTCTRL_RXT_MASK)

/* Endpoint types in TXT and RXT */
#define ENDPTCTRL_TYPE_CONTROL       0
#define ENDPTCTRL_TYPE_ISO           1
#define ENDPTCTRL_TYPE_BULK          2
#define ENDPTCTRL_TYPE_INTR          3

/* Endpoint queue head capabilities (Table 54-57) */
#define DQH_CAP_ZLT                  BIT_32(29) /* No zero length packet after a full one */
#define DQH_CAP_MAX_PACKET_LEN_SHIFT 16
#define DQH_CAP_MAX_PACKET_LEN_MASK  (0x7ffu << DQH_CAP_MAX_PACKET_LEN_SHIFT)
#define DQH_CAP_MAX_PACKET_LEN(x) \
    CI_FIELD(x, DQH_CAP_MAX_PACKET_LEN_SHIFT, DQH_CAP_MAX_PACKET_LEN_MASK)
#define DQH_CAP_IOS                 BIT_32(15) /* Interrupt on setup */

/* Transfer descriptor next link pointer (Table 54-61) */
#define DTD_NEXT_T                  BIT_32(0) /* Terminate: no next dTD */

/* Transfer descriptor token (Table 54-62) */
#define DTD_TOKEN_TOTAL_BYTES_SHIFT 16
#define DTD_TOKEN_TOTAL_BYTES_MASK  (0x7fffu << DTD_TOKEN_TOTAL_BYTES_SHIFT)
#define DTD_TOKEN_TOTAL_BYTES(x) \
    CI_FIELD(x, DTD_TOKEN_TOTAL_BYTES_SHIFT, DTD_TOKEN_TOTAL_BYTES_MASK)
#define DTD_TOKEN_IOC                    BIT_32(15) /* Interrupt on complete */
#define DTD_TOKEN_STATUS_ACTIVE          BIT_32(7)
#define DTD_TOKEN_STATUS_HALTED          BIT_32(6)
#define DTD_TOKEN_STATUS_DATA_BUFFER_ERR BIT_32(5)
#define DTD_TOKEN_STATUS_TRANSACTION_ERR BIT_32(3)

/* Transfer descriptor buffer pointers (Table 54-63): five 4 KiB pages */
#define DTD_NO_OF_PAGES                  5
#define DTD_PAGE_SIZE                    4096
#define DTD_PAGE_OFFSET_MASK             (DTD_PAGE_SIZE - 1)
#define DTD_MAX_BYTES                    (DTD_NO_OF_PAGES * DTD_PAGE_SIZE)

/* Endpoint transfer descriptor (dTD, Table 54-60), 32 byte aligned */
struct imx_ci_udc_transfer_head {
    uint32_t next;
    uint32_t token;
    uint32_t page[DTD_NO_OF_PAGES];
    uint32_t pad;
} __attribute__((packed));

/* Endpoint queue head (dQH, Table 54-56), 64 byte aligned */
struct imx_ci_udc_queue_head {
    uint32_t caps;
    uint32_t current_dtd;
    /* Transfer overlay, from next through page[4] */
    uint32_t next;
    uint32_t token;
    uint32_t page[DTD_NO_OF_PAGES];
    uint32_t __reserved;
    uint32_t setup[2];
    uint32_t padding[4];
} __attribute__((packed));

#endif
