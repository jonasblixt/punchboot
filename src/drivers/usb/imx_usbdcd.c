/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/usb/imx_usbdcd.h>
#include <pb/mmio.h>
#include <pb/pb.h>

/* Registers */
#define IMX_USBDCD_CONTROL         (0x00)
#define CONTROL_SR                 BIT(25)
#define CONTROL_BC12               BIT(17)
#define CONTROL_START              BIT(24)

#define IMX_USBDCD_CLOCK           (0x04)
#define CLOCK_UNIT                 BIT(0)
#define CLOCK_SPEED(x)             ((x & 0x3ff) << 2)

#define IMX_USBDCD_STATUS          (0x08)
#define STATUS_ACTIVE              BIT(22)
#define STATUS_TO                  BIT(21)
#define STATUS_ERR                 BIT(20)

#define IMX_USBDCD_SIGNAL_OVERRIDE (0x0c)
#define IMX_USBDCD_TIMER0          (0x10)
#define IMX_USBDCD_TIMER1          (0x14)
#define IMX_USBDCD_TIMER2_BC11     (0x18)
#define IMX_USBDCD_TIMER2_BC12     (0x18)

static uintptr_t base;

int imx_usbdcd_init(uintptr_t base_)
{
    base = base_;

    /* Software reset */
    mmio_write_32(base + IMX_USBDCD_CONTROL, CONTROL_SR);

    /* BC 1.2*/
    mmio_clrsetbits_32(base + IMX_USBDCD_CONTROL, 0, CONTROL_BC12);

    /* Configure clock to generate a 40ms pulse */
    mmio_write_32(base + IMX_USBDCD_CLOCK, CLOCK_SPEED(0x70) | CLOCK_UNIT);

    return PB_OK;
}

int imx_usbdcd_start(void)
{
    mmio_clrsetbits_32(base + IMX_USBDCD_CONTROL, 0, CONTROL_START);
    return PB_OK;
}

enum usb_charger_type imx_usbdcd_poll_result(void)
{
    uint32_t reg = 0;

    while (true) {
        reg = mmio_read_32(base + IMX_USBDCD_STATUS);

        if (reg & STATUS_TO) { /* Timeout */
            return -PB_ERR_TIMEOUT;
        }

        if (reg & STATUS_ERR) { /* Error */
            return -PB_ERR_IO;
        }

        if (!(reg & STATUS_ACTIVE)) { /* Active */
            break;
        }
    }

    int charger_kind = (reg >> 16) & 0x0f;

    switch (charger_kind) {
    case 0xe:
        LOG_DBG("CDP Port detected");
        return USB_CHARGER_CDP;
        break;
    case 0xf:
        LOG_DBG("DCP Port detected");
        return USB_CHARGER_DCP;
        break;
    case 0x9:
        LOG_DBG("SDP Port detected");
        return USB_CHARGER_SDP;
        break;
    default:
        LOG_DBG("Unknown port");
        return USB_CHARGER_INVALID;
    }
}
