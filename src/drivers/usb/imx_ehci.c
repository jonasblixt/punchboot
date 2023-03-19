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
#include <arch/arch.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/io.h>
#include <plat/defs.h>
#include <pb-tools/wire.h>
#include <drivers/usb/usb_core.h>
#include <drivers/usb/imx_ehci.h>

static struct ehci_transfer_head dtds[EHCI_NO_OF_EPS*2][512] PB_SECTION_NO_INIT PB_ALIGN_4k;
static struct ehci_queue_head    dqhs[EHCI_NO_OF_EPS*2] PB_SECTION_NO_INIT PB_ALIGN_4k;
static struct ehci_transfer_head *current_xfers[EHCI_NO_OF_EPS*2];

static struct pb_usb_interface iface;

static void ehci_reset_queues(void)
{
    for (int i = 0; i < EHCI_NO_OF_EPS*2; i++)
    {
        dqhs[i].caps = 0;
        dqhs[i].next = 0xDEAD0001;
        dqhs[i].token = 0;
        dqhs[i].current_dtd = 0;

        current_xfers[i] = NULL;
    }

}

static void ehci_reset(void)
{

    pb_write32(0xFFFFFFFF, IMX_EHCI_BASE+EHCI_USBSTS);

    pb_write32(0xFFFF, IMX_EHCI_BASE+EHCI_ENDPTSETUPSTAT);
    pb_write32((0xff << 16)  | 0xff, IMX_EHCI_BASE+EHCI_ENDPTCOMPLETE);

    LOG_DBG("Waiting for EP Prime");
    while (pb_read32(IMX_EHCI_BASE+EHCI_ENDPTPRIME))
        __asm__("nop");

    pb_write32(0xFFFFFFFF, IMX_EHCI_BASE+EHCI_ENDPTFLUSH);

    LOG_DBG("Wait for port reset");
    /* Wait for port to come out of reset */
    while ((pb_read32(IMX_EHCI_BASE+EHCI_PORTSC1) & (1<<8)) == (1 <<8))
        __asm__("nop");
}

static inline struct ehci_queue_head * ehci_get_queue(int ep)
{
    return &dqhs[ep];
}

static int ehci_config_ep(int ep, uint32_t size, uint32_t flags)
{
    struct ehci_queue_head *qh = ehci_get_queue(ep);

    qh->caps = (1 << 29) | (size << 16) | flags;

    arch_clean_cache_range((uintptr_t) qh, sizeof(*qh));

    return PB_OK;
}

static int ehci_prime_ep(int ep)
{
    uint32_t epreg = 0;
    struct ehci_queue_head *qh = ehci_get_queue(ep);

    qh->next = (uint32_t) (uintptr_t) dtds[ep];

    arch_clean_cache_range((uintptr_t) qh, sizeof(*qh));
    arch_clean_cache_range((uintptr_t) dtds[ep], sizeof(dtds[0][0])*512);

    if (ep & 1)
    {
        epreg = (1 << ((ep-1)/2 + 16));
    }
    else
    {
        epreg = (1 << (ep/2));
    }

    pb_write32(epreg, IMX_EHCI_BASE + EHCI_ENDPTPRIME);

    while ((pb_read32(IMX_EHCI_BASE + EHCI_ENDPTPRIME) & epreg) == epreg)
        __asm__("nop");

    return PB_OK;
}

static int ehci_usb_wait_for_ep_completion(int ep)
{
    volatile struct ehci_transfer_head *dtd = current_xfers[ep];

    arch_invalidate_cache_range((uintptr_t) dtd, sizeof(*dtd));

    if (dtd == NULL)
        return -PB_ERR;

    while (dtd->token & 0x80)
    {
        plat_wdog_kick();
        imx_ehci_usb_process();

        arch_invalidate_cache_range((uintptr_t) dtd, sizeof(*dtd));

        if (!current_xfers[ep])
            return -PB_ERR;
    }

    return PB_OK;
}

