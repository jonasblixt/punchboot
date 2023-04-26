/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
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

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <arch/arch.h>
#include <pb/pb.h>
#include <pb/mmio.h>
#include <drivers/usb/usbd.h>
#include <drivers/usb/imx_ehci.h>

/* Registers */
#define EHCI_DCIVERSION 0x120

#define EHCI_USBSTS 0x144
#define USBSTS_URI BIT(6)

#define EHCI_ENDPTCOMPLETE 0x1bc

#define EHCI_USB_MODE 0x1a8
#define USB_MODE_SDIS BIT(4)
#define USB_MODE_SLOM BIT(3)
#define USB_MODE_ES BIT(2)
#define USB_MODE_CM_DEVICE (2)

#define EHCI_ENDPTLISTADDR 0x158

#define EHCI_ENDPTSETUPSTAT 0x1ac

#define EHCI_CMD 0x140
#define CMD_RS BIT(0)
#define CMD_RST BIT(1)
#define CMD_SUTW BIT(13)
#define CMD_ITC(x) ((uint16_t) (x << 16) & 0xFFFF)

#define EHCI_ENDPTPRIME 0x1b0
#define EHCI_PORTSC1 0x184
#define EHCI_ENDPTFLUSH 0x1b4
#define EHCI_ENDPTNAK 0x178

#define EHCI_DEVICEADDR 0x154
#define DEVICEADDR_ADDR(x) ((x & 0x7f) << 25)
#define DEVICEADDR_USBADRA BIT(24)

#define EHCI_ENDPTCTRL1 0x1c4
#define EHCI_ENDPTCTRL2 0x1c8
#define EHCI_ENDPTCTRL3 0x1cc
#define EHCI_ENDPTCTRL4 0x1d0
#define EHCI_ENDPTCTRL5 0x1d4
#define EHCI_ENDPTCTRL6 0x1d8
#define EHCI_ENDPTCTRL7 0x1dc

#define EHCI_BURSTSIZE 0x160
#define EHCI_SBUSCFG 0x90
#define EHCI_ENDPTSTAT 0x1b8
#define EHCI_ENDPTNAKEN 0x17C

#define EHCI_EP0_OUT (1 << 0)
#define EHCI_EP0_IN  (1 << 16)
#define EHCI_EP1_OUT (1 << 1)
#define EHCI_EP1_IN  (1 << 17)
#define EHCI_EP2_OUT (1 << 2)
#define EHCI_EP2_IN  (1 << 18)
#define EHCI_EP3_OUT (1 << 3)
#define EHCI_EP3_IN  (1 << 19)

/* Misc defines */
#define EHCI_NO_OF_EPS 8
#define EHCI_NO_OF_DESCRIPTORS (1 + (CONFIG_CM_BUF_SIZE_KiB / 20))
#define EHCI_SZ_512B 0x200
#define EHCI_SZ_64B 0x40
#define EHCI_PAGE_SZ 4096

/* Transfer structures and bits */

#define EPQH_CAP_MULT(x) ((x & 0x03) << 30)
#define EPQH_CAP_ZLT     BIT(29)
#define EPQH_CAP_MAXLEN(x) ((x & 0x7ff) << 16)
#define EPQH_CAP_IOS     BIT(15)

struct ehci_transfer_head
{
    uint32_t next;
    uint32_t token;
    uint32_t page[5];
    uint32_t pad;
} __attribute__((packed));

struct ehci_queue_head
{
    uint32_t caps;
    uint32_t current_dtd;
    uint32_t next;
    uint32_t token;
    uint32_t page[5];
    uint32_t __reserved;
    uint32_t setup[2];
    uint32_t padding[4];
} __attribute__((packed));

/* No data structure seen by the controller should span a 4k page boundary */
static struct ehci_transfer_head dtds[EHCI_NO_OF_DESCRIPTORS] __section(".no_init") __aligned(4096);
static struct ehci_queue_head    dqhs[EHCI_NO_OF_EPS*2] __section(".no_init") __aligned(4096);
static uint8_t align_buffer[4096] __aligned(4096);
static uintptr_t xfer_bfr;
static size_t xfer_length;
static struct ehci_transfer_head *xfer_head;
static usb_ep_t xfer_ep;
static uintptr_t ehci_base;

