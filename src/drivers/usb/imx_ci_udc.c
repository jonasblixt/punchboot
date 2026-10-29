/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * LIMITATIONS:
 *
 * - This driver only supports one queued transfer at the time. The 'dtds' array
 * of transfer descriptors is shared between all endpoints.
 *
 * - It only supports device mode.
 *
 * - The amount of transfer descriptors (dtds) set's an upper limit to xfer
 *   chunk size. One descriptor can address 5 * 4kByte. For now the config
 *   'CONFIG_CM_BUF_SIZE_KiB' is used both at driver level and in the cm
 *   code. The 'CONFIG_CM_BUF_SIZE_KiB' is exposed to the host through
 *   cm 'caps' structure and is used by the tooling to send properly sized
 *   chunks of data.
 *
 */

#include <arch/arch.h>
#include <drivers/usb/imx_ci_udc.h>
#include <drivers/usb/usbd.h>
#include <inttypes.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <stdio.h>
#include <string.h>

#include "imx_ci_udc_private.h"

/* Misc defines */
#define IMX_CI_UDC_NO_OF_EPS         8
#define IMX_CI_UDC_NO_OF_DESCRIPTORS (1 + (CONFIG_CM_BUF_SIZE_KiB / 20))
#define IMX_CI_UDC_SZ_64B            0x40

#define EP_IS_IN(ep)                 ((ep) & 1)
#define EP_USB_NUMBER(ep)            ((ep) >> 1)

/* bEndpointAddress in an endpoint descriptor */
#define USB_EP_ADDR_NUMBER_MASK      0x0f
#define USB_EP_ADDR_DIR_IN           0x80

/* No data structure seen by the controller should span a 4k page boundary */
static struct imx_ci_udc_transfer_head dtds[IMX_CI_UDC_NO_OF_DESCRIPTORS] __section(".no_init")
    __aligned(4096);
static struct imx_ci_udc_queue_head dqhs[IMX_CI_UDC_NO_OF_EPS * 2] __section(".no_init")
    __aligned(4096);
static uint8_t align_buffer[DTD_PAGE_SIZE] __aligned(4096);
static uintptr_t xfer_bfr;
static size_t xfer_length;
static struct imx_ci_udc_transfer_head *xfer_head;
static usb_ep_t xfer_ep;
static uintptr_t imx_ci_udc_base;

/* The bit of an endpoint in ENDPTPRIME, ENDPTFLUSH, ENDPTSTAT and ENDPTCOMPLETE */
static uint32_t ep_bit(usb_ep_t ep)
{
    if (EP_IS_IN(ep))
        return ENDPT_TX(EP_USB_NUMBER(ep));
    else
        return ENDPT_RX(EP_USB_NUMBER(ep));
}

static void imx_ci_udc_reset_queues(void)
{
    for (int i = 0; i < IMX_CI_UDC_NO_OF_EPS * 2; i++) {
        dqhs[i].caps = 0;
        dqhs[i].next = DTD_NEXT_T;
        dqhs[i].token = 0;
        dqhs[i].current_dtd = 0;
    }

    xfer_bfr = 0;
    xfer_length = 0;
    xfer_ep = 0;
    xfer_head = NULL;
}

static void imx_ci_udc_reset(void)
{
    mmio_write_32(imx_ci_udc_base + CI_USBSTS, USBSTS_ALL);
    mmio_write_32(imx_ci_udc_base + CI_ENDPTSETUPSTAT, ENDPTSETUPSTAT_ALL);
    mmio_write_32(imx_ci_udc_base + CI_ENDPTCOMPLETE, ENDPT_ALL);

    while (mmio_read_32(imx_ci_udc_base + CI_ENDPTPRIME)) {
    };

    mmio_write_32(imx_ci_udc_base + CI_ENDPTFLUSH, ENDPT_ALL);

    /* Wait for port to come out of reset */
    while (mmio_read_32(imx_ci_udc_base + CI_PORTSC1) & PORTSC1_PR) {
    };
}

static void imx_ci_udc_config_ep(usb_ep_t ep, uint32_t size, uint32_t flags)
{
    struct imx_ci_udc_queue_head *qh = &dqhs[ep];

    qh->caps = DQH_CAP_ZLT | DQH_CAP_MAX_PACKET_LEN(size) | flags;

    arch_clean_cache_range((uintptr_t)qh, sizeof(*qh));
}

