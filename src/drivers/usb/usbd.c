/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/utils_def.h>
#include <drivers/usb/usbd.h>

/* Defines for commands in setup packets */
#define USB_GET_DESCRIPTOR     0x8006
#define USB_SET_CONFIGURATION  0x0009
#define USB_SET_IDLE           0x210A
#define USB_SET_FEATURE        0x0003
#define USB_SET_ADDRESS        0x0005
#define USB_GET_STATUS         0x8000

static const struct usbd_hal_ops *hal_ops;
static const struct usbd_cls_config *cls_config;
static bool enumerated;

static int usbd_enumerate(struct usb_setup_packet *setup);

static const char *ep_to_str(usb_ep_t ep)
{
    switch (ep) {
    case USB_EP0_IN:
        return "EP0_IN";
    case USB_EP0_OUT:
        return "EP0_OUT";
    case USB_EP1_IN:
        return "EP1_IN";
    case USB_EP1_OUT:
        return "EP1_OUT";
    case USB_EP2_IN:
        return "EP2_IN";
    case USB_EP2_OUT:
        return "EP2_OUT";
    case USB_EP3_IN:
        return "EP3_IN";
    case USB_EP3_OUT:
        return "EP3_OUT";
    case USB_EP4_IN:
        return "EP4_IN";
    case USB_EP4_OUT:
        return "EP4_OUT";
    case USB_EP5_IN:
        return "EP5_IN";
    case USB_EP5_OUT:
        return "EP5_OUT";
    case USB_EP6_IN:
        return "EP6_IN";
    case USB_EP6_OUT:
        return "EP6_OUT";
    case USB_EP7_IN:
        return "EP7_IN";
    case USB_EP7_OUT:
        return "EP7_OUT";
        default:
            return "Unknown";
    };
}

static int wait_for_completion(usb_ep_t ep)
{
    int rc;

    do {
        rc = hal_ops->xfer_complete(ep);
    } while (rc == -PB_ERR_AGAIN);

    return rc;
}

static int ep0_tx(uintptr_t bfr, size_t length)
{
    int rc;

    rc = hal_ops->xfer_start(USB_EP0_IN, bfr, length);

    if (rc != PB_OK)
        return rc;

    rc = wait_for_completion(USB_EP0_IN);

    if (rc != PB_OK)
        return rc;

    rc = hal_ops->xfer_start(USB_EP0_OUT, 0, 0);

    return wait_for_completion(USB_EP0_OUT);
}