static void ehci_reset_queues(void)
{
    for (int i = 0; i < EHCI_NO_OF_EPS*2; i++) {
        dqhs[i].caps = 0;
        dqhs[i].next = 0xDEAD0001;
        dqhs[i].token = 0;
        dqhs[i].current_dtd = 0;
    }

    xfer_bfr = 0;
    xfer_length = 0;
    xfer_ep = 0;
    xfer_head = NULL;
}

static void ehci_reset(void)
{
    mmio_write_32(ehci_base + EHCI_USBSTS, 0xFFFFFFFF);
    mmio_write_32(ehci_base + EHCI_ENDPTSETUPSTAT, 0xFFFF);
    mmio_write_32(ehci_base + EHCI_ENDPTCOMPLETE, (0xff << 16)  | 0xff);

    while (mmio_read_32(ehci_base + EHCI_ENDPTPRIME))
        {};

    mmio_write_32(ehci_base + EHCI_ENDPTFLUSH, 0xFFFFFFFF);

    /* Wait for port to come out of reset */
    while (mmio_read_32(ehci_base + EHCI_PORTSC1) & (1 << 8))
        {};
}

static int ehci_config_ep(usb_ep_t ep, uint32_t size, uint32_t flags)
{
    struct ehci_queue_head *qh = &dqhs[ep];

    // TODO: Define bits
    qh->caps = (1 << 29) | (size << 16) | flags;

    arch_clean_cache_range((uintptr_t) qh, sizeof(*qh));

    return PB_OK;
}

static int ehci_irq_process(void)
{
    uint32_t tmp;
    uint32_t sts = mmio_read_32(ehci_base + EHCI_USBSTS);
    uint32_t epc = mmio_read_32(ehci_base + EHCI_ENDPTCOMPLETE);

    mmio_write_32(ehci_base + EHCI_USBSTS, 0xFFFFFFFF);
    mmio_write_32(ehci_base + EHCI_ENDPTCOMPLETE, epc);

    /* Check for USB reset */
    if (sts & USBSTS_URI) {
        mmio_write_32(ehci_base + EHCI_ENDPTFLUSH, 0x00FF00FF);

        while (mmio_read_32(ehci_base + EHCI_ENDPTFLUSH) == 0x00FF00FF)
            {};

        tmp = mmio_read_32(ehci_base + EHCI_ENDPTSETUPSTAT);
        mmio_write_32(ehci_base + EHCI_ENDPTSETUPSTAT, tmp);

        tmp = mmio_read_32(ehci_base + EHCI_ENDPTCOMPLETE);
        mmio_write_32(ehci_base + EHCI_ENDPTCOMPLETE, tmp);

        mmio_write_32(ehci_base + EHCI_DEVICEADDR, 0);

        LOG_DBG("Bus reset");
    }

    if (sts & BIT(1)) {
        mmio_write_32(ehci_base + EHCI_USBSTS, 2);
        LOG_ERR("EHCI: Error %x", sts);
    }

    return PB_OK;
}

/**
 * Note: This function will accept buffers of any alignment but for optimal
 * performace all buffers should be 4k aligned.
 */
