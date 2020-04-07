/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/io.h>
#include <pb/usb.h>
#include <board/config.h>
#include <plat/imx/ehci.h>
#include <pb-tools/wire.h>
#include <pb/transport.h>

struct imx_ehci_private
{
    struct ehci_transfer_head dtds[EHCI_NO_OF_EPS*2][512] __a4k;
    struct ehci_queue_head    dqhs[EHCI_NO_OF_EPS*2] __a4k;
    struct ehci_transfer_head *current_xfers[EHCI_NO_OF_EPS*2];
};

#define PB_IMX_EHCI(drv) ((struct imx_ehci_device *) drv->private)
#define PB_IMX_PRIV(dev) ((struct imx_ehci_private *) dev->private)

static int imx_ehci_usb_process(struct pb_transport_driver *drv);

static void ehci_reset_queues(struct pb_transport_driver *drv)
{
    struct imx_ehci_device *dev = PB_IMX_EHCI(drv);
    struct imx_ehci_private *priv = PB_IMX_PRIV(dev);

    for (int i = 0; i < EHCI_NO_OF_EPS*2; i++)
    {
        priv->dqhs[i].caps = 0;
        priv->dqhs[i].next = 0xDEAD0001;
        priv->dqhs[i].token = 0;
        priv->dqhs[i].current_dtd = 0;

        priv->current_xfers[i] = NULL;
    }
}

static void ehci_reset(struct pb_transport_driver *drv)
{
    struct imx_ehci_device *dev = PB_IMX_EHCI(drv);

    pb_write32(0xFFFFFFFF, dev->base+EHCI_USBSTS);

    pb_write32(0xFFFF, dev->base+EHCI_ENDPTSETUPSTAT);
    pb_write32((0xff << 16)  | 0xff, dev->base+EHCI_ENDPTCOMPLETE);

    LOG_DBG("Waiting for EP Prime");
    while (pb_read32(dev->base+EHCI_ENDPTPRIME))
        __asm__("nop");

    pb_write32(0xFFFFFFFF, dev->base+EHCI_ENDPTFLUSH);

    LOG_DBG("Wait for port reset");
    /* Wait for port to come out of reset */
    while ((pb_read32(dev->base+EHCI_PORTSC1) & (1<<8)) == (1 <<8))
        __asm__("nop");
}

static struct ehci_queue_head * ehci_get_queue(struct pb_transport_driver *drv,
                                               int ep)
{
    struct imx_ehci_device *dev = PB_IMX_EHCI(drv);
    struct imx_ehci_private *priv = PB_IMX_PRIV(dev);

    return &priv->dqhs[ep];
}

static int ehci_config_ep(struct pb_transport_driver *drv,
                            int ep, uint32_t size, uint32_t flags)
{
    struct ehci_queue_head *qh = ehci_get_queue(drv, ep);

    qh->caps = (1 << 29) | (size << 16) | flags;

    return PB_OK;
}

static int ehci_prime_ep(struct pb_transport_driver *drv, int ep)
{
    struct imx_ehci_device *dev = PB_IMX_EHCI(drv);
    struct imx_ehci_private *priv = PB_IMX_PRIV(dev);
    uint32_t epreg = 0;
    struct ehci_queue_head *qh = ehci_get_queue(drv, ep);

    qh->next = (uint32_t) (uintptr_t) priv->dtds[ep];

    if (ep & 1)
        epreg = (1 << ((ep-1)/2 + 16));
    else
        epreg = (1 << (ep/2));

    pb_write32(epreg, dev->base + EHCI_ENDPTPRIME);

    while ((pb_read32(dev->base + EHCI_ENDPTPRIME) & epreg) == epreg)
        __asm__("nop");

    return PB_OK;
}

static int ehci_usb_wait_for_ep_completion(struct pb_transport_driver *drv,
                                            int ep)
{
    struct imx_ehci_device *dev = PB_IMX_EHCI(drv);
    struct imx_ehci_private *priv = PB_IMX_PRIV(dev);

    volatile struct ehci_transfer_head *dtd = priv->current_xfers[ep];

    if (dtd == NULL)
        return -PB_ERR;

    while (dtd->token & 0x80)
    {
        plat_wdog_kick();
        imx_ehci_usb_process(drv);

        if (!priv->current_xfers[ep])
            return -PB_ERR;
    }

    return PB_OK;
}

static int ehci_transfer(struct pb_transport_driver *drv,
                            int ep, void *bfr, size_t size)
{
    struct imx_ehci_device *dev = PB_IMX_EHCI(drv);
    struct imx_ehci_private *priv = PB_IMX_PRIV(dev);
    struct ehci_transfer_head *dtd = priv->dtds[ep];
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
            dtd->page[n] = (uint32_t)(uintptr_t) data;

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
            dtd_prev->next = (uint32_t)(uintptr_t) dtd;
        }
    }

    ehci_prime_ep(drv, ep);
    priv->current_xfers[ep] = dtd;

    return ehci_usb_wait_for_ep_completion(drv, ep);
}


static int imx_ehci_usb_set_configuration(struct pb_transport_driver *drv)
{
    struct imx_ehci_device *dev = PB_IMX_EHCI(drv);

    /* Configure EP 1 as bulk IN */
    pb_write32((1 << 23) | (2 << 18) | (1 << 6), (dev->base+EHCI_ENDPTCTRL1));
    /* Configure EP 2 as bulk OUT */
    pb_write32((1 << 7) | (2 << 2) | (1 << 6), (dev->base+EHCI_ENDPTCTRL2));

    drv->ready = true;

    return PB_OK;
}

