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
#include <usb.h>

#define EHCI_DCIVERSION 0x120
#define EHCI_USBSTS 0x144
#define EHCI_ENDPTCOMPLETE 0x1bc
#define EHCI_USBMODE 0x1a8
#define EHCI_ENDPTLISTADDR 0x158
#define EHCI_CMD 0x140
#define EHCI_ENDPTSETUPSTAT 0x1ac
#define EHCI_ENDPTPRIME 0x1b0
#define EHCI_PORTSC1 0x184
#define EHCI_ENDPTFLUSH 0x1b4
#define EHCI_ENDPTSETUPSTAT 0x1ac
#define EHCI_DEVICEADDR 0x154
#define EHCI_ENDPTCTRL1 0x1c4
#define EHCI_ENDPTCTRL2 0x1c8
#define EHCI_ENDPTCTRL3 0x1cc
#define EHCI_BURSTSIZE 0x160
#define EHCI_SBUSCFG 0x90
#define EHCI_ENDPTSTAT 0x1b8
#define EHCI_ENDPTNAKEN 0x17C

#define EHCI_NO_OF_EPS 8
#define EHCI_NO_OF_DESCRIPTORS 512
#define EHCI_SZ_512B 0x200
#define EHCI_SZ_64B 0x40
#define EHCI_INTR_ON_COMPLETE (1 << 15)
#define EHCI_PAGE_SZ 4096

#define EHCI_EP0_OUT (1 << 0)
#define EHCI_EP0_IN  (1 << 16)
#define EHCI_EP1_OUT (1 << 1)
#define EHCI_EP1_IN  (1 << 17)
#define EHCI_EP2_OUT (1 << 2)
#define EHCI_EP2_IN  (1 << 18)
#define EHCI_EP3_OUT (1 << 3)
#define EHCI_EP3_IN  (1 << 19)

struct ehci_transfer_head {
    uint32_t next;
    uint32_t token;
    uint32_t page[5];
} __attribute__((aligned(32))) __attribute__((packed));

struct ehci_queue_head {
    uint32_t caps;
    uint32_t current_dtd;
    uint32_t next;
    uint32_t token;
    uint32_t page[5];
    uint32_t __reserved;
    uint32_t setup[2];
    uint32_t padding[4];
} __attribute__((aligned(32))) __attribute__((packed));

struct ehci_device {
    __iomem base;
};


uint32_t  ehci_usb_init(struct usb_device *dev);

void ehci_usb_set_configuration(struct usb_device *dev);
void ehci_usb_wait_for_ep_completion(struct usb_device *dev, uint32_t ep);
void ehci_usb_task(struct usb_device *dev) ;

uint32_t ehci_transfer(struct ehci_device *dev,
                                uint32_t ep,
                                uint8_t *bfr, 
                                uint32_t size);
#endif