static int ehci_xfer_start(usb_ep_t ep, void *bfr_, size_t length)
{
    struct ehci_queue_head *qh = &dqhs[ep];
    struct ehci_transfer_head *dtd = dtds;
    uint32_t epreg = 0;
    size_t bytes_to_tx = length;
    size_t align_length = 0;
    uintptr_t bfr = (uintptr_t) bfr_;
    uintptr_t buf_ptr = bfr;

    if (ep >= USB_EP_END)
        return -PB_ERR_PARAM;

    if (bytes_to_tx == 0) {
        dtd->next = 0xDEAD0001;
        dtd->token = 0x80 | (1 << 15);
    } else if (bfr) {
        arch_clean_cache_range(bfr, length);
    }

    /* Check and align input buffer if needed */
    if ((length > 0) && (bfr & 0xfff)) {
        align_length = (4096 - (bfr & 0xfff)) & 0xfff;
        align_length = (length > 4096)?align_length:length;
        /* LOG_DBG("Aligning buffer <%p> (%zu bytes)", (void *) bfr, align_length); */
        /* If this is an IN transfer we need to fill the align buffer */
        if ((ep & 1) == 1) {
            memcpy(align_buffer, (void *) bfr, align_length);
        }
        arch_clean_cache_range((uintptr_t) align_buffer, align_length);
    }

    while (bytes_to_tx) {
        dtd->next = 0xDEAD0001;
        dtd->token = 0x80;

        if (bytes_to_tx > (5*EHCI_PAGE_SZ)) {
            dtd->token |= ((5*EHCI_PAGE_SZ) << 16);
        } else {
            dtd->token |= (bytes_to_tx << 16);
        }

        for (int n = 0; n < 5; n++) {
            dtd->page[n] = (uint32_t) buf_ptr;

            if (align_length) {
                /* If we need to align the input buffer, it will be at most
                 * one 4k page */
                dtd->page[n] = (uint32_t)(uintptr_t) align_buffer;
                bytes_to_tx -= align_length;
                buf_ptr += align_length;
            } else if (bytes_to_tx >= EHCI_PAGE_SZ) {
                buf_ptr += EHCI_PAGE_SZ;
                bytes_to_tx -= EHCI_PAGE_SZ;
            } else {
                buf_ptr += bytes_to_tx;
                bytes_to_tx = 0;
            }

            if (dtd->page[n] & 0xfff)
                return -PB_ERR_ALIGN;

            if (bytes_to_tx == 0) {
                buf_ptr = 0;
                dtd->token |= (1 << 15);
                break;
            }
        }

        if (bytes_to_tx) {
            struct ehci_transfer_head *dtd_prev = dtd;
            dtd++;
            if (dtd == &dtds[EHCI_NO_OF_DESCRIPTORS])
                return -PB_ERR_MEM;
            dtd_prev->next = (uint32_t)(uintptr_t) dtd;
        }
    }

    qh->next = (uint32_t) (uintptr_t) dtds;

    arch_clean_cache_range((uintptr_t) qh, sizeof(*qh));
    arch_clean_cache_range((uintptr_t) dtds,
                           sizeof(dtds[0])*EHCI_NO_OF_DESCRIPTORS);

    if (ep & 1) {
        epreg = (1 << ((ep-1)/2 + 16));
    } else {
        epreg = (1 << (ep/2));
    }

    mmio_write_32(ehci_base + EHCI_ENDPTPRIME, epreg);

    while (mmio_read_32(ehci_base + EHCI_ENDPTPRIME) & epreg)
        {};

    xfer_bfr = (uintptr_t) bfr;
    xfer_length = length;
    xfer_head = dtd;
    xfer_ep = ep;

    return PB_OK;
}

static int ehci_xfer_complete(usb_ep_t ep)
{
    int rc;

    if (ep >= USB_EP_END)
        return -PB_ERR_PARAM;

    rc = ehci_irq_process();

    if (rc != PB_OK)
        return rc;

    if (xfer_head == NULL)
        return -PB_ERR;

    arch_invalidate_cache_range((uintptr_t) xfer_head, sizeof(*xfer_head));

   // TODO: We should look at the error bit's as well
   // TODO: Define bits

    if (xfer_head->token & 0x80)
        return -PB_ERR_AGAIN;

   /* Check for un-aligned buffers */
   if (xfer_length && (xfer_bfr & 0xfff)) {
        size_t align_length = (4096 - (xfer_bfr & 0xfff)) & 0xfff;
        align_length = (xfer_length > 4096)?align_length:xfer_length;

        /* If this was an out transfer we should copy data from the
         * alignment buffer */
        if ((ep & 1) == 0) {
            /* LOG_DBG("Align: copying %zu bytes to %p", align_length,
                    (void *) xfer_bfr); */
            arch_invalidate_cache_range((uintptr_t) align_buffer, align_length);
            memcpy((void *) xfer_bfr, align_buffer, align_length);
        }
   }

    if (!(ep & 1) && xfer_bfr && xfer_length) {
        /* Output from host, invalidate cache*/
        arch_invalidate_cache_range(xfer_bfr, xfer_length);
    }

    return PB_OK;
}

