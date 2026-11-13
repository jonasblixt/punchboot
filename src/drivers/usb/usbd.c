/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/usb/usbd.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/utils_def.h>
#include <stdbool.h>
#include <stdint.h>

/* Defines for commands in setup packets */
#define USB_GET_DESCRIPTOR    0x8006
#define USB_SET_CONFIGURATION 0x0009
#define USB_SET_IDLE          0x210A
#define USB_SET_FEATURE       0x0003
#define USB_SET_ADDRESS       0x0005
#define USB_GET_STATUS        0x8000
#define USB_GET_VENDOR        0xc001

static const struct usbd_hal_ops *hal_ops;
static const struct usbd_cls_config *cls_config;
static bool enumerated;
/* The controller has one transfer for all its endpoints (usbd_hal_one_xfer()) */
static bool hal_one_xfer;
static usb_ep_t cur_xfer_ep;
static void *cur_xfer_buf;
static size_t cur_xfer_length;

/* Microsoft OS Descriptor
 *
 * When a new USB device enumerates on Windows the system will ask for string
 * descriptor 0xEE, Which is Windows specific, if the string descriptor contains
 * 'MSFT100' it will issue a vendor specific request for a 'Microsoft Compatible ID Feature
 * Descriptor'.
 *
 * With these two in place Windows will automatically bind the device to WinUSB and
 * in turn we can use libusb directly.
 *
 * Historically this was handled by the windows binary installer that bundeled
 * libwdi.
 *
 * It's explained in greater detail here:
 *     https://github.com/pbatard/libwdi/wiki/WCID-Devices
 */
static const struct usb_msft_os_descriptor msft_os_descriptor = {
    .bLength = 18,
    .bDescriptorType = 3,
    .signature = { 0x4D, 0x00, 0x53, 0x00, 0x46, 0x00, 0x54, 0x00, 0x31, 0x00, 0x30, 0x00, 0x30, 0x00, },
    .bVendorCode = 1,
    .bPad = 0,
};

static const struct usb_msft_comp_id_feature msft_comp_id_feature = {
    .length = 40,
    .bcdVersion = { 0x00, 0x01 },
    .wCompIDidx = 4,
    .bNumSel = 1,
    .rz1 = { 0 },
    .bIfaceNo = 0,
    .rz2 = 0,
    .compatibleId = { 0x57, 0x49, 0x4E, 0x55, 0x53, 0x42, 0x00, 0x00, },
    .subCompatibleId = { 0 },
    .rz3 = { 0 },
};

static int usbd_enumerate(struct usb_setup_packet *setup);

const char *ep_to_str(usb_ep_t ep)
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

static int ep0_tx(const void *bfr, size_t length)
{
    int rc;

    rc = hal_ops->xfer_start(USB_EP0_IN, (void *)bfr, length);

    if (rc != PB_OK)
        return rc;

    rc = wait_for_completion(USB_EP0_IN);

    if (rc != PB_OK)
        return rc;

    return hal_ops->ep0_xfer_zlp(USB_EP0_OUT);
}

