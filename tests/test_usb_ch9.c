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
#include <plat/test/semihosting.h>
#include <usb.h>
#include <pb/assert.h>

#include "test.h"

static uint16_t usb_addr = 0;
static struct usb_setup_packet pkt;
static struct usb_device *usbdev;


static const uint8_t qf_descriptor[] = {
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

static const struct usb_descriptors descriptors = {
    .device = {
        .bLength = 0x12,  // length of this descriptor
        .bDescriptorType = 0x01,  // Device descriptor
        .bcdUSB = 0x0200,  // USB version 2.0
        .bDeviceClass = 0x00,
        .bDeviceSubClass = 0x00,
        .bDeviceProtocol = 0x00,
        .bMaxPacketSize = 0x40,  // Max packet size for control endpoint
        .idVendor = 0x1209,  // Freescale Vendor ID. -- DO NOT USE IN A PRODUCT
        .idProduct = 0x2019,  // Decvice ID -- DO NOT USE IN A PRODUCT
        .bcdDevice = 0x0000,  // Device revsion
        .iManufacturer = 0x01,  // Index of  Manufacturer string descriptor
        .iProduct = 0x01,  // Index of Product string descriptor
        .iSerialNumber = 0x01,  // Index of serial number string descriptor
        .bNumConfigurations = 0x01,  // Number of configurations
    },
    .config = {
        .bLength = 0x09,
        .bDescriptorType = 0x02,  // Configuration descriptor
        .wTotalLength = 0x27,
        .bNumInterfaces = 0x01,  // Number of interfaces
        .bConfigurationValue = 0x01,  // Number to select for this configuration
        .iConfiguration = 0x00,  // No string descriptor
        .bmAttributes = 0x80,  // Self powered, No remote wakeup
        .MaxPower = 0xfa
    },
    .interface = {
        .bLength = 0x09,
        .bDescriptorType = 0x04,  // Interface descriptor
        .bInterfaceNumber = 0x00,  // This interface = #0
        .bAlternateSetting = 0x00,  // Alternate setting
        .bNumEndpoints = 0x03,  // Number of endpoints for this interface
        .bInterfaceClass = 0xFF,  // HID class interface
        .bInterfaceSubClass = 0xFF,  // Boot interface Subclass
        .bInterfaceProtocol = 0xFF,  // Mouse protocol
        .iInterface = 0,  // No string descriptor
    },
    .endpoint_bulk_out = {
        .bLength = 0x07,
        .bDescriptorType = 0x05,  // Endpoint descriptor
        .bEndpointAddress = 0x01,
        .bmAttributes = 0x2,
        .wMaxPacketSize = 0x0200,
        .bInterval = 0x00,  // 10 ms interval
    },
    .endpoint_intr_out = {
        .bLength = 0x07,
        .bDescriptorType = 0x05,  // Endpoint descriptor
        .bEndpointAddress = 0x02,
        .bmAttributes = 0x3,
        .wMaxPacketSize = 0x0040,
        .bInterval = 0x05,
    },
    .endpoint_intr_in = {
        .bLength = 0x07,
        .bDescriptorType = 0x05,  // Endpoint descriptor
        .bEndpointAddress = 0x83,
        .bmAttributes = 0x3,
        .wMaxPacketSize = 0x0200,
        .bInterval = 0x05,
    }
};

static const uint8_t usb_string_id[] =
    {0x16, 3, 'P', 0, 'u', 0, 'n', 0, 'c', 0, 'h', 0, ' ',
     0, 'B', 0, 'O', 0, 'O', 0, 'T', 0};

static bool flag_zlp, flag_wait_for_zlp;
static bool flag_set_config;

uint32_t  plat_usb_transfer(struct usb_device *dev, uint8_t ep,
                            uint8_t *bfr, uint32_t sz)
{
    UNUSED(dev);

    if ((sz == 0) &&
        (ep == USB_EP0_OUT))
    {
        flag_zlp = true;
    }

    if ((sz == 0) &&
        (ep == USB_EP0_IN))
    {
        flag_wait_for_zlp = true;
    }

    /* Get descriptor */
    if ((pkt.bRequestType == 0x80) &&
        (pkt.bRequest == 0x06) &&
        (ep == USB_EP0_IN))
    {
        switch (pkt.wValue)
        {
            case 0x600:
            {
                assert(memcmp(bfr, qf_descriptor, sz) == 0);
            }
            break;
            case 0x100:
            {
                assert(memcmp(bfr, &descriptors.device, sz) == 0);
            }
            break;
            case 0x200:
            {
                assert(memcmp(bfr, &descriptors.config, sz) == 0);
            }
            break;
            case 0xA00:
            {
                assert(memcmp(bfr, &descriptors.interface, sz) == 0);
            }
            break;
            case 0x300:
            {
                assert(memcmp(bfr, descriptor_300, sz) == 0);
            }
            break;
            case 0x301:
            {
                assert(memcmp(bfr, usb_string_id, sz) == 0);
            }
            break;
            default:
            {
                LOG_ERR("Unknown descriptor");
                assert(0);
            }
        }
    }

    return PB_OK;
}

void      plat_usb_set_address(struct usb_device *dev, uint32_t addr)
{
    UNUSED(dev);
    usb_addr = addr;
}

void plat_usb_set_configuration(struct usb_device *dev)
{
    UNUSED(dev);
    flag_set_config = true;
}

void plat_usb_wait_for_ep_completion(uint32_t ep)
{
    UNUSED(ep);
}


uint32_t  plat_usb_init(struct usb_device *dev)
{
    usbdev = dev;
    return PB_OK;
}

void plat_usb_task(struct usb_device *dev)
{
    UNUSED(dev);
}

uint32_t plat_prepare_recovery(struct pb_platform_setup *plat)
{
    UNUSED(plat);
    return PB_OK;
}

void test_main(void)
{
    LOG_INFO("USB CH9 test begin");
    usb_init();

    /* Check standard descriptors */
    pkt.bRequestType = 0x80;
    pkt.bRequest = 0x06;
    pkt.wValue = 0x0600;

    flag_zlp = false;
    usbdev->on_setup_pkt(usbdev, &pkt);
    assert(flag_zlp);

    pkt.bRequestType = 0x80;
    pkt.bRequest = 0x06;
    pkt.wValue = 0x0100;
    pkt.wLength = 8;

    flag_zlp = false;
    usbdev->on_setup_pkt(usbdev, &pkt);
    assert(flag_zlp);

    pkt.bRequestType = 0x80;
    pkt.bRequest = 0x06;
    pkt.wValue = 0x0100;
    pkt.wLength = sizeof(descriptors.device);

    flag_zlp = false;
    usbdev->on_setup_pkt(usbdev, &pkt);
    assert(flag_zlp);

    pkt.bRequestType = 0x80;
    pkt.bRequest = 0x06;
    pkt.wValue = 0x0200;
    pkt.wLength = 8;

    flag_zlp = false;
    usbdev->on_setup_pkt(usbdev, &pkt);
    assert(flag_zlp);

    pkt.bRequestType = 0x80;
    pkt.bRequest = 0x06;
    pkt.wValue = 0x0200;
    pkt.wLength = sizeof(descriptors.config);

    flag_zlp = false;
    usbdev->on_setup_pkt(usbdev, &pkt);
    assert(flag_zlp);

    pkt.bRequestType = 0x80;
    pkt.bRequest = 0x06;
    pkt.wValue = 0x0300;
    pkt.wLength = sizeof(descriptor_300);

    flag_zlp = false;
    usbdev->on_setup_pkt(usbdev, &pkt);
    assert(flag_zlp);


    pkt.bRequestType = 0x80;
    pkt.bRequest = 0x06;
    pkt.wValue = 0x0301;
    pkt.wLength = sizeof(usb_string_id);

    flag_zlp = false;
    usbdev->on_setup_pkt(usbdev, &pkt);
    assert(flag_zlp);


    pkt.bRequestType = 0x80;
    pkt.bRequest = 0x06;
    pkt.wValue = 0x0A00;
    pkt.wLength = 8;

    flag_zlp = false;
    usbdev->on_setup_pkt(usbdev, &pkt);
    assert(flag_zlp);

    pkt.bRequestType = 0x80;
    pkt.bRequest = 0x06;
    pkt.wValue = 0x0A00;
    pkt.wLength = sizeof(descriptors.interface);
    flag_zlp = false;
    usbdev->on_setup_pkt(usbdev, &pkt);
    assert(flag_zlp);

    /* Test invalid descriptor */
    pkt.bRequestType = 0x80;
    pkt.bRequest = 0x06;
    pkt.wValue = 0xABCD;
    pkt.wLength = 1234;
    flag_zlp = false;
    usbdev->on_setup_pkt(usbdev, &pkt);
    assert(!flag_zlp);

    pkt.bRequestType = 0x00;
    pkt.bRequest = 0x05;
    pkt.wValue = 1234;
    pkt.wLength = 0;
    flag_zlp = false;
    flag_wait_for_zlp = false;
    usbdev->on_setup_pkt(usbdev, &pkt);
    assert(!flag_zlp);
    assert(flag_wait_for_zlp);
    assert(usb_addr == 1234);


    pkt.bRequestType = 0x00;
    pkt.bRequest = 0x09;
    pkt.wValue = 0;
    pkt.wLength = 0;
    flag_zlp = false;
    flag_wait_for_zlp = false;
    flag_set_config = false;
    usbdev->on_setup_pkt(usbdev, &pkt);
    assert(!flag_zlp);
    assert(flag_wait_for_zlp);
    assert(flag_set_config);


    pkt.bRequestType = 0x21;
    pkt.bRequest = 0x0A;
    pkt.wValue = 0;
    pkt.wLength = 0;
    flag_zlp = false;
    flag_wait_for_zlp = false;
    usbdev->on_setup_pkt(usbdev, &pkt);
    assert(flag_zlp);

    pkt.bRequestType = 0x80;
    pkt.bRequest = 0x00;
    pkt.wValue = 0;
    pkt.wLength = 0;
    flag_zlp = false;
    flag_wait_for_zlp = false;
    usbdev->on_setup_pkt(usbdev, &pkt);
    assert(flag_zlp);
    /* Test invalid request */

    pkt.bRequestType = 0x12;
    pkt.bRequest = 0x32;
    pkt.wValue = 0;
    pkt.wLength = 0;
    flag_zlp = false;
    flag_wait_for_zlp = false;
    usbdev->on_setup_pkt(usbdev, &pkt);
    assert(!flag_zlp);
    assert(!flag_wait_for_zlp);

    LOG_INFO("USB CH9 test end");
}
