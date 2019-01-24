#ifndef __USB_H__
#define __USB_H__

#include <pb.h>
#include <recovery.h>

/* Defines for commands in setup packets */
#define USB_GET_DESCRIPTOR				0x8006
#define USB_SET_CONFIGURATION			0x0009
#define USB_SET_IDLE					0x210A
#define USB_SET_FEATURE					0x0003
#define USB_SET_ADDRESS                 0x0005
#define USB_GET_STATUS                  0x8000

enum {
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

struct usb_setup_packet {
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

struct usb_device;

typedef uint32_t (*usb_on_setup_pkt_t) (struct usb_device *dev, 
                                        struct usb_setup_packet *pkt);
typedef void     (*usb_on_command_t)   (struct usb_device *dev,
                                        struct pb_cmd_header *cmd);
typedef void     (*usb_on_error_t)     (struct usb_device *dev,
                                        uint32_t error_code);

struct usb_device {
    void *platform_data;
    struct pb_cmd_header cmd;
    usb_on_setup_pkt_t on_setup_pkt;
    usb_on_command_t on_command;
    usb_on_error_t on_error;
};

uint32_t usb_init(void);
void usb_task(void);
void usb_set_on_command_handler(usb_on_command_t handler);

#endif