static int imx_ci_udc_irq_process(void)
{
    uint32_t tmp;
    uint32_t sts = mmio_read_32(imx_ci_udc_base + CI_USBSTS);
    uint32_t epc = mmio_read_32(imx_ci_udc_base + CI_ENDPTCOMPLETE);

    mmio_write_32(imx_ci_udc_base + CI_USBSTS, USBSTS_ALL);
    mmio_write_32(imx_ci_udc_base + CI_ENDPTCOMPLETE, epc);

    /* Check for USB reset */
    if (sts & USBSTS_URI) {
        mmio_write_32(imx_ci_udc_base + CI_ENDPTFLUSH, ENDPT_ALL);

        while (mmio_read_32(imx_ci_udc_base + CI_ENDPTFLUSH) == ENDPT_ALL) {
        };

        tmp = mmio_read_32(imx_ci_udc_base + CI_ENDPTSETUPSTAT);
        mmio_write_32(imx_ci_udc_base + CI_ENDPTSETUPSTAT, tmp);

        tmp = mmio_read_32(imx_ci_udc_base + CI_ENDPTCOMPLETE);
        mmio_write_32(imx_ci_udc_base + CI_ENDPTCOMPLETE, tmp);

        mmio_write_32(imx_ci_udc_base + CI_DEVICEADDR, 0);

        LOG_DBG("Bus reset");
    }

    if (sts & USBSTS_UEI) {
        mmio_write_32(imx_ci_udc_base + CI_USBSTS, USBSTS_UEI);
        LOG_ERR("IMX_CI_UDC: Error %x", sts);
    }

    return PB_OK;
}

/**
 * Note: This function will accept buffers of any alignment but for optimal
 * performace all buffers should be 4k aligned.
 */
