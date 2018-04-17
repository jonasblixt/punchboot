#ifndef __USB_H__
#define __USB_H__

#include <types.h>

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

#define USBPHY_BASE 0x020C9000

struct ehci_dTH {
    u32 next_dtd;
    u32 dtd_token;
    u32 bfr_page0;
    u32 bfr_page1;
    u32 bfr_page2;
    u32 bfr_page3;
    u32 bfr_page4;
} __attribute__((aligned(32)));

struct ehci_dQH {
    u32 caps;
    u32 current_dtd;
    u32 next_dtd;
    u32 dtd_token;
    u32 bfr_page0;
    u32 bfr_page1;
    u32 bfr_page2;
    u32 bfr_page3;
    u32 bfr_page4;
    u32 __reserved;
    u32 setup[2];
    u32 padding[4];
} ;



/* Defines for commands in setup packets */
#define GET_DESCRIPTOR				0x8006
#define SET_CONFIGURATION			0x0009
#define SET_IDLE					0x210A
#define GET_HID_CLASS_DESCRIPTOR	0x8106
#define SET_HID_REPORT              0x2109
#define SET_FEATURE					0x0003
#define SET_ADDRESS                 0x0005

struct usbdSetupPacket {
    u8 bRequestType;	//! Characteristics of request 
    u8 bRequest;           //! Specific request
    u16 wValue;            //! Word-sized field that varies according to request
    u16 wIndex;            //! Index
    u16 wLength;           //! Number of bytes to transfer
}  __attribute__ ((packed));

struct usb_device_descriptor {
    u8 bLength;		//! Size of this descriptor in bytes
    u8 bDescriptorType;	//! DEVICE Descriptor Type
    u16 bcdUSB;		//! USB Specification Release Number
    u8 bDeviceClass;	//! Class code
    u8 bDeviceSubClass;	//! Subclass code
    u8 bDeviceProtocol;	//! Protocol code
    u8 bMaxPacketSize;	//! Maximum packet size for endpoint zero
    u16 idVendor;          //! Vendor ID
    u16 idProduct;         //! Product ID
    u16 bcdDevice;         //! Device release number
    u8 iManufacturer;      //! Index of string descriptor describing manufacturer
    u8 iProduct;           //! Index of string descriptor describing product
    u8 iSerialNumber; 	//! Index of string descriptor describing the device's serial number
    u8 bNumConfigurations;//! Number of possible configurations
}  __attribute__ ((packed));

struct usb_configuration_descriptor {
    u8 bLength;			//! Size of descriptor	
    u8 bDescriptorType;		//! CONFIGURATION Descriptor Type
    u16 wTotalLength;              //! Total length of data returned for this configuration
    u8 bNumInterfaces;             //! Number of interfaces supported by this configuration
    u8 bConfigurationValue;        //! Value to use as an argument to the to select this configuration
    u8 iConfiguration;             //! Index of string descriptor describing this configuration
    u8 bmAttributes;		//! Configuration characteristics
    u8 MaxPower;			//! Maximum power consumption of the USB device
}  __attribute__ ((packed));

struct usb_interface_descriptor {
    u8 bLength;		//! Size of this descriptor in bytes 
    u8 bDescriptorType;    //! INTERFACE Descriptor Type
    u8 bInterfaceNumber;   //! Number of this interface
    u8 bAlternateSetting;  //! Value used to select this alternate setting
    u8 bNumEndpoints;      //! Number of endpoints used by this interface
    u8 bInterfaceClass;    //! Class code
    u8 bInterfaceSubClass; //! Subclass code
    u8 bInterfaceProtocol; //! Protocol code
    u8 iInterface;		//! Index of string descriptor describing this interface
}  __attribute__ ((packed));


struct usb_endpoint_descriptor {
    u8 bLength;		//! Size of this descriptor in bytes 
    u8 bDescriptorType;    //! ENDPOINT Descriptor Type
    u8 bEndpointAddress;   //! The address of the endpoint on the USB device described by this descriptor
    u8 bmAttributes;       //! The endpoint'ss attributes
    u16 wMaxPacketSize;    //! Maximum packet size
    u8 bInterval;          //! Interval for polling endpoint for data transfers
}  __attribute__ ((packed));

struct usb_hid_descriptor {
    u8 bLength;			//! Size of this descriptor in bytes 
    u8 bDescriptorType;            //! Descriptor Type
    u16 bcdHID;                    //! Release number
    u8 bCountryCode;               //! Country code
    u8 bNumDescriptors;            //! Number of descriptors
    u8 bReportDescriptorType;      //! The type of report descriptor
    u8 wDescriptorLength[2];   // !!!! Not aligned on 16-bit boundary  !!!
}  __attribute__ ((packed));



struct usb_descriptors {
    const struct usb_device_descriptor device;
    const struct usb_configuration_descriptor config;
    const struct usb_interface_descriptor interface;
    const struct usb_hid_descriptor hid;
    const struct usb_endpoint_descriptor endpoint1_in;
} __attribute__ ((packed));


struct ehci_device {
    __iomem base;
    u8 enumerated ;
    u8 ready;
    struct ehci_dQH __attribute__((aligned(4096))) dqh[32]  ;
    struct usbdSetupPacket setup;
};


void soc_usb_init(u32 base_addr);
void soc_usb_task(void);

#endif
