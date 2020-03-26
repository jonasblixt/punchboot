/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include <stdio.h>
#include <string.h>
#include <pb/pb.h>
#include <pb/usb.h>
#include <pb/plat.h>
#include <pb/board.h>
#include <pb-tools/wire.h>

#define USB_DEBUG

static const uint8_t qf_descriptor[] =
{
    0x0A,   // USB_DEV_QUALIFIER_DESC_LEN,
    0x06,   // USB_DEV_QUALIFIER_DESC_TYPE,
    0x00,   // USB_DEV_DESC_SPEC_LB,
    0x02,   // USB_DEV_DESC_SPEC_HB,
    0x00,   // USB_DEV_DESC_DEV_CLASS,
    0x00,   // USB_DEV_DESC_DEV_SUBCLASS,
    0x00,   // USB_DEV_DESC_DEV_PROTOCOL,
    0x40,   // USB_DEV_DESC_EP0_MAXPACKETSIZE,
    0x01,   // USB_DEV_DESC_NUM_OT_SPEED_CONF,
    0       // USB_DEV_DESC_RESERVED
};

/**
 *
 *  Endpoint 0 <--> USB Configuration endpoint
 *  Endpoint 1 <--> BULK OUT, used for transfering large amounts of data
 *  Endpoint 2 <--> INTR OUT, used for commands from host -> device
 *  Endpoint 3 <--> INTR IN, used for responses from device -> host
 *
 */


static const uint8_t descriptor_300[] = "\x04\x03\x04\x09";

static const struct usb_descriptors descriptors =
{
    .device =
    {
        .bLength = 0x12,  //  length of this descriptor
        .bDescriptorType = 0x01,  //  Device descriptor
        .bcdUSB = 0x0200,  //  USB version 2.0
        /* Device class (specified in interface descriptor) */
        .bDeviceClass = 0x00,
        /* Device Subclass (specified in interface descriptor) */
        .bDeviceSubClass = 0x00,
        /* Device Protocol (specified in interface descriptor) */
        .bDeviceProtocol = 0x00,
        .bMaxPacketSize = 0x40,  // Max packet size for control endpoint
        .idVendor = PB_USB_VID,
        .idProduct = PB_USB_PID,
        .bcdDevice = 0x0000,  // Device revsion
        .iManufacturer = 0x01,  // Index of  Manufacturer string descriptor
        .iProduct = 0x01,  // Index of Product string descriptor
        .iSerialNumber = 0x01,  // Index of serial number string descriptor
        .bNumConfigurations = 0x01,  // Number of configurations
    },
    .config =
    {
        .bLength = 0x09,
        .bDescriptorType = 0x02,  // Configuration descriptor
        .wTotalLength = 0x27,  // Total length of data, includes interface
        .bNumInterfaces = 0x01,  // Number of interfaces
        .bConfigurationValue = 0x01,  // Number to select for this configuration
        .iConfiguration = 0x00,  // No string descriptor
        .bmAttributes = 0x80,  // Self powered, No remote wakeup
        .MaxPower = 0xfa
    },
    .interface =
    {
        .bLength = 0x09,
        .bDescriptorType = 0x04,  // Interface descriptor
        .bInterfaceNumber = 0x00,  // This interface = #0
        .bAlternateSetting = 0x00,  // Alternate setting
        .bNumEndpoints = 0x03,  // Number of endpoints for this interface
        .bInterfaceClass = 0xFF,
        .bInterfaceSubClass = 0xFF,
        .bInterfaceProtocol = 0xFF,
        .iInterface = 0,  // No string descriptor
    },
    .endpoint_bulk_out =
    {
        .bLength = 0x07,
        .bDescriptorType = 0x05,  // Endpoint descriptor
        .bEndpointAddress = 0x01,
        .bmAttributes = 0x2,
        .wMaxPacketSize = 0x0200,
        .bInterval = 0x00,
    },
    .endpoint_intr_out =
    {
        .bLength = 0x07,
        .bDescriptorType = 0x05,  // Endpoint descriptor
        .bEndpointAddress = 0x02,
        .bmAttributes = 0x3,
        .wMaxPacketSize = 0x0040,
        .bInterval = 0x01,
    },
    .endpoint_intr_in =
    {
        .bLength = 0x07,
        .bDescriptorType = 0x05,  // Endpoint descriptor
        .bEndpointAddress = 0x83,
        .bmAttributes = 0x3,
        .wMaxPacketSize = 0x0200,
        .bInterval = 0x01,
    }
};