static int imx_ci_udc_xfer_start(usb_ep_t ep, void *bfr_, size_t length)
{
    struct imx_ci_udc_queue_head *qh = &dqhs[ep];
    struct imx_ci_udc_transfer_head *dtd = dtds;
    uint32_t epreg = 0;
    size_t bytes_to_tx = length;
    size_t align_length = 0;
    uintptr_t bfr = (uintptr_t)bfr_;
    uintptr_t buf_ptr = bfr;

    if (ep >= USB_EP_END)
        return -PB_ERR_PARAM;

    if (bytes_to_tx == 0) {
        dtd->next = DTD_NEXT_T;
        dtd->token = DTD_TOKEN_STATUS_ACTIVE | DTD_TOKEN_IOC;
    } else if (bfr) {
        arch_clean_cache_range(bfr, length);
    }

    /* Check and align input buffer if needed */
    if ((length > 0) && (bfr & DTD_PAGE_OFFSET_MASK)) {
        align_length = (DTD_PAGE_SIZE - (bfr & DTD_PAGE_OFFSET_MASK)) & DTD_PAGE_OFFSET_MASK;
        align_length = (length > DTD_PAGE_SIZE) ? align_length : length;
        /* LOG_DBG("Aligning buffer <%p> (%zu bytes)", (void *) bfr, align_length); */
        /* If this is an IN transfer we need to fill the align buffer */
        if (EP_IS_IN(ep)) {
            memcpy(align_buffer, (void *)bfr, align_length);
        }
        arch_clean_cache_range((uintptr_t)align_buffer, align_length);
    }

    while (bytes_to_tx) {
        dtd->next = DTD_NEXT_T;
        dtd->token = DTD_TOKEN_STATUS_ACTIVE;

        if (bytes_to_tx > DTD_MAX_BYTES) {
            dtd->token |= DTD_TOKEN_TOTAL_BYTES(DTD_MAX_BYTES);
        } else {
            dtd->token |= DTD_TOKEN_TOTAL_BYTES(bytes_to_tx);
        }

        for (int n = 0; n < DTD_NO_OF_PAGES; n++) {
            dtd->page[n] = (uint32_t)buf_ptr;

            if (align_length) {
                /* If we need to align the input buffer, it will be at most
                 * one 4k page */
                dtd->page[n] = (uint32_t)(uintptr_t)align_buffer;
                bytes_to_tx -= align_length;
                buf_ptr += align_length;
            } else if (bytes_to_tx >= DTD_PAGE_SIZE) {
                buf_ptr += DTD_PAGE_SIZE;
                bytes_to_tx -= DTD_PAGE_SIZE;
            } else {
                buf_ptr += bytes_to_tx;
                bytes_to_tx = 0;
            }

            if (dtd->page[n] & DTD_PAGE_OFFSET_MASK)
                return -PB_ERR_ALIGN;

            if (bytes_to_tx == 0) {
                buf_ptr = 0;
                dtd->token |= DTD_TOKEN_IOC;
                break;
            }
        }

        if (bytes_to_tx) {
            struct imx_ci_udc_transfer_head *dtd_prev = dtd;
            dtd++;
            if (dtd == &dtds[IMX_CI_UDC_NO_OF_DESCRIPTORS])
                return -PB_ERR_MEM;
            dtd_prev->next = (uint32_t)(uintptr_t)dtd;
        }
    }

    /* The controller writes the dQH overlay, so a cached copy of it can be
     * stale and the clean below would write an old, active overlay back over
     * the controller's, which it then resumes instead of fetching 'next'.
     * Drop the cached copy first, and clear the overlay's active and halted
     * bits before priming (RM 54.4.6.6.3, an empty list). */
    arch_invalidate_cache_range((uintptr_t)qh, sizeof(*qh));
    qh->next = (uint32_t)(uintptr_t)dtds;
    qh->token = 0;

    arch_clean_cache_range((uintptr_t)qh, sizeof(*qh));
    arch_clean_cache_range((uintptr_t)dtds, sizeof(dtds[0]) * IMX_CI_UDC_NO_OF_DESCRIPTORS);

    epreg = ep_bit(ep);

    mmio_write_32(imx_ci_udc_base + CI_ENDPTPRIME, epreg);

    while (mmio_read_32(imx_ci_udc_base + CI_ENDPTPRIME) & epreg) {
    };

    xfer_bfr = (uintptr_t)bfr;
    xfer_length = length;
    xfer_head = dtd;
    xfer_ep = ep;

    return PB_OK;
}

static int imx_ci_udc_xfer_complete(usb_ep_t ep)
{
    int rc;

    if (ep >= USB_EP_END)
        return -PB_ERR_PARAM;

    rc = imx_ci_udc_irq_process();

    if (rc != PB_OK)
        return rc;

    if (xfer_head == NULL)
        return -PB_ERR;

    arch_invalidate_cache_range((uintptr_t)xfer_head, sizeof(*xfer_head));

    // TODO: We should look at the error bit's as well

    if (xfer_head->token & DTD_TOKEN_STATUS_ACTIVE)
        return -PB_ERR_AGAIN;

    /* Check for un-aligned buffers */
    if (xfer_length && (xfer_bfr & DTD_PAGE_OFFSET_MASK)) {
        size_t align_length = (DTD_PAGE_SIZE - (xfer_bfr & DTD_PAGE_OFFSET_MASK)) &
                              DTD_PAGE_OFFSET_MASK;
        align_length = (xfer_length > DTD_PAGE_SIZE) ? align_length : xfer_length;

        /* If this was an out transfer we should copy data from the
         * alignment buffer */
        if (!EP_IS_IN(ep)) {
            /* LOG_DBG("Align: copying %zu bytes to %p", align_length,
                    (void *) xfer_bfr); */
            arch_invalidate_cache_range((uintptr_t)align_buffer, align_length);
            memcpy((void *)xfer_bfr, align_buffer, align_length);
        }
    }

    if (!EP_IS_IN(ep) && xfer_bfr && xfer_length) {
        /* Output from host, invalidate cache*/
        arch_invalidate_cache_range(xfer_bfr, xfer_length);
    }

    return PB_OK;
}

