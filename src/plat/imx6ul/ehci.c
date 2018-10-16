/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb.h>
#include <plat.h>
#include <io.h>
#include <string.h>
#include <tinyprintf.h>
#include <board_config.h>

#include "ehci.h"

static struct ehci_transfer_head __no_bss __a4k dtds[EHCI_NO_OF_EPS*2][512];
static struct ehci_queue_head    __no_bss __a4k dqhs[EHCI_NO_OF_EPS*2];
static struct ehci_transfer_head *current_xfers[EHCI_NO_OF_EPS*2];

static void ehci_reset_queues(void)
{
    for (uint32_t i = 0; i < EHCI_NO_OF_EPS*2; i++) 
    {
        dqhs[i].caps = 0;
        dqhs[i].next = 0xDEAD0001;
        dqhs[i].token = 0;
        dqhs[i].current_dtd = 0;

        current_xfers[i] = NULL;
    }
}

static void ehci_reset(struct ehci_device *dev)
{

    pb_writel(0xFFFFFFFF, dev->base+EHCI_USBSTS);

    pb_writel(0xFFFF, dev->base+EHCI_ENDPTSETUPSTAT);
    pb_writel((0xff << 16)  | 0xff, dev->base+EHCI_ENDPTCOMPLETE);

    while (pb_readl(dev->base+EHCI_ENDPTPRIME))
        asm("nop");

    pb_writel(0xFFFFFFFF, dev->base+EHCI_ENDPTFLUSH);
 
    /* Wait for reset */
    while (!(pb_readl(dev->base+EHCI_USBSTS) & (1<<6)))
        asm("nop");
   
    /* Wait for port to come out of reset */
    while (pb_readl(dev->base+EHCI_PORTSC1) & (1<<8))
        asm("nop");
  
}

static struct ehci_queue_head * ehci_get_queue(uint32_t ep)
{
    return &dqhs[ep];
}

static void ehci_config_ep(uint32_t ep, uint32_t size, uint32_t flags)
{
    struct ehci_queue_head *qh = ehci_get_queue(ep);
    qh->caps = (1 << 29) | (size << 16) | flags;
}

static void ehci_prime_ep(struct ehci_device *dev, uint32_t ep)
{
    uint32_t epreg = 0;
    struct ehci_queue_head *qh = ehci_get_queue(ep);

    qh->next = (uint32_t) dtds[ep];
 
    if (ep & 1)
        epreg = (1 << ((ep-1)/2 + 16));
    else
        epreg = (1 << (ep/2));

    pb_writel(epreg, dev->base + EHCI_ENDPTPRIME);
    
    while (pb_readl(dev->base + EHCI_ENDPTPRIME) & epreg)
        asm("nop");
}

static uint32_t ehci_transfer(struct ehci_device *dev,
                                uint32_t ep,
                                uint8_t *bfr, 
                                uint32_t size)
{
    struct ehci_transfer_head *dtd = dtds[ep];
    uint32_t bytes_to_tx = size;
    uint8_t *data = bfr;

    if (bytes_to_tx == 0)
    {
        dtd->next = 0xDEAD0001;
        dtd->token = 0x80 | (1 << 15);
    }

    while (bytes_to_tx) 
    {
        dtd->next = 0xDEAD0001;
        dtd->token = 0x80;

        if (bytes_to_tx > 5*EHCI_PAGE_SZ)
            dtd->token |= (5*EHCI_PAGE_SZ) << 16;
        else
            dtd->token |= bytes_to_tx << 16;

        for (int n = 0; n < 5; n++)
        {
            dtd->page[n] = (uint32_t) data;
            
            if (bytes_to_tx)
            {
                if (bytes_to_tx >= EHCI_PAGE_SZ) 
                {
                    data += EHCI_PAGE_SZ;
                    bytes_to_tx -= EHCI_PAGE_SZ;
                } else {
                    data += bytes_to_tx;
                    bytes_to_tx = 0;
                }

            } else {
                data = NULL;
                dtd->token |= (1 << 15);
            }
        }
        
        if (bytes_to_tx) 
        {
            struct ehci_transfer_head *dtd_prev = dtd;
            dtd++;
            dtd_prev->next = (uint32_t) dtd;
        }
    }
    
    ehci_prime_ep(dev, ep);
    current_xfers[ep] = dtd;

    return PB_OK;
}

void plat_usb_wait_for_ep_completion(uint32_t ep)
{
    struct ehci_transfer_head *dtd = current_xfers[ep];

    if (dtd == NULL)
        return ;

    while (dtd->token & 0x80)   
        asm("nop");
}

