/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb.h>
#include <usb.h>
#include <plat.h>
#include <board.h>
#include <tinyprintf.h>
#include <string.h>
#include <inttypes.h>

#define USB_DEBUG
static struct usb_device *usb_dev;

static const uint8_t qf_descriptor[] = {
	0x0A,	//USB_DEV_QUALIFIER_DESC_LEN,
	0x06,   //USB_DEV_QUALIFIER_DESC_TYPE,
	0x00,   //USB_DEV_DESC_SPEC_LB,
	0x02,   //USB_DEV_DESC_SPEC_HB,
	0x00,   //USB_DEV_DESC_DEV_CLASS,
	0x00,   //USB_DEV_DESC_DEV_SUBCLASS,
	0x00,   //USB_DEV_DESC_DEV_PROTOCOL,
	0x40,   //USB_DEV_DESC_EP0_MAXPACKETSIZE,
	0x01,   //USB_DEV_DESC_NUM_OT_SPEED_CONF,
	0		//USB_DEV_DESC_RESERVED
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
        .bLength = 0x12, // length of this descriptor
        .bDescriptorType = 0x01, // Device descriptor
        .bcdUSB = 0x0200, // USB version 2.0
        .bDeviceClass = 0x00, // Device class (specified in interface descriptor)
        .bDeviceSubClass = 0x00, // Device Subclass (specified in interface descriptor)
        .bDeviceProtocol = 0x00, // Device Protocol (specified in interface descriptor)
        .bMaxPacketSize = 0x40, // Max packet size for control endpoint
        .idVendor = 0xFfff, // Freescale Vendor ID. -- DO NOT USE IN A PRODUCT
        .idProduct = 0x0001, // Decvice ID -- DO NOT USE IN A PRODUCT
        .bcdDevice = 0x0000, // Device revsion
        .iManufacturer = 0x01, // Index of  Manufacturer string descriptor
        .iProduct = 0x01, // Index of Product string descriptor
        .iSerialNumber = 0x01, // Index of serial number string descriptor
        .bNumConfigurations = 0x01, // Number of configurations
    },
    .config = {
        .bLength = 0x09, //
        .bDescriptorType = 0x02, // Configuration descriptor
        .wTotalLength = 0x27, // Total length of data, includes interface, HID and endpoint
        .bNumInterfaces = 0x01, // Number of interfaces
        .bConfigurationValue = 0x01, // Number to select for this configuration
        .iConfiguration = 0x00, // No string descriptor
        .bmAttributes = 0x80, // Self powered, No remote wakeup
        .MaxPower = 0xfa 
    },
    .interface = {
        .bLength = 0x09,
        .bDescriptorType = 0x04, // Interface descriptor
        .bInterfaceNumber = 0x00, // This interface = #0
        .bAlternateSetting = 0x00, // Alternate setting
        .bNumEndpoints = 0x03, // Number of endpoints for this interface
        .bInterfaceClass = 0xFF, // HID class interface
        .bInterfaceSubClass = 0xFF, // Boot interface Subclass
        .bInterfaceProtocol = 0xFF, // Mouse protocol
        .iInterface = 0, // No string descriptor
    },
    .endpoint_bulk_out = {
        .bLength = 0x07,
        .bDescriptorType = 0x05, // Endpoint descriptor
        .bEndpointAddress = 0x01, 
        .bmAttributes = 0x2, 
        .wMaxPacketSize = 0x0200, 
        .bInterval = 0x00, // 10 ms interval
    },
    .endpoint_intr_out = {
        .bLength = 0x07,
        .bDescriptorType = 0x05, // Endpoint descriptor
        .bEndpointAddress = 0x02, 
        .bmAttributes = 0x3, 
        .wMaxPacketSize = 0x0040, 
        .bInterval = 0x05, 
    },
    .endpoint_intr_in = {
        .bLength = 0x07,
        .bDescriptorType = 0x05, // Endpoint descriptor
        .bEndpointAddress = 0x83, 
        .bmAttributes = 0x3, 
        .wMaxPacketSize = 0x0200, 
        .bInterval = 0x05,
    }
};

static const uint8_t usb_string_id[] = 
    {0x16,3,'P',0,'u',0,'n',0,'c',0,'h',0,' ',0,'B',0,'O',0,'O',0,'T',0};


static uint8_t __a4k __no_bss usb_data_buffer[4096];

static void usb_send_ep0(struct usb_device *dev, uint8_t *bfr, uint32_t sz)
{
    memcpy(usb_data_buffer, bfr, sz);
    plat_usb_transfer(dev, USB_EP0_IN, usb_data_buffer, sz);
    plat_usb_wait_for_ep_completion(dev, USB_EP0_IN);

    plat_usb_transfer(dev, USB_EP0_OUT, NULL, 0);
    plat_usb_wait_for_ep_completion(dev, USB_EP0_OUT);
}