static int ehci_transfer(int ep, void *bfr, size_t size)
{
    struct ehci_transfer_head *dtd = dtds[ep];
    int rc;
    uint32_t bytes_to_tx = size;
    uint8_t *data = bfr;

    if (bytes_to_tx == 0)
    {
        dtd->next = 0xDEAD0001;
        dtd->token = 0x80 | (1 << 15);
    }

    if (bfr && size) /* Input to host, flush cache*/
    {
        arch_clean_cache_range((uintptr_t) bfr, size);
    }

    while (bytes_to_tx)
    {
        dtd->next = 0xDEAD0001;
        dtd->token = 0x80;

        if (bytes_to_tx > (5*EHCI_PAGE_SZ))
        {
            dtd->token |= ((5*EHCI_PAGE_SZ) << 16);
        }
        else
        {
            dtd->token |= (bytes_to_tx << 16);
        }

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
                arch_clean_cache_range((uintptr_t) dtd, sizeof(*dtd));
            }
        }

        if (bytes_to_tx)
        {
            struct ehci_transfer_head *dtd_prev = dtd;
            dtd++;
            dtd_prev->next = (uint32_t)(uintptr_t) dtd;
            arch_clean_cache_range((uintptr_t) dtd_prev, sizeof(*dtd_prev));
        }

    }

    ehci_prime_ep(ep);
    current_xfers[ep] = dtd;

    rc = ehci_usb_wait_for_ep_completion(ep);

    if (!(ep & 1) && bfr && size) /* Output from host, invalidate cache*/
    {
        arch_invalidate_cache_range((uintptr_t) bfr, size);
    }

    return rc;
}

static int imx_ehci_usb_set_configuration(void)
{

    /* Configure EP 1 as bulk IN */
    pb_write32((1 << 23) | (2 << 18) | (1 << 6),
                (IMX_EHCI_BASE+EHCI_ENDPTCTRL1));
    /* Configure EP 2 as bulk OUT */
    pb_write32((1 << 7) | (2 << 2) | (1 << 6),
                (IMX_EHCI_BASE+EHCI_ENDPTCTRL2));

    return PB_OK;
}

int imx_ehci_usb_process(void)
{
    uint32_t sts = pb_read32(IMX_EHCI_BASE+EHCI_USBSTS);
    uint32_t epc = pb_read32(IMX_EHCI_BASE+EHCI_ENDPTCOMPLETE);
    struct usb_setup_packet setup_pkt;
    uint32_t tmp;

    pb_write32(0xFFFFFFFF, IMX_EHCI_BASE+EHCI_USBSTS);

    /* EP0 Process setup packets */
    if  (pb_read32(IMX_EHCI_BASE+EHCI_ENDPTSETUPSTAT) & 1)
    {
        uint32_t cmd_reg = pb_read32(IMX_EHCI_BASE+EHCI_CMD);
        struct ehci_queue_head *qh = ehci_get_queue(USB_EP0_OUT);

        do
        {
            pb_write32(cmd_reg | (1 << 13), IMX_EHCI_BASE + EHCI_CMD);

            arch_invalidate_cache_range((uintptr_t) qh->setup,
                                                    sizeof(*qh->setup));
            memcpy(&setup_pkt, qh->setup, sizeof(struct usb_setup_packet));
        } while (!(pb_read32(IMX_EHCI_BASE + EHCI_CMD) & (1<<13)));

        pb_write32(1, IMX_EHCI_BASE + EHCI_ENDPTSETUPSTAT);

        cmd_reg = pb_read32(IMX_EHCI_BASE + EHCI_CMD);
        pb_write32(cmd_reg & ~(1 << 13), IMX_EHCI_BASE + EHCI_CMD);

        pb_write32((1<< 16) | 1, IMX_EHCI_BASE + EHCI_ENDPTFLUSH);

        while (pb_read32(IMX_EHCI_BASE + EHCI_ENDPTSETUPSTAT) & 1)
            __asm__("nop");

        usb_process_setup_pkt(&iface, &setup_pkt);
    }

    pb_write32(epc, IMX_EHCI_BASE + EHCI_ENDPTCOMPLETE);

    if (sts & (1 << 6))
    {
        LOG_DBG("Got RST");

        for (uint32_t i = 1; i < EHCI_NO_OF_EPS*2; i++)
        {
            dqhs[i].next = 0xDEAD0001;
            dqhs[i].token = 0;
            dqhs[i].current_dtd = 0;
            current_xfers[i] = NULL;

            arch_clean_cache_range((uintptr_t) &dqhs[i], sizeof(dqhs[0]));
        }

        tmp = pb_read32(IMX_EHCI_BASE + EHCI_ENDPTSETUPSTAT);
        pb_write32(tmp, IMX_EHCI_BASE + EHCI_ENDPTSETUPSTAT);

        tmp = pb_read32(IMX_EHCI_BASE + EHCI_ENDPTCOMPLETE);
        pb_write32(tmp, IMX_EHCI_BASE + EHCI_ENDPTCOMPLETE);

        pb_write32(0, IMX_EHCI_BASE+EHCI_DEVICEADDR);

        while (pb_read32(IMX_EHCI_BASE+EHCI_ENDPTPRIME))
            __asm__("nop");

        pb_write32(0x00FF00FF, IMX_EHCI_BASE+EHCI_ENDPTFLUSH);
    }

    if (sts & 2)
    {
        pb_write32(2, IMX_EHCI_BASE+EHCI_USBSTS);
        LOG_ERR("EHCI: Error %x", sts);
    }

    return PB_OK;
}

