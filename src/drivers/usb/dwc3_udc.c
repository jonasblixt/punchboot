/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <arch/arch.h>
#include <drivers/usb/dwc3_udc.h>
#include <drivers/usb/usbd.h>
#include <pb/delay.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <stdio.h>
#include <string.h>

#include "dwc3_udc_private.h"

#define DWC3_EV_BUFFER_SIZE 16
#define DWC3_DEF_TIMEOUT_ms 10

struct dwc3_trb {
    uint32_t bptrl;
    uint32_t bptrh;
    uint32_t ssz;
    uint32_t control;
#define DWC3_TRB_IOC          BIT(11)
#define DWC3_TRB_ISP_IMI      BIT(10)
#define DWC3_TRB_TRBCTL_SHIFT (4)
#define DWC3_TRB_TRBCTL_MASK  (0x3f << DWC3_TRB_TRBCTL_SHIFT)
#define DWC3_TRB_TRBCTL(x)    ((x << DWC3_TRB_TRBCTL_SHIFT) & DWC3_TRB_TRBCTL_MASK)
#define DWC3_TRB_CSP          BIT(3)
#define DWC3_TRB_CHN          BIT(2)
#define DWC3_TRB_LST          BIT(1)
#define DWC3_TRB_HWO          BIT(0)
} __attribute__((packed));

struct dwc3_xfer_status {
    size_t length;
    void *buf;
    bool done;
};

static uint32_t ev_buffer[DWC3_EV_BUFFER_SIZE] __aligned(64) __section(".no_init");
static uint32_t ev_index;
static struct usb_setup_packet setup_pkt __aligned(64) __section(".no_init");
static struct dwc3_trb trbs[USB_EP_END] __aligned(64) __section(".no_init");
static struct dwc3_xfer_status xfer_status[USB_EP_END];
static uintptr_t base;
static uint32_t version;
static uint32_t caps;
static bool setup_pkt_queued;

static int dwc3_command(uint8_t ep, uint32_t cmd, uint32_t p0, uint32_t p1, uint32_t p2)
{
    struct pb_timeout to;

    if (ep >= USB_EP_END)
        return -PB_ERR_PARAM;
    /*
        LOG_DBG("cmd %u, ep %u, p0 %x, p1 %x, p2 %x",
                    cmd, ep, p0, p1, p2);
    */

    uint32_t param0_addr = (DWC3_DEPCMDPAR0(ep));
    uint32_t param1_addr = (DWC3_DEPCMDPAR1(ep));
    uint32_t param2_addr = (DWC3_DEPCMDPAR2(ep));

    mmio_write_32(base + param0_addr, p0);
    mmio_write_32(base + param1_addr, p1);
    mmio_write_32(base + param2_addr, p2);

    mmio_write_32(base + DWC3_DEPCMD(ep), DEPCMD_CMDACT | cmd);

    pb_timeout_init_us(&to, DWC3_DEF_TIMEOUT_ms * 1000);

    do {
        if (pb_timeout_has_expired(&to)) {
            uint32_t ev_status = mmio_read_32(base + DWC3_DEPCMD(ep));
            LOG_ERR("CMD %x, Timeout, status = 0x%x", cmd, (ev_status >> 12) & 0xF);
            return -PB_ERR_TIMEOUT;
        }
    } while (mmio_read_32(base + DWC3_DEPCMD(ep)) & DEPCMD_CMDACT);

    return PB_OK;
}

static int dwc3_config_ep(uint8_t ep, uint32_t sz, uint32_t type)
{
    int err;
    uint8_t fifo_no = 0;

    LOG_INFO("Configuring %s<%i>, sz=%u, type=%u", ep_to_str(ep), ep, sz, type);

    /* For control endpoints the fifo number must be the same for IN and OUT
     * directions.
     *
     * For all other OUT EP's this field must be zero.
     *
     * For now we just assign a TX fifo number that matches the enpoint number
     * if ep > 1 (Not a control EP) and we check the last bit to figure out
     * if it's the IN direction.
     *
     * See Table 11-19 (Command 1: DEPCFG) in RM.
     */
    if ((ep > 1) && ep & 1) {
        fifo_no = ep;
    } else if (ep <= 1) {
        fifo_no = 1;
    }

    err = dwc3_command(ep,
                       CMDTYP_DEPCFG,
                       /* Parameter 0 */
                       DEPCFG_P0_BURST_SZ(1) | DEPCFG_P0_FIFO_NO(fifo_no) | DEPCFG_P0_MPS(sz) |
                           DEPCFG_P0_EP_TYPE(type),
                       /* Parameter 1 */
                       DEPCFG_P1_USB_EP_NO(ep) | DEPCFG_P1_EVT_XFERNOTREADY_EN |
                           DEPCFG_P1_EVT_XFERINPROGRESS_EN | DEPCFG_P1_EVT_XFERCOMPLETE,
                       /* Parameter 2 */
                       0);

    if (err != PB_OK)
        return err;

    /* DEPXFERCFG only uses parameter 0 and it must be set to '1'.
     * See 11.1.2.5.2 (Command 2)
     */
    err = dwc3_command(ep, CMDTYP_DEPXFERCFG, 1, 0, 0);

    if (err != PB_OK)
        return err;

    return PB_OK;
}