static int usbd_enumerate(struct usb_setup_packet *setup)
{
    int rc;
    uint16_t length = 0;
    uint16_t device_status = 0;
    uint16_t request;

    request = (setup->bRequestType << 8) | setup->bRequest;

    LOG_DBG("EP0 %x %x %ub", request, setup->wValue, setup->wLength);

    switch (request) {
        case USB_GET_DESCRIPTOR:
        {
            if (setup->wValue == 0x0600) {
                length = MIN(setup->wLength, (uint16_t) sizeof(cls_config->desc->qualifier));
                rc = ep0_tx((uintptr_t) &cls_config->desc->qualifier, length);
            } else if (setup->wValue == 0x0100) {
                length = MIN(setup->wLength, (uint16_t) sizeof(cls_config->desc->device));
                rc = ep0_tx((uintptr_t) &cls_config->desc->device, length);
            } else if (setup->wValue == 0x0200) {
                length = MIN(setup->wLength, cls_config->desc->config.wTotalLength);
                rc = ep0_tx((uintptr_t) &cls_config->desc->config, length);
            } else if (setup->wValue == 0x0A00) {
                length = MIN(setup->wLength, (uint16_t) cls_config->desc->interface.bLength);
                rc = ep0_tx((uintptr_t) &cls_config->desc->interface, length);
            } else if ((setup->wValue & 0xFF00) == 0x0300) {
                uint8_t str_desc_idx = (uint8_t) (setup->wValue & 0xFF);
                if (str_desc_idx == 0) {
                    length = MIN(setup->wLength, (uint16_t) sizeof(cls_config->desc->language));
                    rc = ep0_tx((uintptr_t) &cls_config->desc->language, length);
                } else {
                    struct usb_string_descriptor *str_desc = \
                               cls_config->get_string_descriptor(str_desc_idx);
                    length = MIN(setup->wLength, (uint16_t) str_desc->bLength);
                    rc = ep0_tx((uintptr_t) str_desc, length);
                }
            } else {
                LOG_ERR("Unhandled descriptor 0x%x", setup->wValue);
                rc = -PB_ERR_IO;
            }
        }
        break;
        case USB_SET_ADDRESS:
        {
            LOG_DBG("Set address 0x%02x", setup->wValue);
            rc = hal_ops->set_address(setup->wValue);

            if (rc != PB_OK)
                break;

            rc = hal_ops->xfer_start(USB_EP0_IN, 0, 0);

            if (rc != PB_OK)
                break;

            rc = wait_for_completion(USB_EP0_IN);
        }
        break;
        case USB_SET_CONFIGURATION:
        {
            LOG_DBG("Set configuration");
            rc = hal_ops->xfer_start(USB_EP0_IN, 0, 0);

            if (rc != PB_OK)
                break;

            rc = wait_for_completion(USB_EP0_IN);

            if (rc != PB_OK)
                break;

            for (int n = 0; n < cls_config->desc->interface.bNumEndpoints; n++) {
                usb_ep_t ep = (cls_config->desc->eps[n].bEndpointAddress & 0x7f) * 2;
                if (cls_config->desc->eps[n].bEndpointAddress & 0x80)
                    ep++;

                enum usb_ep_type ep_type = cls_config->desc->eps[n].bmAttributes + 1;
                uint16_t max_pkt_sz = cls_config->desc->eps[n].wMaxPacketSize;

                LOG_DBG("Configuring %s (0x%x), sz=%u, type=%u",
                                ep_to_str(ep),
                                cls_config->desc->eps[n].bEndpointAddress,
                                max_pkt_sz,
                                cls_config->desc->eps[n].bmAttributes);

                rc = hal_ops->configure_ep(ep, ep_type, max_pkt_sz);

                if (rc != PB_OK) {
                    LOG_ERR("Configuration failed (%i)", rc);
                    break;
                }
            }

            enumerated = true;
        }
        break;
        case USB_SET_IDLE:
        {
            LOG_DBG("Set idle");
            rc = ep0_tx(0, 0);
        }
        break;
        case USB_GET_STATUS:
        {
            LOG_DBG("Get status");
            rc = ep0_tx((uintptr_t) &device_status, sizeof(device_status));
        }
        break;
        default:
        {
            LOG_ERR("EP0 Unhandled request %x", request);
            LOG_ERR(" bRequestType = 0x%x", setup->bRequestType);
            LOG_ERR(" bRequest = 0x%x", setup->bRequest);
            LOG_ERR(" wValue = 0x%x", setup->wValue);
            LOG_ERR(" wIndex = 0x%x", setup->wIndex);
            LOG_ERR(" wLength = 0x%x", setup->wLength);
            rc = -PB_ERR;
        }
    }

    return rc;
}

int usbd_init_hal_ops(const struct usbd_hal_ops *ops)
{
    hal_ops = ops;

    if (!hal_ops->xfer_start ||
        !hal_ops->xfer_complete ||
        !hal_ops->init ||
        !hal_ops->configure_ep ||
        !hal_ops->set_address) {
        return -PB_ERR_PARAM;
    }

    return PB_OK;
}

int usbd_init_cls(const struct usbd_cls_config *cfg)
{
    cls_config = cfg;

    if (cfg == NULL)
        return -PB_ERR_PARAM;

    return PB_OK;
}

int usbd_init(void)
{
    return hal_ops->init();
}

int usbd_connect(void)
{
    int rc;
    struct usb_setup_packet pkt;

    /**
     * 'poll_setup_pkt' returns -PB_ERR_AGAIN when there was no
     * setup packet to poll.
     *
     * Code that call's 'usbd_connect' is expected to call usbd_connect
     * repeatedly until enumeration is complete or a user defined timeout
     * has expired.
     */
    rc = hal_ops->poll_setup_pkt(&pkt);

    if (rc != PB_OK)
        return rc;

    rc = usbd_enumerate(&pkt);

    if (rc != PB_OK)
        return rc;

    if (!enumerated)
        return -PB_ERR_AGAIN;
    else
        return PB_OK;
}

int usbd_disconnect(void)
{
    if (hal_ops->stop)
        return hal_ops->stop();
    else
        return PB_OK;
}

int usbd_xfer(usb_ep_t ep, uintptr_t buf, size_t length)
{
    int rc;
    struct usb_setup_packet pkt;

restart_xfer:
    rc = hal_ops->xfer_start(ep, buf, length);

    if (rc != PB_OK)
        return rc;

    /* Here we can block for a long time so we need to service WDT and
     * check if there are any new control transfers on EP0 */
    do {
        if (hal_ops->poll_setup_pkt(&pkt) == PB_OK) {
            hal_ops->xfer_cancel(ep);

            rc = usbd_enumerate(&pkt);

            if (rc != PB_OK)
                return rc;
            goto restart_xfer;
        }
        rc = hal_ops->xfer_complete(ep);
        plat_wdog_kick();
    } while (rc == -PB_ERR_AGAIN);

    return rc;
}