int imx_ehci_usb_read(void *buf, size_t size)
{
    return ehci_transfer(USB_EP2_OUT, buf, size);
}

int imx_ehci_usb_write(void *buf, size_t size)
{
    return ehci_transfer(USB_EP1_IN, buf, size);
}

int imx_ehci_usb_init(void)
{
    LOG_DBG("Init");

    pb_setbit32(1<<1, IMX_EHCI_BASE + EHCI_CMD);

    LOG_DBG("Waiting for reset");

    while ((pb_read32(IMX_EHCI_BASE+EHCI_CMD) & (1<<1)) == (1 << 1))
        __asm__("nop");

    LOG_DBG("Reset complete");

    ehci_reset_queues();
    LOG_DBG("Queues reset");

    ehci_config_ep(USB_EP0_IN,  EHCI_SZ_64B,  0);
    ehci_config_ep(USB_EP0_OUT, EHCI_SZ_64B,  0);
    ehci_config_ep(USB_EP1_IN,  EHCI_SZ_512B, EHCI_INTR_ON_COMPLETE);
    ehci_config_ep(USB_EP2_OUT, EHCI_SZ_512B, EHCI_INTR_ON_COMPLETE);

    LOG_DBG("EP's configured");

    /* Program QH top */

    pb_write32((uint32_t)(uintptr_t) dqhs, IMX_EHCI_BASE + EHCI_ENDPTLISTADDR);

    LOG_DBG("QH loaded");

    /* Enable USB */
    pb_write32(0x0A | (1 << 4), IMX_EHCI_BASE + EHCI_USBMODE);
    pb_write32((0x40 << 16) |0x01, IMX_EHCI_BASE + EHCI_CMD);

    LOG_DBG("USB Enable");

    ehci_reset();

    LOG_DBG("USB Reset complete");

    pb_write32(7, IMX_EHCI_BASE+EHCI_SBUSCFG);
    pb_write32(0x0000FFFF, IMX_EHCI_BASE+EHCI_BURSTSIZE);

    LOG_INFO("Init completed");

    iface.read = ehci_transfer;
    iface.write = ehci_transfer;
    iface.set_address = imx_ehci_set_address;
    iface.set_configuration = imx_ehci_usb_set_configuration;
    iface.enumerated = false;

    return PB_OK;
}

bool imx_ehci_usb_ready(void)
{
    return iface.enumerated;
}