static void imx_ci_udc_flush_ep(usb_ep_t ep)
{
    if (ep >= USB_EP_END)
        return;

    uint32_t flush_cmd = ENDPT_TX(EP_USB_NUMBER(ep)) | ENDPT_RX(EP_USB_NUMBER(ep));

    mmio_write_32(imx_ci_udc_base + CI_ENDPTFLUSH, flush_cmd);

    while ((mmio_read_32(imx_ci_udc_base + CI_ENDPTFLUSH) & flush_cmd) != 0) {
    };
}

static int imx_ci_udc_set_configuration(const struct usb_endpoint_descriptor *eps, size_t no_of_eps)
{
    enum usb_ep_type ep_type;
    usb_ep_t ep;
    uint16_t max_pkt_sz;
    uintptr_t epctrl;
    uint32_t ep_type_val;
    uint32_t ep_reg;

    for (size_t n = 0; n < no_of_eps; n++) {
        ep = (eps[n].bEndpointAddress & USB_EP_ADDR_NUMBER_MASK) * 2;
        if (eps[n].bEndpointAddress & USB_EP_ADDR_DIR_IN)
            ep++;

        ep_type = eps[n].bmAttributes + 1;
        max_pkt_sz = eps[n].wMaxPacketSize;
        switch (ep_type) {
        case USB_EP_TYPE_CONTROL:
            ep_type_val = ENDPTCTRL_TYPE_CONTROL;
            break;
        case USB_EP_TYPE_ISO:
            ep_type_val = ENDPTCTRL_TYPE_ISO;
            break;
        case USB_EP_TYPE_BULK:
            ep_type_val = ENDPTCTRL_TYPE_BULK;
            break;
        case USB_EP_TYPE_INTR:
            ep_type_val = ENDPTCTRL_TYPE_INTR;
            break;
        default:
            return -PB_ERR_PARAM;
        };

        /* Endpoint 0 is always the control endpoint, it has no configuration */
        if (ep < USB_EP1_OUT || ep >= USB_EP_END)
            return -PB_ERR_PARAM;

        epctrl = CI_ENDPTCTRL(EP_USB_NUMBER(ep));

        /* Keep the other direction, and make it bulk if it's unused: a
         * disabled direction left as control type makes the data PID
         * tracking of the enabled one undefined. */
        ep_reg = mmio_read_32(imx_ci_udc_base + epctrl);

        if (EP_IS_IN(ep)) {
            ep_reg &= ENDPTCTRL_RX_MASK;
            ep_reg |= ENDPTCTRL_TXE | ENDPTCTRL_TXT(ep_type_val) | ENDPTCTRL_TXR;

            if (!(ep_reg & ENDPTCTRL_RXE))
                ep_reg = (ep_reg & ~ENDPTCTRL_RXT_MASK) | ENDPTCTRL_RXT(ENDPTCTRL_TYPE_BULK);
        } else {
            ep_reg &= ENDPTCTRL_TX_MASK;
            ep_reg |= ENDPTCTRL_RXE | ENDPTCTRL_RXT(ep_type_val) | ENDPTCTRL_RXR;

            if (!(ep_reg & ENDPTCTRL_TXE))
                ep_reg = (ep_reg & ~ENDPTCTRL_TXT_MASK) | ENDPTCTRL_TXT(ENDPTCTRL_TYPE_BULK);
        }

        LOG_DBG("EP config: reg=0x%lx, val=0x%x", epctrl, ep_reg);
        mmio_write_32(imx_ci_udc_base + epctrl, ep_reg);
        imx_ci_udc_config_ep(ep, max_pkt_sz, 0);
    }

    return PB_OK;
}

