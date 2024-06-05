/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/usb/pb_dev_cls.h>
#include <drivers/usb/usbd.h>
#include <pb/device_uuid.h>
#include <pb/pb.h>
#include <stdint.h>
#include <string.h>
#include <uuid.h>

#ifndef PB_USB_VID
#define PB_USB_VID 0x1209
#endif

#ifndef PB_USB_PID
#define PB_USB_PID 0x2019
#endif

/**
 *
 *  Endpoint 0 <--> USB Configuration endpoint
 *  Endpoint 1 <--> BULK IN, Data from device to host
 *  Endpoint 2 <--> BULK OUT, Data from host to device
 *
 */

static const struct usbd_descriptors descriptors =
{
    .language = {
        .bLength = 0x04,              // Length of descriptor
        .bDescriptorType = 0x03,      // Language descriptor
        .wLang = 0x0409,            // en-us
    },
    .qualifier = {
        .bLength = 0x0A,              // USB_DEV_QUALIFIER_DESC_LEN,
        .bDescriptorType = 0x06,      // USB_DEV_QUALIFIER_DESC_TYPE,
        .bcdUSB = 0x0200,             // USB_DEV_DESC_SPEC
        .bDeviceClass = 0x00,         // USB_DEV_DESC_DEV_CLASS,
        .bDeviceSubClass = 0x00,      // USB_DEV_DESC_DEV_SUBCLASS,
        .bDeviceProtocol = 0x00,      // USB_DEV_DESC_DEV_PROTOCOL,
        .bMaxPacketSize0 = 0x40,      // USB_DEV_DESC_EP0_MAXPACKETSIZE,
        .bNumConfigurations = 0x01,   // USB_DEV_DESC_NUM_OT_SPEED_CONF,
        .bReserved = 0                // USB_DEV_DESC_RESERVED
    },
    .device = {
        .bLength = 0x12,              // length of this descriptor
        .bDescriptorType = 0x01,      // Device descriptor
        .bcdUSB = 0x0200,             // USB version 2.0
        .bDeviceClass = 0x00,         // Device class (specified in interface descriptor)
        .bDeviceSubClass = 0x00,      // Device Subclass (specified in interface descriptor)
        .bDeviceProtocol = 0x00,      // Device Protocol (specified in interface descriptor)
        .bMaxPacketSize = 0x40,       // Max packet size for control endpoint
        .idVendor = PB_USB_VID,
        .idProduct = PB_USB_PID,
        .bcdDevice = 0x0000,          // Device revsion
        .iManufacturer = 0x01,        // Index of  Manufacturer string descriptor
        .iProduct = 0x01,             // Index of Product string descriptor
        .iSerialNumber = 0x02,        // Index of serial number string descriptor
        .bNumConfigurations = 0x01,   // Number of configurations
    },
    .config = {
        .bLength = 0x09,
        .bDescriptorType = 0x02,      // Configuration descriptor
        .wTotalLength = 0x20,         // len = sizeof(config) + sizeof(interface) + no_eps * sizeof(endpoint)
        .bNumInterfaces = 0x01,       // Number of interfaces
        .bConfigurationValue = 0x01,  // Number to select for this configuration
        .iConfiguration = 0x00,       // No string descriptor
        .bmAttributes = 0x80,         // Self powered, No remote wakeup
        .MaxPower = 0xfa
    },
    .interface = {
        .bLength = 0x09,
        .bDescriptorType = 0x04,      // Interface descriptor
        .bInterfaceNumber = 0x00,     // This interface = #0
        .bAlternateSetting = 0x00,    // Alternate setting
        .bNumEndpoints = 0x02,        // Number of endpoints for this interface
        .bInterfaceClass = 0xFF,
        .bInterfaceSubClass = 0xFF,
        .bInterfaceProtocol = 0xFF,
        .iInterface = 0,              // No string descriptor
    },
    .eps = {
        {
            .bLength = 0x07,
            .bDescriptorType = 0x05,  // Endpoint descriptor
            .bEndpointAddress = 0x81, // EP1_IN (PB -> HOST)
            .bmAttributes = 0x2,
            .wMaxPacketSize = 0x0200,
            .bInterval = 0x00,
        },
        {
            .bLength = 0x07,
            .bDescriptorType = 0x05,  // Endpoint descriptor
            .bEndpointAddress = 0x02, // EP2_OUT (HOST -> PB)
            .bmAttributes = 0x2,
            .wMaxPacketSize = 0x0200,
            .bInterval = 0x00,
        },
    }
};

static void usb_string(struct usb_string_descriptor *desc, const char *str)
{
    memset(desc, 0, sizeof(*desc));
    desc->bLength = strlen(str) * 2 + 2;
    desc->bDescriptorType = 0x03;

    for (unsigned int i = 0; i < strlen(str); i++)
        desc->unicode[i] = str[i];
}

static struct usb_string_descriptor *get_string_descriptor(uint8_t idx)
{
    static struct usb_string_descriptor usb_str_desc;
    uuid_t device_uu;
    char device_uu_str[37];

    if (idx == 1) {
        usb_string(&usb_str_desc, "Punchboot");
    } else if (idx == 2) {
        device_uuid(device_uu);
        uuid_unparse(device_uu, device_uu_str);
        usb_string(&usb_str_desc, device_uu_str);
    } else {
        usb_string(&usb_str_desc, "");
    }

    return &usb_str_desc;
}

int pb_dev_cls_init(void)
{
    static const struct usbd_cls_config cls_config = {
        .desc = &descriptors,
        .get_string_descriptor = get_string_descriptor,
    };

    return usbd_init_cls(&cls_config);
}

int pb_dev_cls_write(const void *buf, size_t length)
{
    return usbd_xfer_start(USB_EP1_IN, (void *)buf, length);
}

int pb_dev_cls_read(void *buf, size_t length)
{
    return usbd_xfer_start(USB_EP2_OUT, buf, length);
}

int pb_dev_cls_xfer_complete(void)
{
    return usbd_xfer_complete();
}
