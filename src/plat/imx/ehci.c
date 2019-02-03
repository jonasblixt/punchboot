/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <pb.h>
#include <plat.h>
#include <io.h>
#include <string.h>
#include <board/config.h>
#include <recovery.h>

#include <plat/imx/ehci.h>

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

    pb_write32(0xFFFFFFFF, dev->base+EHCI_USBSTS);

    pb_write32(0xFFFF, dev->base+EHCI_ENDPTSETUPSTAT);
    pb_write32((0xff << 16)  | 0xff, dev->base+EHCI_ENDPTCOMPLETE);

    LOG_DBG("Waiting for EP Prime");
    while (pb_read32(dev->base+EHCI_ENDPTPRIME))
        __asm__("nop");

    pb_write32(0xFFFFFFFF, dev->base+EHCI_ENDPTFLUSH);
 
    LOG_DBG("Wait for reset");
    /* Wait for reset */
    while (!(pb_read32(dev->base+EHCI_USBSTS) & (1<<6)))
        __asm__("nop");
   
    LOG_DBG("Wait for port reset");
    /* Wait for port to come out of reset */
    while (pb_read32(dev->base+EHCI_PORTSC1) & (1<<8))
        __asm__("nop");
  
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

    pb_write32(epreg, dev->base + EHCI_ENDPTPRIME);
    
    while (pb_read32(dev->base + EHCI_ENDPTPRIME) & epreg)
        __asm__("nop");
}

uint32_t ehci_transfer(struct ehci_device *dev,
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

void ehci_usb_wait_for_ep_completion(struct usb_device *dev, uint32_t ep)
{
    struct ehci_transfer_head *dtd = current_xfers[ep];
    UNUSED(dev);

    if (dtd == NULL)
        return ;

    while (dtd->token & 0x80)   
        __asm__("nop");
}

uint32_t ehci_usb_init(struct usb_device *dev) 
{
    uint32_t reg;
    struct ehci_device *ehci = (struct ehci_device *) dev->platform_data;

    LOG_DBG ("Init...");



    reg = pb_read32(ehci->base+EHCI_CMD);
    reg |= (1<<1);
    pb_write32(reg, ehci->base+EHCI_CMD);

    while (pb_read32(ehci->base+EHCI_CMD) & (1<<1))
        __asm__("nop");
    
    LOG_DBG("Reset complete");

    ehci_reset_queues();
    LOG_DBG("Queues reset");

    ehci_config_ep(USB_EP0_IN,  EHCI_SZ_64B,  0);
    ehci_config_ep(USB_EP0_OUT, EHCI_SZ_64B,  0);
    ehci_config_ep(USB_EP1_OUT, EHCI_SZ_512B, EHCI_INTR_ON_COMPLETE);
    ehci_config_ep(USB_EP2_OUT, EHCI_SZ_64B,  EHCI_INTR_ON_COMPLETE);
    ehci_config_ep(USB_EP3_IN,  EHCI_SZ_512B, EHCI_INTR_ON_COMPLETE);

    LOG_DBG("EP's configured");

    /* Program QH top */
    pb_write32((uint32_t) dqhs, ehci->base + EHCI_ENDPTLISTADDR); 
    
    LOG_DBG("QH loaded");

    /* Enable USB */
    pb_write32(0x0A | (1 << 4), ehci->base + EHCI_USBMODE);
    pb_write32( (0x40 << 16) |0x01, ehci->base + EHCI_CMD);

    LOG_DBG("USB Enable");

    ehci_reset(ehci);

    LOG_DBG("USB Reset complete");

    pb_write32(7, ehci->base+EHCI_SBUSCFG);
    pb_write32(0x0000FFFF, ehci->base+EHCI_BURSTSIZE);

    LOG_INFO ("Init completed");

    return PB_OK;
}


void ehci_usb_set_configuration(struct usb_device *dev)
{
    struct ehci_device *ehci = (struct ehci_device *) dev->platform_data;

    /* Configure EP 1 as bulk OUT */
    pb_write32 ((1 << 7) | (2 << 2) , ehci->base+EHCI_ENDPTCTRL1);
    /* Configure EP 2 as intr OUT */
    pb_write32 ((1 << 7) | (3 << 2) , ehci->base+EHCI_ENDPTCTRL2);
    /* Configure EP3 as intr IN */
    pb_write32 ((1 << 23) | (3 << 18), ehci->base+EHCI_ENDPTCTRL3);

    ehci_transfer(ehci, USB_EP2_OUT, (uint8_t *) &dev->cmd, 
                                        sizeof(struct pb_cmd_header));
}

void ehci_usb_task(struct usb_device *dev) 
{
    struct ehci_device *ehci = (struct ehci_device *) dev->platform_data;
    uint32_t sts = pb_read32(ehci->base+EHCI_USBSTS);
    uint32_t epc = pb_read32(ehci->base+EHCI_ENDPTCOMPLETE);
    struct usb_setup_packet setup_pkt;

    pb_write32(0xFFFFFFFF, ehci->base+EHCI_USBSTS);

    /* EP0 Process setup packets */
    if  (pb_read32(ehci->base+EHCI_ENDPTSETUPSTAT) & 1)
    {

        LOG_DBG("EP1");
        uint32_t cmd_reg = pb_read32(ehci->base+EHCI_CMD);
        struct ehci_queue_head *qh = ehci_get_queue(USB_EP0_OUT);

        do {
            pb_write32(cmd_reg | (1 << 13), ehci->base + EHCI_CMD);
            memcpy(&setup_pkt, qh->setup, sizeof(struct usb_setup_packet));
        } while (! (pb_read32(ehci->base + EHCI_CMD) & (1<<13)));
       
        pb_write32(1, ehci->base + EHCI_ENDPTSETUPSTAT);
        
        cmd_reg = pb_read32(ehci->base + EHCI_CMD);
        pb_write32(cmd_reg & ~(1 << 13), ehci->base + EHCI_CMD);

        pb_write32 ((1<< 16) | 1, ehci->base + EHCI_ENDPTFLUSH);

        while (pb_read32(ehci->base + EHCI_ENDPTSETUPSTAT) & 1)
            __asm__("nop");

        dev->on_setup_pkt(dev, &setup_pkt);
    }

    /* EP2 INTR OUT */
    if  (epc & EHCI_EP2_OUT) 
    {
        dev->on_command(dev, &dev->cmd);
        /* Queue up next command to be received */
        ehci_transfer(ehci, USB_EP2_OUT, (uint8_t *) &dev->cmd, 
                                        sizeof(struct pb_cmd_header));
    }

    pb_write32(epc, ehci->base + EHCI_ENDPTCOMPLETE);

    if (sts & 2) 
    {
        pb_write32 (2, ehci->base+EHCI_USBSTS);
        LOG_ERR ("EHCI: Error %lx",sts);
        //dev->on_error(dev, sts);
    }
}

