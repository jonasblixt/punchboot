/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/usb/usbd.h>
#include <pb/delay.h>
#include <pb/dma.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/utils_def.h>
#include <stdbool.h>
#include <stdint.h>

/* Defines for commands in setup packets */
#define USB_GET_DESCRIPTOR    0x8006
#define USB_SET_CONFIGURATION 0x0009
#define USB_SET_FEATURE       0x0003
#define USB_SET_ADDRESS       0x0005
#define USB_GET_STATUS        0x8000
#define USB_GET_STATUS_IFACE  0x8100
#define USB_GET_STATUS_EP     0x8200
#define USB_GET_CONFIGURATION 0x8008
#define USB_GET_INTERFACE     0x810a
#define USB_GET_VENDOR        0xc001

static const struct usbd_hal_ops *hal_ops;
static const struct usbd_cls_config *cls_config;
static bool enumerated;
static usb_ep_t cur_xfer_ep;
/* A transfer was started and its result not given yet: one at a time
 * (usb.md 2.3, 2.4) */
static bool cur_xfer_running;

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

/*
 * Endpoint 0's data stage, a reply to the host (F-138, drivers.md 3). USB 2.0
 * 9.2.6.4 gives a device 500 ms for each packet of a data stage; a host takes
 * a reply at once, so one that hasn't in that time was reset or went away.
 * The request is given up, -PB_ERR_TIMEOUT: before the device is configured
 * the host starts over (usbd_connect()), in a session the transfer is lost
 * and command mode starts the transport again, whose init resets the
 * controller. An IN transfer only reads its buffer, which stays the stack's.
 * The loop waited for ever, with nothing kicking the watchdog.
 */
#define EP0_DATA_STAGE_TIMEOUT_us 500000

static int wait_for_completion(usb_ep_t ep)
{
    struct pb_timeout to;
    int rc;

    rc = pb_timeout_init_us(&to, EP0_DATA_STAGE_TIMEOUT_us);

    if (rc != PB_OK)
        return rc;

    do {
        rc = hal_ops->xfer_complete(ep);

        if (rc == -PB_ERR_AGAIN && pb_timeout_has_expired(&to)) {
            LOG_ERR("%s: the host didn't take the reply", ep_to_str(ep));
            return -PB_ERR_TIMEOUT;
        }
    } while (rc == -PB_ERR_AGAIN);

    return rc;
}

/*
 * A request the stack refuses is stalled, and the host then goes on at once
 * (usb.md 3.2, F-138). A stall that fails isn't an error: the device goes
 * on enumerating, or with the transfer that runs.
 */
static void ep0_refuse(void)
{
    int rc;

    rc = hal_ops->ep0_stall();

    if (rc != PB_OK)
        LOG_ERR("EP0 stall failed (%i)", rc);
}

static int ep0_unknown(const struct usb_setup_packet *setup)
{
    LOG_ERR("EP0 unhandled request %x %x, value 0x%x, index 0x%x, %ub",
            setup->bRequestType,
            setup->bRequest,
            setup->wValue,
            setup->wIndex,
            setup->wLength);
    ep0_refuse();
    return PB_OK;
}

static int ep0_tx(const void *bfr, size_t length)
{
    int rc;

    /* A request of wLength 0 has no data stage, and its status stage is IN
     * (USB 2.0 8.5.3, 9.3.5) */
    if (length == 0)
        return hal_ops->ep0_xfer_zlp(USB_EP0_IN);

    rc = hal_ops->xfer_start(USB_EP0_IN, (void *)bfr, length);

    if (rc != PB_OK)
        return rc;

    rc = wait_for_completion(USB_EP0_IN);

    if (rc != PB_OK)
        return rc;

    return hal_ops->ep0_xfer_zlp(USB_EP0_OUT);
}

/* An endpoint the device has in its state (USB 2.0 9.4.5): endpoint 0 always,
 * the configuration's once it is configured */
static bool known_endpoint(uint16_t address)
{
    const struct usbd_descriptors *desc = cls_config->desc;

    if ((address & ~(uint16_t)USB_EP_ADDR_IN) == 0)
        return true;

    if (!enumerated)
        return false;

    for (unsigned int n = 0; n < desc->interface.bNumEndpoints; n++) {
        if (desc->eps[n].bEndpointAddress == address)
            return true;
    }

    return false;
}