static void ehci_flush_ep(usb_ep_t ep)
{
    uint8_t idx;

    switch (ep) {
    case USB_EP0_IN:
    case USB_EP0_OUT:
        idx = 0;
    break;
    case USB_EP1_IN:
    case USB_EP1_OUT:
        idx = 1;
    break;
    case USB_EP2_IN:
    case USB_EP2_OUT:
        idx = 2;
    break;
    case USB_EP3_IN:
    case USB_EP3_OUT:
        idx = 3;
    break;
    case USB_EP4_IN:
    case USB_EP4_OUT:
        idx = 4;
    break;
    case USB_EP5_IN:
    case USB_EP5_OUT:
        idx = 5;
    break;
    case USB_EP6_IN:
    case USB_EP6_OUT:
        idx = 6;
    break;
    case USB_EP7_IN:
    case USB_EP7_OUT:
        idx = 7;
    break;
        default:
            return ;
    };

    uint32_t flush_cmd = (1 << (16 + idx)) | (1 << idx);

    mmio_write_32(ehci_base + EHCI_ENDPTFLUSH, flush_cmd);

    while ((mmio_read_32(ehci_base + EHCI_ENDPTFLUSH) & flush_cmd) != 0)
        {};
}

static int ehci_configure_ep(usb_ep_t ep,
                             enum usb_ep_type ep_type,
                             size_t pkt_sz)
{
    bool ep_in = false;
    uintptr_t epctrl = 0;
    uint8_t ep_type_val;
    uint32_t ep_reg = 0;

    switch (ep_type) {
        case USB_EP_TYPE_CONTROL:
            ep_type_val = 0;
        break;
        case USB_EP_TYPE_ISO:
            ep_type_val = 1;
        break;
        case USB_EP_TYPE_BULK:
            ep_type_val = 2;
        break;
        case USB_EP_TYPE_INTR:
            ep_type_val = 3;
        break;
        default:
            return -PB_ERR_PARAM;
    };

    switch (ep) {
    case USB_EP1_IN:
        ep_in = true;
        /* Fallthrough */
    case USB_EP1_OUT:
        epctrl = EHCI_ENDPTCTRL1;
    break;
    case USB_EP2_IN:
        ep_in = true;
        /* Fallthrough */
    case USB_EP2_OUT:
        epctrl = EHCI_ENDPTCTRL2;
    break;
    case USB_EP3_IN:
        ep_in = true;
        /* Fallthrough */
    case USB_EP3_OUT:
        epctrl = EHCI_ENDPTCTRL3;
    break;
    case USB_EP4_IN:
        ep_in = true;
        /* Fallthrough */
    case USB_EP4_OUT:
        epctrl = EHCI_ENDPTCTRL4;
    break;
    case USB_EP5_IN:
        ep_in = true;
        /* Fallthrough */
    case USB_EP5_OUT:
        epctrl = EHCI_ENDPTCTRL5;
    break;
    case USB_EP6_IN:
        ep_in = true;
        /* Fallthrough */
    case USB_EP6_OUT:
        epctrl = EHCI_ENDPTCTRL6;
    break;
    case USB_EP7_IN:
        ep_in = true;
        /* Fallthrough */
    case USB_EP7_OUT:
        epctrl = EHCI_ENDPTCTRL7;
    break;
        default:
            return -PB_ERR_PARAM;
    };

    if (ep_in) {
        ep_reg = (1 << 23) | (ep_type_val << 18) | (1 << 6);
    } else {
        ep_reg = (1 << 7) | (ep_type_val << 2) | (1 << 6);
    }

    LOG_DBG("EP config: reg=0x%lx, val=0x%x", epctrl, ep_reg);
    mmio_write_32(ehci_base + epctrl, ep_reg);
    ehci_config_ep(ep, pkt_sz, 0);

    return PB_OK;
}

