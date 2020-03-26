/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_USB_H_
#define INCLUDE_PB_USB_H_

#include <stdint.h>
#include <stdbool.h>
#include <pb/transport.h>

/* Defines for commands in setup packets */
#define USB_GET_DESCRIPTOR     0x8006
#define USB_SET_CONFIGURATION  0x0009
#define USB_SET_IDLE           0x210A
#define USB_SET_FEATURE        0x0003
#define USB_SET_ADDRESS        0x0005
#define USB_GET_STATUS         0x8000

#ifndef PB_USB_VID
    #define PB_USB_VID 0x1209
#endif

#ifndef PB_USB_PID
    #define PB_USB_PID 0x2019
#endif

enum pb_usb_eps
{
    USB_EP0_OUT,
    USB_EP0_IN,
    USB_EP1_OUT,
    USB_EP1_IN,
    USB_EP2_OUT,
    USB_EP2_IN,
    USB_EP3_OUT,
    USB_EP3_IN,
    USB_EP4_OUT,
    USB_EP4_IN,
    USB_EP5_OUT,
    USB_EP5_IN,
    USB_EP6_OUT,
    USB_EP6_IN,
    USB_EP7_OUT,
    USB_EP7_IN,
};

struct usb_setup_packet
{
    uint8_t bRequestType;       /*!< Characteristics of request */
    uint8_t bRequest;           /*!< Specific request */
    uint16_t wValue;            /*!< Word-sized field that varies according to
                                     request*/
    uint16_t wIndex;            /*!< Index */
    uint16_t wLength;           /*!< Number of bytes to transfer */
}  __attribute__((packed));

struct usb_device_descriptor
{
    uint8_t bLength;            /*!< Size of this descriptor in bytes */
    uint8_t bDescriptorType;    /*!< DEVICE Descriptor Type */
    uint16_t bcdUSB;            /*!< USB Specification Release Number */
    uint8_t bDeviceClass;       /*!< Class code */
    uint8_t bDeviceSubClass;    /*!< Subclass code */
    uint8_t bDeviceProtocol;    /*!< Protocol code */
    uint8_t bMaxPacketSize;     /*!< Maximum packet size for endpoint zero */
    uint16_t idVendor;          /*!< Vendor ID */
    uint16_t idProduct;         /*!< Product ID */
    uint16_t bcdDevice;         /*!< Device release number */
    uint8_t iManufacturer;      /*!< Index of string descriptor describing
                                         manufacturer */
    uint8_t iProduct;           /*!< Index of string descriptor describing product */
    uint8_t iSerialNumber;      /*!< Index of string descriptor describing the
                                      device's serial number */
    uint8_t bNumConfigurations; /*!< Number of possible configurations */
}  __attribute__((packed));

struct usb_configuration_descriptor
{
    uint8_t bLength;                // Size of descriptor
    uint8_t bDescriptorType;        // CONFIGURATION Descriptor Type
    uint16_t wTotalLength;       // Total length of data returned for this
                                 // configuration

    uint8_t bNumInterfaces;      // Number of interfaces supported by
                                 // this configuration
    uint8_t bConfigurationValue;  // Value to use as an argument to the to
                                  // select this configuration
    uint8_t iConfiguration;      // Index of string descriptor describing
                                 // this configuration
    uint8_t bmAttributes;        // Configuration characteristics
    uint8_t MaxPower;            // Maximum power consumption of the USB device
}  __attribute__((packed));

struct usb_interface_descriptor
{
    uint8_t bLength;               // Size of this descriptor in bytes
    uint8_t bDescriptorType;    // INTERFACE Descriptor Type
    uint8_t bInterfaceNumber;   // Number of this interface
    uint8_t bAlternateSetting;  // Value used to select this alternate setting
    uint8_t bNumEndpoints;      // Number of endpoints used by this interface
    uint8_t bInterfaceClass;    // Class code
    uint8_t bInterfaceSubClass;  // Subclass code
    uint8_t bInterfaceProtocol;  // Protocol code
    uint8_t iInterface;  // Index of string descriptor describing this interface
}  __attribute__((packed));


struct usb_endpoint_descriptor
{
    uint8_t bLength;               // Size of this descriptor in bytes
    uint8_t bDescriptorType;    // ENDPOINT Descriptor Type
    uint8_t bEndpointAddress;   // The address of the endpoint on the USB
                                // device described by this descriptor
    uint8_t bmAttributes;       // The endpoint'ss attributes
    uint16_t wMaxPacketSize;    //  Maximum packet size
    uint8_t bInterval;      // Interval for polling endpoint for data transfers
}  __attribute__((packed));


struct usb_descriptors
{
    const struct usb_device_descriptor device;
    const struct usb_configuration_descriptor config;
    const struct usb_interface_descriptor interface;
    const struct usb_endpoint_descriptor endpoint_bulk_out;
    const struct usb_endpoint_descriptor endpoint_intr_out;
    const struct usb_endpoint_descriptor endpoint_intr_in;
} __attribute__((packed));

typedef int (*pb_usb_io_t) (struct pb_transport_driver *drv,
                         int ep, void *buf, size_t size);

typedef int (*pb_usb_call_t) (struct pb_transport_driver *drv);

typedef bool (*pb_usb_status_t) (struct pb_transport_driver *drv);

typedef int (*pb_usb_set_addr_t) (struct pb_transport_driver *drv,
                                    uint16_t addr);

struct pb_usb_interface
{
    uint8_t device_uuid[16];
    struct pb_transport_driver *transport;
    pb_usb_io_t read;
    pb_usb_io_t write;
    pb_usb_call_t set_configuration;
    pb_usb_set_addr_t set_address;
    pb_usb_status_t enumerated;
};

int usb_process_setup_pkt(struct pb_usb_interface *iface,
                          struct usb_setup_packet *setup);

#endif  // INCLUDE_PB_USB_H_
