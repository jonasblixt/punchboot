/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __EHCI_H__
#define __EHCI_H__

#include <pb.h>

#define USB_DCIVERSION 0x120
#define USB_USBSTS 0x144
#define USB_ENDPTCOMPLETE 0x1bc
#define USB_USBMODE 0x1a8
#define USB_ENDPTLISTADDR 0x158
#define USB_CMD 0x140
#define USB_ENDPTSETUPSTAT 0x1ac
#define USB_ENDPTPRIME 0x1b0
#define USB_PORTSC1 0x184
#define USB_ENDPTFLUSH 0x1b4
#define USB_ENDPTSETUPSTAT 0x1ac
#define USB_DEVICEADDR 0x154
#define USB_ENDPTCTRL1 0x1c4
#define USB_ENDPTCTRL2 0x1c8
#define USB_ENDPTCTRL3 0x1cc
#define USB_BURSTSIZE 0x160
#define USB_SBUSCFG 0x90
#define USB_ENDPTSTAT 0x1b8
#define USBPHY_BASE 0x020C9000
#define USB_ENDPTNAKEN 0x17C


struct ehci_dTH {
    uint32_t next_dtd;
    uint32_t dtd_token;
    uint32_t bfr_page0;
    uint32_t bfr_page1;
    uint32_t bfr_page2;
    uint32_t bfr_page3;
    uint32_t bfr_page4;
} __attribute__((aligned(32)));

struct ehci_dQH {
    uint32_t caps;
    uint32_t current_dtd;
    uint32_t next_dtd;
    uint32_t dtd_token;
    uint32_t bfr_page0;
    uint32_t bfr_page1;
    uint32_t bfr_page2;
    uint32_t bfr_page3;
    uint32_t bfr_page4;
    uint32_t __reserved;
    uint32_t setup[2];
    uint32_t padding[4];
} __attribute__((aligned(32)));



/* Defines for commands in setup packets */
#define GET_DESCRIPTOR				0x8006
#define SET_CONFIGURATION			0x0009
#define SET_IDLE					0x210A
#define SET_FEATURE					0x0003
#define SET_ADDRESS                 0x0005
#define GET_STATUS                  0x8000

struct usbdSetupPacket {
    uint8_t bRequestType;	   // Characteristics of request 
    uint8_t bRequest;           // Specific request
    uint16_t wValue;            // Word-sized field that varies according to request
    uint16_t wIndex;            // Index
    uint16_t wLength;           // Number of bytes to transfer
}  __attribute__ ((packed));

struct usb_device_descriptor {
    uint8_t bLength;		       // Size of this descriptor in bytes
    uint8_t bDescriptorType;	   // DEVICE Descriptor Type
    uint16_t bcdUSB;		       // USB Specification Release Number
    uint8_t bDeviceClass;	   // Class code
    uint8_t bDeviceSubClass;	   // Subclass code
    uint8_t bDeviceProtocol;	   // Protocol code
    uint8_t bMaxPacketSize;	   // Maximum packet size for endpoint zero
    uint16_t idVendor;          // Vendor ID
    uint16_t idProduct;         // Product ID
    uint16_t bcdDevice;         // Device release number
    uint8_t iManufacturer;      // Index of string descriptor describing manufacturer
    uint8_t iProduct;           // Index of string descriptor describing product
    uint8_t iSerialNumber; 	   // Index of string descriptor describing the device's serial number
    uint8_t bNumConfigurations; // Number of possible configurations
}  __attribute__ ((packed));

struct usb_configuration_descriptor {
    uint8_t bLength;			    // Size of descriptor	
    uint8_t bDescriptorType;		// CONFIGURATION Descriptor Type
    uint16_t wTotalLength;       // Total length of data returned for this configuration
    uint8_t bNumInterfaces;      // Number of interfaces supported by this configuration
    uint8_t bConfigurationValue; // Value to use as an argument to the to select this configuration
    uint8_t iConfiguration;      // Index of string descriptor describing this configuration
    uint8_t bmAttributes;		// Configuration characteristics
    uint8_t MaxPower;			// Maximum power consumption of the USB device
}  __attribute__ ((packed));

struct usb_interface_descriptor {
    uint8_t bLength;		       // Size of this descriptor in bytes 
    uint8_t bDescriptorType;    // INTERFACE Descriptor Type
    uint8_t bInterfaceNumber;   // Number of this interface
    uint8_t bAlternateSetting;  // Value used to select this alternate setting
    uint8_t bNumEndpoints;      // Number of endpoints used by this interface
    uint8_t bInterfaceClass;    // Class code
    uint8_t bInterfaceSubClass; // Subclass code
    uint8_t bInterfaceProtocol; // Protocol code
    uint8_t iInterface;		   // Index of string descriptor describing this interface
}  __attribute__ ((packed));


struct usb_endpoint_descriptor {
    uint8_t bLength;		       // Size of this descriptor in bytes 
    uint8_t bDescriptorType;    // ENDPOINT Descriptor Type
    uint8_t bEndpointAddress;   // The address of the endpoint on the USB device described by this descriptor
    uint8_t bmAttributes;       // The endpoint'ss attributes
    uint16_t wMaxPacketSize;    //  Maximum packet size
    uint8_t bInterval;          // Interval for polling endpoint for data transfers
}  __attribute__ ((packed));


struct usb_descriptors {
    const struct usb_device_descriptor device;
    const struct usb_configuration_descriptor config;
    const struct usb_interface_descriptor interface;
    const struct usb_endpoint_descriptor endpoint_bulk_out;
    const struct usb_endpoint_descriptor endpoint_intr_out;
    const struct usb_endpoint_descriptor endpoint_intr_in;
} __attribute__ ((packed));


struct ehci_device {
    __iomem base;
    uint8_t enumerated ;
    uint8_t ready;
    struct ehci_dQH __attribute__((aligned(4096))) dqh[16];
    struct usbdSetupPacket setup;
};


uint32_t ehci_usb_init(__iomem base);

#endif