static uint32_t usb_process_setup_pkt(struct usb_device *dev,
                                      struct usb_setup_packet *setup)
{
    volatile uint16_t request;
    request = (setup->bRequestType << 8) | setup->bRequest;

    LOG_DBG ("EP0 %4.4X %4.4X %ib", request, setup->wValue, setup->wLength);

    uint16_t sz = 0;
    uint16_t device_status = 0;

    switch (request) 
    {
        case USB_GET_DESCRIPTOR:
        {
            LOG_DBG("Get descriptor 0x%4.4X", setup->wValue);

            if(setup->wValue == 0x0600) 
            {
                usb_send_ep0(dev, (uint8_t *) qf_descriptor, 
                                                    sizeof(qf_descriptor));
            } else if (setup->wValue == 0x0100) {
                
                sz = sizeof(struct usb_device_descriptor);

                if (setup->wLength < sz)
                    sz = setup->wLength;

                usb_send_ep0(dev, (uint8_t *) &descriptors.device, sz);

            } else if (setup->wValue == 0x0200) {
                uint16_t desc_tot_sz = descriptors.config.wTotalLength;

                sz = desc_tot_sz;

                if (setup->wLength < sz)
                    sz = setup->wLength;

                usb_send_ep0(dev, (uint8_t *) &descriptors.config, sz);
            } else if (setup->wValue == 0x0300) { 
                usb_send_ep0(dev, (uint8_t *) descriptor_300, 4);
            } else if(setup->wValue == 0x0301) {
                
                sz = setup->wLength > sizeof(usb_string_id)?
                            sizeof(usb_string_id): setup->wLength;
                
                usb_send_ep0(dev, (uint8_t *) usb_string_id, sz);
     
            } else if (setup->wValue == 0x0A00) {
                uint16_t desc_tot_sz = descriptors.interface.bLength;

                sz = desc_tot_sz;

                if (setup->wLength < sz)
                    sz = setup->wLength;

                usb_send_ep0(dev, (uint8_t *) &descriptors.interface, sz);
                
            } else {
                LOG_ERR ("Unhandeled descriptor 0x%4.4X", setup->wValue);
            }
        }
        break;
        case USB_SET_ADDRESS:
        {
            plat_usb_set_address(dev, setup->wValue );
            plat_usb_transfer(dev, USB_EP0_IN, NULL, 0);
            plat_usb_wait_for_ep_completion(dev, USB_EP0_IN);
        }
        break;
        case USB_SET_CONFIGURATION:
        {
            plat_usb_transfer(dev, USB_EP0_IN, NULL, 0);
            plat_usb_wait_for_ep_completion(dev, USB_EP0_IN);
            plat_usb_set_configuration(dev);
        }
        break;
        case USB_SET_IDLE:
        {
            usb_send_ep0(dev,NULL,0);
        }
        break;
        case USB_GET_STATUS:
        {
            usb_send_ep0(dev,(uint8_t *) &device_status, 2);
            usb_send_ep0(dev,NULL,0);
        }
        break;
        default:
        {
            LOG_ERR ("EP0 Unhandled request %4.4x",request);
            LOG_ERR (" bRequestType = 0x%2.2x", setup->bRequestType);
            LOG_ERR (" bRequest = 0x%2.2x", setup->bRequest);
            LOG_ERR (" wValue = 0x%4.4x", setup->wValue);
            LOG_ERR (" wIndex = 0x%4.4x",setup->wIndex);
            LOG_ERR (" wLength = 0x%4.4x",setup->wLength);
        }
    }

    return PB_OK;
}

void usb_on_command(struct usb_device *dev, struct pb_cmd_header *cmd)
{
    UNUSED(dev);
    UNUSED(cmd);

    LOG_ERR ("Unhandeled command 0x%8.8"PRIx32,cmd->cmd);
}

uint32_t usb_init(void)
{
    uint32_t err = PB_OK;
    
    LOG_INFO("Init");

    err = board_usb_init(&usb_dev);

    if (err != PB_OK)
        return err;

    usb_dev->on_setup_pkt = usb_process_setup_pkt;
    usb_dev->on_command = usb_on_command;

    return plat_usb_init(usb_dev);
}

void usb_set_on_command_handler(usb_on_command_t handler)
{
    usb_dev->on_command = handler;
}

void usb_task(void)
{
    plat_usb_task(usb_dev);
}