static int dwc3_udc_xfer_start(usb_ep_t ep, void *bfr, size_t sz)
{
    struct dwc3_trb *trb = &trbs[ep];
    size_t xfr_sz = sz;

    // TODO: Check current xfer if it's busy
    // If it's busy but the buffer and length are the same -> return PB_OK
    // Maybe the wait_for_completion function should reset the busy flag?

    if ((ep & 1) && bfr) {
        arch_clean_cache_range((uintptr_t)bfr, sz);
    }

    xfer_status[ep].done = false;
    xfer_status[ep].buf = bfr;
    xfer_status[ep].length = xfr_sz;

    trb->bptrh = 0;
    trb->bptrl = (uintptr_t)bfr;
    trb->ssz = xfr_sz;
    trb->control = DWC3_TRB_IOC | DWC3_TRB_LST | DWC3_TRB_HWO;

    switch (ep) {
    case USB_EP0_OUT:
        trb->control |= DWC3_TRB_TRBCTL(2);
        trb->control |= DWC3_TRB_ISP_IMI;
        break;
    case USB_EP0_IN:
        trb->control |= DWC3_TRB_TRBCTL(1);
        trb->control |= DWC3_TRB_ISP_IMI;
        break;
    default:
        trb->control |= DWC3_TRB_TRBCTL(1);
    }

    // LOG_DBG("trx %p EP%u %s, %p, sz %zu bytes", trb, (ep >> 1), (ep & 1 ? "IN" : "OUT"), bfr,
    // sz);
    arch_clean_cache_range((uintptr_t)trb, sizeof(*trb));

    return dwc3_command(ep, CMDTYP_DEPSTRTXFER, 0, (uintptr_t)trb, 0);
}

static void dwc3_udc_xfer_cancel(usb_ep_t ep)
{
    dwc3_command(ep, CMDTYP_DEPENDXFER, 0, 0, 0);
}

static void dwc3_process_irq(void)
{
    uint32_t evcnt = mmio_read_32(base + DWC3_GEVNTCOUNT);
    uint32_t ev;

    arch_invalidate_cache_range((uintptr_t)ev_buffer, sizeof(uint32_t) * DWC3_EV_BUFFER_SIZE);

    for (unsigned int i = 0; i < (evcnt / 4); i++) {
        ev = ev_buffer[ev_index];

        /* Device Specific Events DEVT*/
        if (ev & 1) {
            uint8_t ev_type = ((ev >> 8) & 0xF);

            switch (ev_type) {
            case 2: /* Connection done */
            {
                LOG_INFO("Connection done 0x%x", ev);
            } break;
            case 1: /* USB Reset*/
            {
                LOG_INFO("USB Reset %u 0x%x", ev_index, ev);
                mmio_clrsetbits_32(base + DWC3_DCFG, DCFG_DEVADDR_MASK, 0);
            } break;
            default:
                LOG_ERR("Unknown event %x", ev_type);
            }
        } else { /* Device Endpoint-n events*/
            // TODO: Error handling?
            // What if an error is set on the endpoint, we probably want to set
            // done = true, but also indicate upwards that the xfer failed.
            uint32_t ep = (ev >> 1) & 0x1F;
            // uint32_t ev_param = (ev >> 16) & 0xFFFF;
            // uint32_t ev_status = (ev >> 12) & 0xF;
            uint32_t ev_cc = (ev >> 6) & 0xF;
            // LOG_DBG("EV EP%u %s, param: %x, sts: %x, cc: %x (0x%08x) %u", ep>>1,ep&1?"IN":"OUT",
            // ev_param, ev_status, ev_cc, ev, evcnt);
            if (ev_cc == 1) {
                xfer_status[ep].done = true;
                // LOG_DBG("%s: Xfer complete", ep_to_str(ep));
            }
        }

        ev_index = (((ev_index + 1) % DWC3_EV_BUFFER_SIZE));
        mmio_write_32(base + DWC3_GEVNTCOUNT, 4);
    }
}