uint32_t plat_usb_init(struct usb_device *dev) 
{
    uint32_t reg;
    struct ehci_device *ehci = (struct ehci_device *) dev->platform_data;

    LOG_INFO ("Init...");

    reg = pb_readl(ehci->base+EHCI_CMD);
    reg |= (1<<1);
    pb_writel(reg, ehci->base+EHCI_CMD);

    while (pb_readl(ehci->base+EHCI_CMD) & (1<<1))
        asm("nop");
    
    LOG_INFO("Reset complete");
    uint32_t usb_id = pb_readl(ehci->base);

    LOG_INFO("ID=0x%8.8lX",usb_id);

    uint32_t usb_dci_version = pb_readl(ehci->base+EHCI_DCIVERSION);

    LOG_INFO("Controller version: v%li.%li",
                                        (usb_dci_version >> 4)&0x0f,
                                        (usb_dci_version & 0x0f));


    ehci_reset_queues();
    ehci_config_ep(USB_EP0_IN,  EHCI_SZ_64B,  0);
    ehci_config_ep(USB_EP0_OUT, EHCI_SZ_64B,  0);
    ehci_config_ep(USB_EP1_OUT, EHCI_SZ_512B, EHCI_INTR_ON_COMPLETE);
    ehci_config_ep(USB_EP2_OUT, EHCI_SZ_64B,  EHCI_INTR_ON_COMPLETE);
    ehci_config_ep(USB_EP3_IN,  EHCI_SZ_512B, EHCI_INTR_ON_COMPLETE);

    /* Program QH top */
    pb_writel((uint32_t) dqhs, ehci->base + EHCI_ENDPTLISTADDR); 
 
    /* Enable USB */
    pb_writel(0x0A | (1 << 4), ehci->base + EHCI_USBMODE);
    pb_writel( (0x40 << 16) |0x01, ehci->base + EHCI_CMD);

    ehci_reset(ehci);

    pb_writel(7, ehci->base+EHCI_SBUSCFG);
    pb_writel(0x0000FFFF, ehci->base+EHCI_BURSTSIZE);

    LOG_INFO ("Init completed");

    return PB_OK;
}


uint32_t plat_usb_transfer (struct usb_device *dev, uint8_t ep, 
                            uint8_t *bfr, uint32_t sz) 
{
    struct ehci_device *ehci = (struct ehci_device *) dev->platform_data;
    return ehci_transfer(ehci, ep, bfr, sz);
}

void plat_usb_set_address(struct usb_device *dev, uint32_t addr)
{
    struct ehci_device *ehci = (struct ehci_device *) dev->platform_data;
    pb_writel(addr, ehci->base+EHCI_DEVICEADDR);
}

void plat_usb_set_configuration(struct usb_device *dev)
{
    struct ehci_device *ehci = (struct ehci_device *) dev->platform_data;

    /* Configure EP 1 as bulk OUT */
    pb_writel ((1 << 7) | (2 << 2) , ehci->base+EHCI_ENDPTCTRL1);
    /* Configure EP 2 as intr OUT */
    pb_writel ((1 << 7) | (3 << 2) , ehci->base+EHCI_ENDPTCTRL2);
    /* Configure EP3 as intr IN */
    pb_writel ((1 << 23) | (3 << 18), ehci->base+EHCI_ENDPTCTRL3);

    ehci_transfer(ehci, USB_EP2_OUT, (uint8_t *) &dev->cmd, 
                                        sizeof(struct usb_pb_command));
}

void plat_usb_task(struct usb_device *dev) 
{
    struct ehci_device *ehci = (struct ehci_device *) dev->platform_data;
    uint32_t sts = pb_readl(ehci->base+EHCI_USBSTS);
    uint32_t epc = pb_readl(ehci->base+EHCI_ENDPTCOMPLETE);
    struct usb_setup_packet setup_pkt;

    pb_writel(0xFFFFFFFF, ehci->base+EHCI_USBSTS);

    /* EP0 Process setup packets */
    if  (pb_readl(ehci->base+EHCI_ENDPTSETUPSTAT) & 1)
    {
        uint32_t cmd_reg = pb_readl(ehci->base+EHCI_CMD);
        struct ehci_queue_head *qh = ehci_get_queue(USB_EP0_OUT);

        do {
            pb_writel(cmd_reg | (1 << 13), ehci->base + EHCI_CMD);
            memcpy(&setup_pkt, qh->setup, sizeof(struct usb_setup_packet));
        } while (! (pb_readl(ehci->base + EHCI_CMD) & (1<<13)));
       
        pb_writel(1, ehci->base + EHCI_ENDPTSETUPSTAT);
        
        cmd_reg = pb_readl(ehci->base + EHCI_CMD);
        pb_writel(cmd_reg & ~(1 << 13), ehci->base + EHCI_CMD);

        pb_writel ((1<< 16) | 1, ehci->base + EHCI_ENDPTFLUSH);

        while (pb_readl(ehci->base + EHCI_ENDPTSETUPSTAT) & 1)
            asm("nop");

        dev->on_setup_pkt(dev, &setup_pkt);
    }

    /* EP2 INTR OUT */
    if  (epc & EHCI_EP2_OUT) 
    {
        dev->on_command(dev, &dev->cmd);
        ehci_transfer(ehci, USB_EP2_OUT, (uint8_t *) &dev->cmd, 
                                        sizeof(struct usb_pb_command));
    }

    pb_writel(epc, ehci->base + EHCI_ENDPTCOMPLETE);

    if (sts & 2) 
    {
        pb_writel (2, ehci->base+EHCI_USBSTS);
        LOG_ERR ("EHCI: Error %lx",sts);
        //dev->on_error(dev, sts);
    }
}