static int ehci_poll_setup_pkt(struct usb_setup_packet *pkt)
{

    /* EP0 Process setup packets */
    if  (mmio_read_32(ehci_base + EHCI_ENDPTSETUPSTAT) & 1) {
        struct ehci_queue_head *qh = &dqhs[USB_EP0_OUT];

        do {
            mmio_clrsetbits_32(ehci_base + EHCI_CMD, 0, CMD_SUTW);
            arch_invalidate_cache_range((uintptr_t) qh->setup,
                                                    sizeof(*qh->setup));
            memcpy(pkt, qh->setup, sizeof(struct usb_setup_packet));
        } while (!(mmio_read_32(ehci_base + EHCI_CMD) & CMD_SUTW));

        mmio_write_32(ehci_base + EHCI_ENDPTSETUPSTAT, 1);
        mmio_clrsetbits_32(ehci_base + EHCI_CMD, CMD_SUTW, 0);

        /* Do we really need to flush transmitt buffer here? */
        // TODO: Bits
        mmio_write_32(ehci_base + EHCI_ENDPTFLUSH, (1<< 16) | 1);

        while (mmio_read_32(ehci_base + EHCI_ENDPTSETUPSTAT) & 1)
            {};

        return PB_OK;
    }

    ehci_irq_process();

    return -PB_ERR_AGAIN;
}

static int ehci_init(void)
{
    LOG_DBG("Init base: 0x%"PRIxPTR, ehci_base);

    mmio_clrsetbits_32(ehci_base + EHCI_CMD, 0, CMD_RST);

    while (mmio_read_32(ehci_base + EHCI_CMD) & CMD_RST)
        {};

    ehci_reset_queues();

    mmio_write_32(ehci_base + EHCI_DEVICEADDR, 0);

    ehci_config_ep(USB_EP0_IN,  EHCI_SZ_64B,  0);
    ehci_config_ep(USB_EP0_OUT, EHCI_SZ_64B,  EPQH_CAP_IOS);

    /* Program QH top */
    mmio_write_32(ehci_base + EHCI_ENDPTLISTADDR, (uint32_t)(uintptr_t) dqhs);

    /* Enable USB */
    mmio_write_32(ehci_base + EHCI_USB_MODE, USB_MODE_SDIS |
                                             USB_MODE_SLOM |
                                             USB_MODE_CM_DEVICE);

    mmio_write_32(ehci_base + EHCI_CMD, CMD_ITC(0x40) | CMD_RS);

    ehci_reset();

    return PB_OK;
}

static int ehci_set_address(uint16_t addr)
{
    mmio_write_32(ehci_base + EHCI_DEVICEADDR, DEVICEADDR_ADDR(addr) | DEVICEADDR_USBADRA);
    return PB_OK;
}

static int ehci_stop(void)
{
    ehci_reset_queues();
    ehci_reset();
    mmio_write_32(ehci_base + EHCI_DEVICEADDR, 0);

    mmio_clrsetbits_32(ehci_base + EHCI_CMD, 0, CMD_RST);

    while (mmio_read_32(ehci_base + EHCI_CMD) & CMD_RST)
        {};

    return PB_OK;
}

static int ehci_xfer_zlp(usb_ep_t ep)
{
    int rc;

    rc = ehci_xfer_start(ep, 0, 0);

    if (rc != PB_OK)
        return rc;

    do {
        rc = ehci_xfer_complete(ep);
    } while (rc == -PB_ERR_AGAIN);

    return rc;
}

int imx_ehci_init(uintptr_t base)
{
    ehci_base = base;

    static const struct usbd_hal_ops ops = {
        .init = ehci_init,
        .stop = ehci_stop,
        .xfer_start = ehci_xfer_start,
        .xfer_complete = ehci_xfer_complete,
        .xfer_cancel = ehci_flush_ep,
        .poll_setup_pkt = ehci_poll_setup_pkt,
        .configure_ep = ehci_configure_ep,
        .set_address = ehci_set_address,
        .ep0_xfer_zlp = ehci_xfer_zlp,
    };

    return usbd_init_hal_ops(&ops);
}