static int usbd_enumerate(struct usb_setup_packet *setup)
{
    int rc = -PB_ERR;
    uint16_t length = 0;
    uint16_t device_status = 0;
    uint16_t request;

    request = (setup->bRequestType << 8) | setup->bRequest;

    LOG_DBG("EP0 %x %x %ub", request, setup->wValue, setup->wLength);

    switch (request) {
    case USB_GET_DESCRIPTOR: {
        if (setup->wValue == USB_DESC_VALUE(USB_DESC_TYPE_QUALIFIER, 0)) {
            length = MIN(setup->wLength, (uint16_t)sizeof(cls_config->desc->qualifier));
            rc = ep0_tx(&cls_config->desc->qualifier, length);
        } else if (setup->wValue == USB_DESC_VALUE(USB_DESC_TYPE_DEVICE, 0)) {
            length = MIN(setup->wLength, (uint16_t)sizeof(cls_config->desc->device));
            rc = ep0_tx(&cls_config->desc->device, length);
        } else if (setup->wValue == USB_DESC_VALUE(USB_DESC_TYPE_CONFIG, 0)) {
            length = MIN(setup->wLength, cls_config->desc->config.wTotalLength);
            rc = ep0_tx(&cls_config->desc->config, length);
        } else if (setup->wValue == USB_DESC_VALUE(USB_DESC_TYPE_DEBUG, 0)) {
            length = MIN(setup->wLength, (uint16_t)cls_config->desc->interface.bLength);
            rc = ep0_tx(&cls_config->desc->interface, length);
        } else if ((setup->wValue & USB_DESC_TYPE_MASK) ==
                   USB_DESC_VALUE(USB_DESC_TYPE_STRING, 0)) {
            uint8_t str_desc_idx = (uint8_t)(setup->wValue & USB_DESC_INDEX_MASK);
            if (str_desc_idx == 0) {
                length = MIN(setup->wLength, (uint16_t)sizeof(cls_config->desc->language));
                rc = ep0_tx(&cls_config->desc->language, length);
            } else if (str_desc_idx == USB_DESC_STRING_MS_OS) {
                /* Microsoft OS Descriptor */
                length = MIN(setup->wLength, (uint16_t)sizeof(msft_os_descriptor));
                rc = ep0_tx(&msft_os_descriptor, length);
            } else {
                struct usb_string_descriptor *str_desc =
                    cls_config->get_string_descriptor(str_desc_idx);
                length = MIN(setup->wLength, (uint16_t)str_desc->bLength);
                rc = ep0_tx(str_desc, length);
            }
        } else {
            LOG_ERR("Unhandled descriptor 0x%x", setup->wValue);
            rc = -PB_ERR_IO;
        }
    } break;
    case USB_SET_ADDRESS: {
        rc = hal_ops->set_address(setup->wValue);

        if (rc != PB_OK)
            break;

        hal_ops->ep0_xfer_zlp(USB_EP0_IN);
    } break;
    case USB_SET_CONFIGURATION: {
        LOG_DBG("Set configuration");

        /* The one configuration there is, or 0 (usb.md 3.2) */
        if (setup->wValue == 0) {
            /* Deconfigured: not ready until the host configures the device */
            enumerated = false;
            hal_ops->ep0_xfer_zlp(USB_EP0_IN);
            rc = PB_OK;
            break;
        }

        if (setup->wValue != cls_config->desc->config.bConfigurationValue) {
            /* Refused: nothing is set up and there is no status stage. It
             * is to be stalled, which no controller can yet (F-138) */
            LOG_ERR("No configuration %u", setup->wValue);
            rc = PB_OK;
            break;
        }

        hal_ops->ep0_xfer_zlp(USB_EP0_IN);

        rc = hal_ops->set_configuration(cls_config->desc->eps,
                                        cls_config->desc->interface.bNumEndpoints);

        if (rc != PB_OK) {
            LOG_ERR("Configuration failed (%i)", rc);
            // Here we should stall the endpoint
            break;
        }
        enumerated = true;
    } break;
    case USB_SET_IDLE: {
        LOG_DBG("Set idle");
        hal_ops->ep0_xfer_zlp(USB_EP0_IN);
        hal_ops->ep0_xfer_zlp(USB_EP0_OUT);
    } break;
    case USB_GET_STATUS: {
        LOG_DBG("Get status");
        rc = ep0_tx(&device_status, sizeof(device_status));
    } break;
    case USB_GET_VENDOR: {
        LOG_DBG("Get vendor specific descriptor");
        if (setup->wIndex == USB_MS_EXT_COMPAT_ID) {
            length = MIN(setup->wLength, (uint16_t)sizeof(msft_comp_id_feature));
            rc = ep0_tx(&msft_comp_id_feature, length);
        }
    } break;
    default: {
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
    hal_ops = NULL;
    hal_one_xfer = false;

    /* Every operation is mandatory, and none is called that wasn't checked
     * here (usb.md 4): a driver that leaves one out isn't registered */
    if (ops == NULL || !ops->init || !ops->stop || !ops->xfer_start || !ops->xfer_complete ||
        !ops->xfer_cancel || !ops->poll_setup_pkt || !ops->set_configuration || !ops->set_address ||
        !ops->ep0_xfer_zlp) {
        return -PB_ERR_PARAM;
    }

    hal_ops = ops;

    return PB_OK;
}

void usbd_hal_one_xfer(void)
{
    hal_one_xfer = true;
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
    if (hal_ops == NULL)
        return -PB_ERR_IO;

    /* The controller starts with nothing configured (usb.md 4.1), and so
     * does the stack: ready is when the host has configured the device */
    enumerated = false;

    return hal_ops->init();
}

int usbd_connect(void)
{
    int rc;
    struct usb_setup_packet pkt;

    if (hal_ops == NULL)
        return -PB_ERR_IO;

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
    if (hal_ops == NULL)
        return -PB_ERR_IO;

    enumerated = false;

    if (hal_ops->stop)
        return hal_ops->stop();
    else
        return PB_OK;
}

int usbd_xfer_start(usb_ep_t ep, void *buf, size_t length)
{
    if (hal_ops == NULL)
        return -PB_ERR_IO;

    cur_xfer_ep = ep;
    cur_xfer_buf = buf;
    cur_xfer_length = length;

    return hal_ops->xfer_start(ep, buf, length);
}

int usbd_xfer_cancel(void)
{
    if (hal_ops == NULL)
        return -PB_ERR_IO;

    hal_ops->xfer_cancel(cur_xfer_ep);
    return PB_OK;
}

/* The two requests of an enumeration that change what the device is to the
 * host: its address and its configuration */
static bool usbd_host_starts_over(const struct usb_setup_packet *setup)
{
    uint16_t request = (setup->bRequestType << 8) | setup->bRequest;

    return request == USB_SET_ADDRESS || request == USB_SET_CONFIGURATION;
}

/* The largest packet of one of the class's endpoints, 0 when it has no such */
static size_t usbd_max_packet(usb_ep_t ep)
{
    uint8_t addr = (uint8_t)(ep >> 1) | ((ep & 1) ? USB_EP_ADDR_IN : 0);

    for (unsigned int i = 0; i < cls_config->desc->interface.bNumEndpoints; i++) {
        if (cls_config->desc->eps[i].bEndpointAddress == addr)
            return cls_config->desc->eps[i].wMaxPacketSize;
    }

    return 0;
}

/*
 * A request while a bulk transfer runs, on a controller with one transfer
 * for all its endpoints: answering on endpoint 0 ends the bulk transfer, so
 * it can't go on where it was (usb.md 3.2, 8).
 *
 *  - A transfer that is done isn't touched: its result is the caller's.
 *  - One of a packet at most is given up and started again after the
 *    answer. Nothing of it has moved: a packet arrives whole or not at all.
 *    It is what the device waits in between two commands.
 *  - A longer one fails. A part of it may have moved, and starting it again
 *    from its first byte would lose that part without a word (F-203).
 */
static int usbd_xfer_poll_one_xfer(struct usb_setup_packet *pkt)
{
    int rc;

    rc = hal_ops->xfer_complete(cur_xfer_ep);

    if (rc == PB_OK)
        return usbd_enumerate(pkt);

    if (rc != -PB_ERR_AGAIN)
        return rc;

    if (cur_xfer_length > usbd_max_packet(cur_xfer_ep)) {
        LOG_ERR("EP0 request during a transfer of %zu bytes on %s",
                cur_xfer_length,
                ep_to_str(cur_xfer_ep));
        return -PB_ERR_IO;
    }

    hal_ops->xfer_cancel(cur_xfer_ep);

    rc = usbd_enumerate(pkt);

    if (rc != PB_OK)
        return rc;

    rc = hal_ops->xfer_start(cur_xfer_ep, cur_xfer_buf, cur_xfer_length);

    if (rc != PB_OK)
        return rc;

    return hal_ops->xfer_complete(cur_xfer_ep);
}

static int usbd_xfer_poll(void)
{
    int rc;
    struct usb_setup_packet pkt;

    if (hal_ops->poll_setup_pkt(&pkt) == PB_OK) {
        /* A host that sets an address or a configuration inside a session
         * enumerates the device again: it was plugged in again or the bus
         * was reset, whether the controller reported that or not, or the
         * host deconfigures it. The session is lost (usb.md 2.4, 3.2). The
         * request gets no answer: the address and the configuration are the
         * next session's, and the device leaves the bus when the transport
         * is restarted (usb.md 2.1) */
        if (usbd_host_starts_over(&pkt)) {
            LOG_ERR("The host enumerates again (%x %x)", pkt.bRequest, pkt.wValue);
            return -PB_ERR_IO;
        }

        if (hal_one_xfer)
            return usbd_xfer_poll_one_xfer(&pkt);

        /* A request doesn't disturb a bulk transfer (usb.md 3.2): it is
         * answered on endpoint 0 and the transfer goes on where it was */
        rc = usbd_enumerate(&pkt);

        if (rc != PB_OK)
            return rc;
    }

    return hal_ops->xfer_complete(cur_xfer_ep);
}

int usbd_xfer_complete(void)
{
    int rc;

    if (hal_ops == NULL)
        return -PB_ERR_IO;

    rc = usbd_xfer_poll();

    /* Every error is "lost" (usb.md 2.4): the bus was reset or the host is
     * gone. Nothing is configured after it (usb.md 3.2) */
    if (rc != PB_OK && rc != -PB_ERR_AGAIN)
        enumerated = false;

    return rc;
}