static int dwc3_udc_xfer_complete(usb_ep_t ep)
{
    dwc3_process_irq();

    if (ep >= USB_EP_END)
        return -PB_ERR_PARAM;
    if (!xfer_status[ep].done)
        return -PB_ERR_AGAIN;

    if (!(ep & 1) && xfer_status[ep].length) {
        /* Output from host, invalidate cache*/
        // LOG_DBG("Invalidate %p, len=%zu", xfer_status[ep].buf, xfer_status[ep].length);
        arch_invalidate_cache_range((uintptr_t)xfer_status[ep].buf, xfer_status[ep].length);
    }

    return PB_OK;
}

static void dwc3_reset(void)
{
    LOG_INFO("Reset");
    /* Read DCFG and mask out Device address*/
    mmio_clrsetbits_32(base + DWC3_DCFG, DCFG_DEVADDR_MASK, 0);
}

/*
 * TODO:
 *
 * 1. Set DALEPENA to 0x3 to disable all endpoints other than the default control endpoint 0
 * 2. Issue a DEPENDXFER command for any active transfers (except for the default control endpoint
 * 0) 3.
 *
 */

static int dwc3_udc_set_configuration(const struct usb_endpoint_descriptor *eps, size_t no_of_eps)
{
    int rc;
    enum usb_ep_type ep_type;
    usb_ep_t ep;
    uint16_t max_pkt_sz;

    mmio_write_32(base + DWC3_DALEPENA, DALEPENA_EP0_OUT | DALEPENA_EP0_IN);

    // TODO: Cancel any active transfers
    // TODO: DEPCFG to re-initialize TXFIFO allocation

    /* Here we must issue DEPSTARTCFG with XferRscIdx = 2 (COMMANDPARAM).
     * See 11.1.2.5.8 (Command 9: Start new configuration) in RM.
     *
     * There arent much more details on why this particular field must be set
     * to two. According to the datasheet we're "Re-initializing the transfer
     * resource allocation".
     */
    rc = dwc3_command(0, CMDTYP_DEPSTARTCFG | DEPCMD_COMMANDPARAM(2), 0, 0, 0);

    if (rc != PB_OK)
        return rc;

    for (size_t n = 0; n < no_of_eps; n++) {
        ep = (eps[n].bEndpointAddress & 0x7f) * 2;
        if (eps[n].bEndpointAddress & 0x80)
            ep++;

        ep_type = eps[n].bmAttributes;
        max_pkt_sz = eps[n].wMaxPacketSize;

        rc = dwc3_config_ep(ep, max_pkt_sz, ep_type);

        if (rc != PB_OK) {
            LOG_ERR("Configuration failed (%i)", rc);
            return rc;
        }

        mmio_clrsetbits_32(base + DWC3_DALEPENA, 0, (1 << ep));
    }

    return PB_OK;
}

static int dwc3_udc_poll_setup_pkt(struct usb_setup_packet *pkt)
{
    int rc;

    dwc3_process_irq();

    if (!setup_pkt_queued) {
        /* Queue up the first control transfer on EP0 */
        rc = dwc3_udc_xfer_start(
            USB_EP0_OUT, (uint8_t *)&setup_pkt, sizeof(struct usb_setup_packet));

        if (rc != PB_OK)
            return rc;
        setup_pkt_queued = true;
    }

    if (xfer_status[USB_EP0_OUT].done) {
        arch_invalidate_cache_range((uintptr_t)&setup_pkt, sizeof(setup_pkt));
        memcpy(pkt, &setup_pkt, sizeof(*pkt));
        setup_pkt_queued = false;
        return PB_OK;
    } else {
        return -PB_ERR_AGAIN;
    }
}

static int dwc3_udc_set_address(uint16_t addr)
{
    mmio_clrsetbits_32(base + DWC3_DCFG, DCFG_DEVADDR_MASK, DCFG_DEVADDR(addr));

    // Hack: It seems like scheduling an IN transfer on EP0 too quickly after
    // we've updated the device address fails.
    pb_delay_us(250);
    return PB_OK;
}

static int dwc3_udc_xfer_zlp(usb_ep_t ep)
{
    int rc = dwc3_udc_xfer_start(ep, NULL, 0);

    if (rc != 0)
        return 0;

    do {
        rc = dwc3_udc_xfer_complete(ep);
    } while (rc == -PB_ERR_AGAIN);

    return rc;
}