/* The interface there is, once the device is configured (USB 2.0 9.4.4, 9.4.5) */
static bool known_interface(uint16_t index)
{
    return enumerated && index == cls_config->desc->interface.bInterfaceNumber;
}

static int usbd_enumerate(struct usb_setup_packet *setup)
{
    int rc = -PB_ERR;
    uint16_t length = 0;
    uint16_t device_status = 0;
    uint8_t value = 0;
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

                /* An index the class doesn't have (NULL): refused (D-073) */
                if (str_desc == NULL) {
                    rc = ep0_unknown(setup);
                } else {
                    length = MIN(setup->wLength, (uint16_t)str_desc->bLength);
                    rc = ep0_tx(str_desc, length);
                }
            }
        } else {
            rc = ep0_unknown(setup);
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
            /* Refused: nothing is set up, and it is stalled (F-138) */
            LOG_ERR("No configuration %u", setup->wValue);
            ep0_refuse();
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
    /* USB 2.0 9.4: pb never halts an endpoint, has no remote wakeup and
     * reports no self power, so a status is two zero bytes, and its one
     * interface has alternate setting 0. A reply is at most wLength (usb.md
     * 3.2); an interface or an endpoint the device doesn't have in its state
     * is a request error, stalled (F-463) */
    case USB_GET_STATUS:
    case USB_GET_STATUS_IFACE:
    case USB_GET_STATUS_EP: {
        LOG_DBG("Get status");
        if ((request == USB_GET_STATUS_IFACE && !known_interface(setup->wIndex)) ||
            (request == USB_GET_STATUS_EP && !known_endpoint(setup->wIndex))) {
            rc = ep0_unknown(setup);
            break;
        }
        length = MIN(setup->wLength, (uint16_t)sizeof(device_status));
        rc = ep0_tx(&device_status, length);
    } break;
    case USB_GET_CONFIGURATION: {
        /* 0 before the device is configured */
        value = enumerated ? cls_config->desc->config.bConfigurationValue : 0;
        length = MIN(setup->wLength, (uint16_t)sizeof(value));
        rc = ep0_tx(&value, length);
    } break;
    case USB_GET_INTERFACE: {
        if (!known_interface(setup->wIndex)) {
            rc = ep0_unknown(setup);
            break;
        }
        length = MIN(setup->wLength, (uint16_t)sizeof(value));
        rc = ep0_tx(&value, length);
    } break;
    case USB_GET_VENDOR: {
        LOG_DBG("Get vendor specific descriptor");
        if (setup->wIndex == USB_MS_EXT_COMPAT_ID) {
            length = MIN(setup->wLength, (uint16_t)sizeof(msft_comp_id_feature));
            rc = ep0_tx(&msft_comp_id_feature, length);
        } else {
            rc = ep0_unknown(setup);
        }
    } break;
    default: {
        rc = ep0_unknown(setup);
    }
    }

    return rc;
}

int usbd_init_hal_ops(const struct usbd_hal_ops *ops)
{
    hal_ops = NULL;
    cur_xfer_running = false;

    /* Every operation is mandatory, and none is called that wasn't checked
     * here (usb.md 4): a driver that leaves one out isn't registered */
    if (ops == NULL || !ops->init || !ops->stop || !ops->xfer_start || !ops->xfer_complete ||
        !ops->xfer_cancel || !ops->poll_setup_pkt || !ops->set_configuration || !ops->set_address ||
        !ops->ep0_xfer_zlp || !ops->ep0_stall) {
        return -PB_ERR_PARAM;
    }

    hal_ops = ops;

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
    if (hal_ops == NULL)
        return -PB_ERR_IO;

    /* The controller starts with nothing configured (usb.md 4.1), and so
     * does the stack: ready is when the host has configured the device */
    enumerated = false;
    cur_xfer_running = false;

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

    /* A host that left a control transfer (it reset the bus, was unplugged
     * or gave up) starts over: before the device is configured there is no
     * session to lose, and the stack goes on enumerating (usb.md 2.2). Any
     * other error is the controller's */
    if (rc == -PB_ERR_TIMEOUT) {
        LOG_ERR("EP0 request %x %x given up", pkt.bRequestType, pkt.bRequest);
        return -PB_ERR_AGAIN;
    }

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
    /* A transfer that ran is given up: its buffer is the caller's (2.5) */
    cur_xfer_running = false;

    if (hal_ops->stop)
        return hal_ops->stop();
    else
        return PB_OK;
}

