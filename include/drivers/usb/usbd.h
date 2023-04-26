/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_DRIVERS_USB_USBD_H
#define INCLUDE_DRIVERS_USB_USBD_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
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
    USB_EP_END,
} usb_ep_t;

enum usb_ep_type
{
    USB_EP_TYPE_INVALID,
    USB_EP_TYPE_CONTROL,
    USB_EP_TYPE_ISO,
    USB_EP_TYPE_BULK,
    USB_EP_TYPE_INTR,
    USB_EP_TYPE_END,
};

enum usb_charger_type
{
    USB_CHARGER_INVALID,
    USB_CHARGER_CDP,
    USB_CHARGER_DCP,
    USB_CHARGER_SDP,
    USB_CHARGER_END,
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

struct usb_language_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wLang;
} __attribute__((packed));

struct usb_string_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t unicode[127];
} __attribute__((packed));

struct usb_qualifier_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint8_t bNumConfigurations;
    uint8_t bReserved;
} __attribute__((packed));

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

struct usbd_descriptors
{
    const struct usb_language_descriptor language;
    const struct usb_qualifier_descriptor qualifier;
    const struct usb_device_descriptor device;
    const struct usb_configuration_descriptor config;
    const struct usb_interface_descriptor interface;
    const struct usb_endpoint_descriptor eps[];
} __attribute__((packed));

struct usbd_cls_config
{
    const struct usbd_descriptors *desc;
    struct usb_string_descriptor * (*get_string_descriptor)(uint8_t index);
};

struct usbd_hal_ops
{
    int (*init)(void);
    int (*stop)(void);
    int (*xfer_start)(usb_ep_t ep, void *buf, size_t length);
    int (*xfer_complete)(usb_ep_t ep);
    void (*xfer_cancel)(usb_ep_t ep);
    int (*poll_setup_pkt)(struct usb_setup_packet *pkt);
    int (*configure_ep)(usb_ep_t ep, enum usb_ep_type ep_type, size_t pkt_sz);
    int (*set_address)(uint16_t addr);
    int (*ep0_xfer_zlp)(usb_ep_t ep);
};

/* USB Device interface */
const char *ep_to_str(usb_ep_t ep);
int usbd_init_hal_ops(const struct usbd_hal_ops *ops);
int usbd_init_cls(const struct usbd_cls_config *cfg);
int usbd_init(void);
int usbd_connect(void);
int usbd_disconnect(void);
int usbd_xfer(usb_ep_t ep, void *buf, size_t length);


#endif  // INCLUDE_DRIVERS_USB_USBD_H
