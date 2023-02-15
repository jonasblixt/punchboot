/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <pb/io.h>
#include <plat/imx/usbdcd.h>

int imx_usbdcd_init(struct imx_usbdcd *dev, uintptr_t base)
{
    dev->base = base;

    /* Software reset */
    pb_write32(1 << 25, dev->base + IMX_USBDCD_CONTROL);

    /* BC 1.2*/
    pb_setbit32(1 << 17, dev->base + IMX_USBDCD_CONTROL);

    /* Configure clock to generate a 40ms pulse */
    pb_write32(0x1c1, dev->base + IMX_USBDCD_CLOCK);

    return PB_OK;
}

int imx_usbdcd_start(struct imx_usbdcd *dev)
{
    pb_setbit32(1 << 24, dev->base + IMX_USBDCD_CONTROL);
    return PB_OK;
}

int imx_usbdcd_poll_result(struct imx_usbdcd *dev,
                            enum usb_charger_type *result)
{
    uint32_t reg = 0;
    int rc = PB_OK;

    while (true)
    {
        reg = pb_read32(dev->base + IMX_USBDCD_STATUS);

        if (reg & (1 << 21)) /* Timeout */
        {
            rc = -PB_ERR;
            break;
        }

        if (reg & (1 << 20)) /* Error */
        {
            rc = -PB_ERR;
            break;
        }

        if (!(reg & (1 << 22))) /* Active */
        {
            break;
        }
    }


    int charger_kind = (reg >> 16) & 0x0f;

    switch (charger_kind)
    {
        case 0xe:
            (*result) = USB_CHARGER_CDP;
            LOG_DBG("CDP Port detected");
        break;
        case 0xf:
            (*result) = USB_CHARGER_DCP;
            LOG_DBG("DCP Port detected");
        break;
        case 0x9:
            (*result) = USB_CHARGER_SDP;
            LOG_DBG("SDP Port detected");
        break;
        default:
            (*result) = USB_CHARGER_UNKNOWN;
            LOG_DBG("Unknown port");
    }

    return rc;
}