int usbd_xfer_start(usb_ep_t ep, void *buf, size_t length)
{
    int rc;

    if (hal_ops == NULL)
        return -PB_ERR_IO;

    /* One transfer at a time: the next is started when the last is done
     * (usb.md 2.3) */
    if (cur_xfer_running) {
        LOG_ERR("%s: a transfer on %s runs", ep_to_str(ep), ep_to_str(cur_xfer_ep));
        return -PB_ERR_STATE;
    }

    /* What the controller writes owns its cache lines (usb.md 2.3, 4.3,
     * drivers.md 4.3): an OUT transfer into a buffer that doesn't start on
     * one is refused before the controller sees it. Its last line is the
     * buffer's by its declaration. IN reads the buffer only, and takes any */
    if (!(ep & 1) && !dma_owns_lines(buf, 0)) {
        LOG_ERR("%s: buffer %p isn't on a cache line", ep_to_str(ep), buf);
        return -PB_ERR_ALIGN;
    }

    cur_xfer_ep = ep;

    rc = hal_ops->xfer_start(ep, buf, length);

    if (rc == PB_OK)
        cur_xfer_running = true;

    return rc;
}

/*
 * An OUT transfer of at most 'length' bytes that a short packet ends (usb.md
 * 4.5): what a protocol of messages of their own length needs, fastboot's
 * commands. *received is the byte count once usbd_xfer_complete() gave
 * PB_OK. A controller without the operation: -PB_ERR_NOT_SUPPORTED
 */
int usbd_xfer_start_short(usb_ep_t ep, void *buf, size_t length, size_t *received)
{
    int rc;

    if (hal_ops == NULL)
        return -PB_ERR_IO;

    if (hal_ops->xfer_start_short == NULL)
        return -PB_ERR_NOT_SUPPORTED;

    if ((ep & 1) || received == NULL)
        return -PB_ERR_PARAM;

    if (cur_xfer_running) {
        LOG_ERR("%s: a transfer on %s runs", ep_to_str(ep), ep_to_str(cur_xfer_ep));
        return -PB_ERR_STATE;
    }

    if (!dma_owns_lines(buf, 0)) {
        LOG_ERR("%s: buffer %p isn't on a cache line", ep_to_str(ep), buf);
        return -PB_ERR_ALIGN;
    }

    cur_xfer_ep = ep;
    rc = hal_ops->xfer_start_short(ep, buf, length, received);

    if (rc == PB_OK)
        cur_xfer_running = true;

    return rc;
}

int usbd_xfer_cancel(void)
{
    if (hal_ops == NULL)
        return -PB_ERR_IO;

    hal_ops->xfer_cancel(cur_xfer_ep);
    cur_xfer_running = false;
    return PB_OK;
}

/* The two requests of an enumeration that change what the device is to the
 * host: its address and its configuration */
static bool usbd_host_starts_over(const struct usb_setup_packet *setup)
{
    uint16_t request = (setup->bRequestType << 8) | setup->bRequest;

    return request == USB_SET_ADDRESS || request == USB_SET_CONFIGURATION;
}

static int usbd_xfer_poll(void)
{
    int rc;
    struct usb_setup_packet pkt;

    rc = hal_ops->poll_setup_pkt(&pkt);

    /* An error is a bus reset or an unplug of the device that was
     * configured, or a controller that failed: lost (usb.md 2.4, 4.4) */
    if (rc != PB_OK && rc != -PB_ERR_AGAIN)
        return rc;

    if (rc == PB_OK) {
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

    /* With no transfer started there is nothing to complete (usb.md 2.4) */
    if (!cur_xfer_running)
        return -PB_ERR_STATE;

    rc = usbd_xfer_poll();

    /* The result is given once: done or lost, the transfer is over */
    if (rc != -PB_ERR_AGAIN)
        cur_xfer_running = false;

    /* Every error is "lost" (usb.md 2.4): the bus was reset or the host is
     * gone. Nothing is configured after it (usb.md 3.2) */
    if (rc != PB_OK && rc != -PB_ERR_AGAIN)
        enumerated = false;

    return rc;
}