static int imx_ehci_usb_process(struct pb_transport_driver *drv)
{
    struct imx_ehci_device *dev = PB_IMX_EHCI(drv);
    struct imx_ehci_private *priv = PB_IMX_PRIV(dev);

    uint32_t sts = pb_read32(dev->base+EHCI_USBSTS);
    uint32_t epc = pb_read32(dev->base+EHCI_ENDPTCOMPLETE);
    struct usb_setup_packet setup_pkt;
    uint32_t tmp;

    pb_write32(0xFFFFFFFF, dev->base+EHCI_USBSTS);

    /* EP0 Process setup packets */
    if  (pb_read32(dev->base+EHCI_ENDPTSETUPSTAT) & 1)
    {
        uint32_t cmd_reg = pb_read32(dev->base+EHCI_CMD);
        struct ehci_queue_head *qh = ehci_get_queue(drv, USB_EP0_OUT);

        do
        {
            pb_write32(cmd_reg | (1 << 13), dev->base + EHCI_CMD);
            memcpy(&setup_pkt, qh->setup, sizeof(struct usb_setup_packet));
        } while (!(pb_read32(dev->base + EHCI_CMD) & (1<<13)));

        pb_write32(1, dev->base + EHCI_ENDPTSETUPSTAT);

        cmd_reg = pb_read32(dev->base + EHCI_CMD);
        pb_write32(cmd_reg & ~(1 << 13), dev->base + EHCI_CMD);

        pb_write32((1<< 16) | 1, dev->base + EHCI_ENDPTFLUSH);

        while (pb_read32(dev->base + EHCI_ENDPTSETUPSTAT) & 1)
            __asm__("nop");

        usb_process_setup_pkt(&dev->iface, &setup_pkt);
    }

    pb_write32(epc, dev->base + EHCI_ENDPTCOMPLETE);

    if (sts & (1 << 6))
    {
        LOG_DBG("Got RST");

        for (uint32_t i = 1; i < EHCI_NO_OF_EPS*2; i++)
        {
            priv->dqhs[i].next = 0xDEAD0001;
            priv->dqhs[i].token = 0;
            priv->dqhs[i].current_dtd = 0;
            priv->current_xfers[i] = NULL;
        }

        tmp = pb_read32(dev->base + EHCI_ENDPTSETUPSTAT);
        pb_write32(tmp, dev->base + EHCI_ENDPTSETUPSTAT);

        tmp = pb_read32(dev->base + EHCI_ENDPTCOMPLETE);
        pb_write32(tmp, dev->base + EHCI_ENDPTCOMPLETE);

        pb_write32(0, dev->base+EHCI_DEVICEADDR);

        while (pb_read32(dev->base+EHCI_ENDPTPRIME))
            __asm__("nop");

        pb_write32(0x00FF00FF, dev->base+EHCI_ENDPTFLUSH);
    }

    if (sts & 2)
    {
        pb_write32(2, dev->base+EHCI_USBSTS);
        LOG_ERR("EHCI: Error %x", sts);
    }

    return PB_OK;
}

static int imx_ehci_usb_read(struct pb_transport_driver *drv,
                              void *buf, size_t size)
{
    return ehci_transfer(drv, USB_EP2_OUT, buf, size);
}


static int imx_ehci_usb_write(struct pb_transport_driver *drv,
                              void *buf, size_t size)
{
    return ehci_transfer(drv, USB_EP1_IN, buf, size);
}

int imx_ehci_usb_free(struct pb_transport_driver *drv)
{
    return PB_OK;
}

int imx_ehci_usb_init(struct pb_transport_driver *drv)
{
    struct imx_ehci_device *dev = PB_IMX_EHCI(drv);
    struct imx_ehci_private *priv = PB_IMX_PRIV(dev);

    LOG_DBG("Init... base: %p %p", (void *) dev->base, priv->dqhs);

    if (sizeof(*priv) > dev->size)
        return -PB_ERR_MEM;

    memset(priv, 0, sizeof(*priv));
    pb_setbit32(1<<1, dev->base + EHCI_CMD);

    LOG_DBG("Waiting for reset");

    while ((pb_read32(dev->base+EHCI_CMD) & (1<<1)) == (1 << 1))
        __asm__("nop");

    LOG_DBG("Reset complete");

    ehci_reset_queues(drv);
    LOG_DBG("Queues reset");

    ehci_config_ep(drv, USB_EP0_IN,  EHCI_SZ_64B,  0);
    ehci_config_ep(drv, USB_EP0_OUT, EHCI_SZ_64B,  0);
    ehci_config_ep(drv, USB_EP1_IN,  EHCI_SZ_512B, EHCI_INTR_ON_COMPLETE);
    ehci_config_ep(drv, USB_EP2_OUT, EHCI_SZ_512B, EHCI_INTR_ON_COMPLETE);

    LOG_DBG("EP's configured");

    /* Program QH top */
    pb_write32((uint32_t)(uintptr_t) priv->dqhs, dev->base + EHCI_ENDPTLISTADDR);

    LOG_DBG("QH loaded");

    /* Enable USB */
    pb_write32(0x0A | (1 << 4), dev->base + EHCI_USBMODE);
    pb_write32((0x40 << 16) |0x01, dev->base + EHCI_CMD);

    LOG_DBG("USB Enable");

    ehci_reset(drv);

    LOG_DBG("USB Reset complete");

    pb_write32(7, dev->base+EHCI_SBUSCFG);
    pb_write32(0x0000FFFF, dev->base+EHCI_BURSTSIZE);

    LOG_INFO("Init completed");

    dev->iface.transport = drv;
    dev->iface.write = ehci_transfer;
    dev->iface.read = ehci_transfer;
    dev->iface.set_configuration = imx_ehci_usb_set_configuration;

    drv->process = imx_ehci_usb_process;
    drv->read = imx_ehci_usb_read;
    drv->write = imx_ehci_usb_write;

    return PB_OK;
}