static int dwc3_udc_init(void)
{
    int err;
    ev_index = 0;

    LOG_INFO("Init");
    version = (mmio_read_32(base + DWC3_CAPLENGTH) >> 16) & 0xFFFF;
    caps = mmio_read_32(base + DWC3_CAPLENGTH) & 0xFF;

    memset((void *)ev_buffer, 0, sizeof(ev_buffer));
    memset((void *)trbs, 0, sizeof(trbs));
    memset(xfer_status, 0, sizeof(xfer_status[0]) * USB_EP_END);

    LOG_INFO("HCI ver: 0x%x, caps: 0x%x", version, caps);

    /* Put core in reset */
    mmio_clrsetbits_32(base + DWC3_GCTL, 0, GCTL_CORESOFTRESET);
    /* Disable all events */
    mmio_write_32(base + DWC3_DEVTEN, 0);
    /* Reset USB3 PHY */
    mmio_clrsetbits_32(base + DWC3_GUSB3PIPECTL, 0, GUSB3PIPECTL_PHYSOFTRST);
    /* Reset USB2 PHY */
    mmio_clrsetbits_32(base + DWC3_GUSB2PHYCFG, 0, GUSB2PHYCFG_PHYSOFTRST);

    // TODO: Double check datasheet and add comment about these delays
    pb_delay_ms(100);

    /* Release resets */
    mmio_clrsetbits_32(base + DWC3_GUSB3PIPECTL, GUSB3PIPECTL_PHYSOFTRST, 0);
    mmio_clrsetbits_32(base + DWC3_GUSB2PHYCFG, GUSB2PHYCFG_PHYSOFTRST, 0);

    pb_delay_ms(100); /* TODO: Is this really needed? */

    mmio_clrsetbits_32(base + DWC3_GCTL, GCTL_CORESOFTRESET, 0);
    mmio_clrsetbits_32(base + DWC3_GUSB2PHYCFG, GUSB2PHYCFG_SUSPENDUSB2, 0);

    /* Reset usb controller */
    mmio_clrsetbits_32(base + DWC3_DCTL, 0, 1 << 30);

    while (mmio_read_32(base + DWC3_DCTL) & (1 << 30))
        ;

    arch_clean_cache_range((uintptr_t)ev_buffer, sizeof(ev_buffer));
    mmio_write_32(base + DWC3_GEVNTADRLO, (uintptr_t)ev_buffer);

    mmio_clrsetbits_32(base + DWC3_GCTL, DWC3_GCTL_SCALEDOWN_MASK, 0);
    mmio_clrsetbits_32(base + DWC3_GCTL, GCTL_BYPSSETADDR, 0);

    mmio_write_32(base + DWC3_GTXFIFOSIZ_0, 0x82);
    mmio_write_32(base + DWC3_GRXFIFOSIZ_0, 0x305);

    /* Configure event buffer */
    mmio_write_32(base + DWC3_GEVNTADRHI, 0);
    mmio_write_32(base + DWC3_GEVNTSIZ, DWC3_EV_BUFFER_SIZE * 4);
    mmio_write_32(base + DWC3_GEVNTCOUNT, 0);

    mmio_clrsetbits_32(base + DWC3_GCTL, GCTL_PRTCAPDIR_MASK, GCTL_PRTCAPDIR(2) | GCTL_DSBLCLKGTNG);

    mmio_write_32(base + DWC3_DEVTEN,
                  DEVTEN_ERRTICERREVTEN | DEVTEN_CONNECTDONEEVTEN | DEVTEN_USBRSTEVTEN |
                      DEVTEN_DISSCONNEVTEN);

    err = dwc3_command(0, CMDTYP_DEPSTARTCFG, 0, 0, 0);

    if (err != PB_OK)
        return err;

    /* Perform EP0 out configuration */
    err = dwc3_config_ep(0, 64, 0);

    if (err != PB_OK)
        return err;

    err = dwc3_config_ep(1, 64, 0);

    if (err != PB_OK)
        return err;

    dwc3_reset();

    mmio_write_32(base + DWC3_DALEPENA, DALEPENA_EP0_OUT | DALEPENA_EP0_IN);
    mmio_clrsetbits_32(base + DWC3_DCTL, 0, 1 << 31);

    return PB_OK;
}

static int dwc3_udc_stop(void)
{
    mmio_clrsetbits_32(base + DWC3_DCTL, 1 << 31, 0);
    return PB_OK;
}

int imx_dwc3_udc_init(uintptr_t base_)
{
    base = base_;

    static const struct usbd_hal_ops ops = {
        .init = dwc3_udc_init,
        .stop = dwc3_udc_stop,
        .xfer_start = dwc3_udc_xfer_start,
        .xfer_complete = dwc3_udc_xfer_complete,
        .xfer_cancel = dwc3_udc_xfer_cancel,
        .poll_setup_pkt = dwc3_udc_poll_setup_pkt,
        .set_configuration = dwc3_udc_set_configuration,
        .set_address = dwc3_udc_set_address,
        .ep0_xfer_zlp = dwc3_udc_xfer_zlp,
    };

    return usbd_init_hal_ops(&ops);
}