static int imx_ci_udc_poll_setup_pkt(struct usb_setup_packet *pkt)
{
    /* EP0 Process setup packets */
    if (mmio_read_32(imx_ci_udc_base + CI_ENDPTSETUPSTAT) & ENDPTSETUPSTAT_EP(0)) {
        struct imx_ci_udc_queue_head *qh = &dqhs[USB_EP0_OUT];

        do {
            mmio_clrsetbits_32(imx_ci_udc_base + CI_USBCMD, 0, USBCMD_SUTW);
            arch_invalidate_cache_range((uintptr_t)qh->setup, sizeof(*qh->setup));
            memcpy(pkt, qh->setup, sizeof(struct usb_setup_packet));
        } while (!(mmio_read_32(imx_ci_udc_base + CI_USBCMD) & USBCMD_SUTW));

        mmio_write_32(imx_ci_udc_base + CI_ENDPTSETUPSTAT, ENDPTSETUPSTAT_EP(0));
        mmio_clrsetbits_32(imx_ci_udc_base + CI_USBCMD, USBCMD_SUTW, 0);

        /* Do we really need to flush transmitt buffer here? */
        mmio_write_32(imx_ci_udc_base + CI_ENDPTFLUSH, ENDPT_TX(0) | ENDPT_RX(0));

        while (mmio_read_32(imx_ci_udc_base + CI_ENDPTSETUPSTAT) & ENDPTSETUPSTAT_EP(0)) {
        };

        return PB_OK;
    }

    imx_ci_udc_irq_process();

    return -PB_ERR_AGAIN;
}

static int imx_ci_udc_init_internal(void)
{
    LOG_DBG("Init base: 0x%" PRIxPTR, imx_ci_udc_base);

    mmio_clrsetbits_32(imx_ci_udc_base + CI_USBCMD, 0, USBCMD_RST);

    while (mmio_read_32(imx_ci_udc_base + CI_USBCMD) & USBCMD_RST) {
    };

    imx_ci_udc_reset_queues();

    mmio_write_32(imx_ci_udc_base + CI_DEVICEADDR, 0);

    imx_ci_udc_config_ep(USB_EP0_IN, IMX_CI_UDC_SZ_64B, 0);
    imx_ci_udc_config_ep(USB_EP0_OUT, IMX_CI_UDC_SZ_64B, DQH_CAP_IOS);

    /* Program QH top */
    mmio_write_32(imx_ci_udc_base + CI_ENDPTLISTADDR, (uint32_t)(uintptr_t)dqhs);

    /* Enable USB */
    mmio_write_32(imx_ci_udc_base + CI_USBMODE, USBMODE_SDIS | USBMODE_SLOM | USBMODE_CM_DEVICE);

    /* Interrupt threshold 0 (immediate), the driver polls */
    mmio_write_32(imx_ci_udc_base + CI_USBCMD, USBCMD_ITC(0) | USBCMD_RS);

    imx_ci_udc_reset();

    return PB_OK;
}

static int imx_ci_udc_set_address(uint16_t addr)
{
    mmio_write_32(imx_ci_udc_base + CI_DEVICEADDR, DEVICEADDR_USBADR(addr) | DEVICEADDR_USBADRA);
    return PB_OK;
}

static int imx_ci_udc_stop(void)
{
    mmio_clrsetbits_32(imx_ci_udc_base + CI_USBCMD, USBCMD_RS, 0);
    imx_ci_udc_reset_queues();
    imx_ci_udc_reset();
    mmio_write_32(imx_ci_udc_base + CI_DEVICEADDR, 0);

    mmio_clrsetbits_32(imx_ci_udc_base + CI_USBCMD, 0, USBCMD_RST);

    while (mmio_read_32(imx_ci_udc_base + CI_USBCMD) & USBCMD_RST) {
    };

    return PB_OK;
}

static int imx_ci_udc_xfer_zlp(usb_ep_t ep)
{
    int rc;

    rc = imx_ci_udc_xfer_start(ep, 0, 0);

    if (rc != PB_OK)
        return rc;

    do {
        rc = imx_ci_udc_xfer_complete(ep);
    } while (rc == -PB_ERR_AGAIN);

    return rc;
}

int imx_ci_udc_init(uintptr_t base)
{
    imx_ci_udc_base = base;

    static const struct usbd_hal_ops ops = {
        .init = imx_ci_udc_init_internal,
        .stop = imx_ci_udc_stop,
        .xfer_start = imx_ci_udc_xfer_start,
        .xfer_complete = imx_ci_udc_xfer_complete,
        .xfer_cancel = imx_ci_udc_flush_ep,
        .poll_setup_pkt = imx_ci_udc_poll_setup_pkt,
        .set_configuration = imx_ci_udc_set_configuration,
        .set_address = imx_ci_udc_set_address,
        .ep0_xfer_zlp = imx_ci_udc_xfer_zlp,
    };

    return usbd_init_hal_ops(&ops);
}