static const uint8_t usb_string_id[] =
    {0x16, 3, 'P', 0, 'u', 0, 'n', 0, 'c', 0, 'h', 0, ' ', 0,
     'B', 0, 'O', 0, 'O', 0, 'T', 0};

static int usb_send_ep0(struct pb_usb_interface *iface,
                            void *bfr, size_t size)
{
    struct pb_transport_driver *drv = iface->transport;
    int rc;

    rc = iface->write(drv, USB_EP0_IN, bfr, size);

    if (rc != PB_OK)
        return rc;

    return iface->read(drv, USB_EP0_OUT, NULL, 0);
}

int usb_process_setup_pkt(struct pb_usb_interface *iface,
                          struct usb_setup_packet *setup)
{
    struct pb_transport_driver *drv = iface->transport;
    volatile uint16_t request;
    int rc;
    request = (setup->bRequestType << 8) | setup->bRequest;

    LOG_DBG("EP0 %x %x %ub", request, setup->wValue, setup->wLength);

    uint16_t sz = 0;
    uint16_t device_status = 0;

    switch (request)
    {
        case USB_GET_DESCRIPTOR:
        {
            LOG_DBG("Get descriptor 0x%x", setup->wValue);

            if (setup->wValue == 0x0600)
            {
                rc = usb_send_ep0(iface, (void *) qf_descriptor,
                                            sizeof(qf_descriptor));
            }
            else if (setup->wValue == 0x0100)
            {
                sz = sizeof(struct usb_device_descriptor);

                if (setup->wLength < sz)
                    sz = setup->wLength;

                rc = usb_send_ep0(iface, (void *) &descriptors.device, sz);
            }
            else if (setup->wValue == 0x0200)
            {
                uint16_t desc_tot_sz = descriptors.config.wTotalLength;

                sz = desc_tot_sz;

                if (setup->wLength < sz)
                    sz = setup->wLength;

                rc = usb_send_ep0(iface, (void *) &descriptors.config, sz);
            }
            else if (setup->wValue == 0x0300)
            {
                rc = usb_send_ep0(iface, (void *) descriptor_300, 4);
            }
            else if (setup->wValue == 0x0301)
            {
                sz = setup->wLength > sizeof(usb_string_id)?
                            sizeof(usb_string_id): setup->wLength;

                rc = usb_send_ep0(iface, (void *) usb_string_id, sz);
            }
            else if (setup->wValue == 0x0A00)
            {
                uint16_t desc_tot_sz = descriptors.interface.bLength;

                sz = desc_tot_sz;

                if (setup->wLength < sz)
                    sz = setup->wLength;

                rc = usb_send_ep0(iface, (void *) &descriptors.interface, sz);
            }
            else
            {
                LOG_ERR("Unhandled descriptor 0x%x", setup->wValue);
                rc = -PB_ERR;
            }
        }
        break;
        case USB_SET_ADDRESS:
        {
            LOG_DBG("Set address");
            rc = iface->set_address(drv, setup->wValue);

            if (rc != PB_OK)
                break;

            rc = iface->write(drv, USB_EP0_IN, NULL, 0);
        }
        break;
        case USB_SET_CONFIGURATION:
        {
            LOG_DBG("Set configuration");
            rc = iface->write(drv, USB_EP0_IN, NULL, 0);

            if (rc != PB_OK)
                break;

            rc = iface->set_configuration(drv);
        }
        break;
        case USB_SET_IDLE:
        {
            LOG_DBG("Set idle");
            rc = usb_send_ep0(iface, NULL, 0);
        }
        break;
        case USB_GET_STATUS:
        {
            LOG_DBG("Get status");
            rc = usb_send_ep0(iface, &device_status, 2);
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

    LOG_DBG("rc %i", rc);
    return rc;
}
