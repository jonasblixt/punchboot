/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Synopsys DWC_usb3 device controller, high-speed only, polled.
 *
 * Follows the device programming model in the i.MX 8M Plus RM (11.2.2.5):
 * register initialization on power on, USB reset, connect done,
 * SetAddress, SetConfiguration and disconnect, and the control transfer
 * programming model:
 *
 *  - Two-stage: Control-Setup TRB on EP0, XferComplete, wait for
 *    XferNotReady(Status) on EP1, Control-Status-2 TRB on EP1, XferComplete.
 *  - Control read: Control-Setup TRB on EP0, XferComplete, Control-Data TRB
 *    on EP1, XferComplete, wait for XferNotReady(Status) on EP0,
 *    Control-Status-3 TRB on EP0, XferComplete.
 *
 * Endpoints are physical endpoints, numbered as usb_ep_t: 2n is USB
 * endpoint n OUT, 2n + 1 is IN. Each has one TRB.
 *
 * usbd waits for endpoint 0 in tight loops and treats an error during
 * enumeration as fatal, so a control transfer that a USB reset or a new
 * SETUP cuts short, or that times out, ends as done (aborted) instead of an
 * error; the next poll_setup_pkt() starts over. Bulk transfers return
 * -PB_ERR_AGAIN until done and -PB_ERR_IO after a USB reset.
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

#define DWC3_EV_BUFFER_ENTRIES  64
#define DWC3_EV_BUFFER_BYTES    (DWC3_EV_BUFFER_ENTRIES * sizeof(uint32_t))
#define DWC3_CMD_TIMEOUT_us     10000
#define DWC3_RESET_TIMEOUT_us   100000
#define DWC3_HALT_TIMEOUT_us    100000
#define DWC3_EP0_TIMEOUT_us     500000
/* The RM gives no duration for the PHY soft resets */
#define DWC3_PHY_RESET_DELAY_us 1000

/* Endpoint 0 max packet size before connect done, and for each speed (RM 11.2.2.5.1) */
#define DWC3_EP0_MPS_INIT       512
#define DWC3_EP0_MPS_HS_FS      64
#define DWC3_SETUP_PKT_SIZE     sizeof(struct usb_setup_packet)

#define EP_IS_IN(ep)            ((ep) & 1)
#define EP_USB_NUMBER(ep)       ((ep) >> 1)

/* bEndpointAddress in an endpoint descriptor */
#define USB_EP_ADDR_NUMBER_MASK 0x0f
#define USB_EP_ADDR_DIR_IN      0x80

/* One TRB per physical endpoint, each in its own cache line: a clean of one
 * must not write back a stale copy of a TRB the controller has updated */
struct dwc3_trb {
    uint32_t bptrl;
    uint32_t bptrh;
    uint32_t size;
    uint32_t control;
} __attribute__((packed, aligned(64)));

struct dwc3_ep {
    bool active; /* Started, and neither completed nor ended */
    bool complete; /* XferComplete received */
    bool not_ready; /* XferNotReady received, stage in not_ready_stage */
    unsigned int not_ready_stage;
    bool cmd_complete; /* EPCmdCmplt received (End Transfer) */
    bool reset; /* A USB reset ended this transfer */
    uint8_t rsc_idx;
    uint16_t mps;
    void *buf;
    size_t length;
};

/* Endpoint 0 control transfer state */
static struct {
    bool setup_armed; /* Control-Setup TRB started on EP0 */
    bool in_control; /* A SETUP was handed to usbd and its status stage isn't done */
    bool data_stage; /* The current control transfer had a data stage */
    bool aborted; /* The current control transfer was cut short */
    uint16_t mps;
} ep0;

static uint32_t ev_buffer[DWC3_EV_BUFFER_ENTRIES] __aligned(DWC3_EV_BUFFER_BYTES) __section(".no_"
                                                                                            "init");
static struct usb_setup_packet setup_pkt __aligned(64) __section(".no_init");
static struct dwc3_trb trbs[USB_EP_END] __section(".no_init");
static struct dwc3_ep eps[USB_EP_END];
static unsigned int ev_index;
static bool reset_pending;
static bool connect_done_pending;
static bool disconnect_pending;
static uintptr_t base;

static int dwc3_wait_clear(uintptr_t reg, uint32_t mask, uint32_t timeout_us)
{
    struct pb_timeout to;

    pb_timeout_init_us(&to, timeout_us);

    while (mmio_read_32(base + reg) & mask) {
        if (pb_timeout_has_expired(&to))
            return -PB_ERR_TIMEOUT;
    }

    return PB_OK;
}

/* Issue an endpoint command and poll CmdAct (RM 11.2.2.7) */
static int dwc3_command(usb_ep_t ep, uint32_t cmd, uint32_t p0, uint32_t p1, uint32_t p2)
{
    uint32_t status;
    int rc;

    mmio_write_32(base + DWC3_DEPCMDPAR0(ep), p0);
    mmio_write_32(base + DWC3_DEPCMDPAR1(ep), p1);
    mmio_write_32(base + DWC3_DEPCMDPAR2(ep), p2);
    mmio_write_32(base + DWC3_DEPCMD(ep), cmd | DEPCMD_CMDACT);

    rc = dwc3_wait_clear(DWC3_DEPCMD(ep), DEPCMD_CMDACT, DWC3_CMD_TIMEOUT_us);
    status = DWC3_FIELD_GET(
        mmio_read_32(base + DWC3_DEPCMD(ep)), DEPCMD_CMDSTATUS_SHIFT, DEPCMD_CMDSTATUS_MASK);

    if (rc != PB_OK || status != 0) {
        LOG_ERR("%s: command %u failed (%i, status %u)",
                ep_to_str(ep),
                (unsigned int)(cmd & DEPCMD_CMDTYP_MASK),
                rc,
                (unsigned int)status);
        return (rc != PB_OK) ? rc : -PB_ERR_IO;
    }

    return PB_OK;
}

static int dwc3_config_ep(usb_ep_t ep, uint32_t action, uint16_t mps, uint32_t type)
{
    uint32_t p0, p1;
    /* Control endpoints use FIFO 0 in both directions, other IN endpoints
     * the FIFO of their USB endpoint number, OUT endpoints 0 */
    uint32_t fifo = (type == DEPCFG_EP_TYPE_CONTROL || !EP_IS_IN(ep)) ? 0 : EP_USB_NUMBER(ep);

    p0 = DEPCFG_P0_CONFIG_ACT(action) | DEPCFG_P0_BURST_SZ(0) | DEPCFG_P0_FIFO_NO(fifo) |
         DEPCFG_P0_MPS(mps) | DEPCFG_P0_EP_TYPE(type);
    p1 = DEPCFG_P1_USB_EP_NO(ep) | DEPCFG_P1_XFERNOTREADY_EN | DEPCFG_P1_XFERCOMPLETE_EN;

    eps[ep].mps = mps;

    return dwc3_command(ep, DEPCMD_DEPCFG, p0, p1, 0);
}

/* Record one event. Actions that issue commands are left to dwc3_handle_events() */
static void dwc3_record_event(uint32_t ev)
{
    if (ev & DWC3_EVT_DEVICE) {
        switch (DWC3_FIELD_GET(ev, DEVT_TYPE_SHIFT, DEVT_TYPE_MASK)) {
        case DEVT_USBRST:
            reset_pending = true;
            break;
        case DEVT_CONNECTDONE:
            connect_done_pending = true;
            break;
        case DEVT_DISCONNECT:
            disconnect_pending = true;
            break;
        case DEVT_ULSTCHNG:
            break;
        case DEVT_ERRTICERR:
            LOG_ERR("Erratic error");
            break;
        case DEVT_EVNTOVERFLOW:
            LOG_ERR("Event buffer overflow");
            break;
        default:
            LOG_DBG("Device event 0x%08x", ev);
            break;
        }
        return;
    }

    usb_ep_t ep = DWC3_FIELD_GET(ev, DEPEVT_EP_SHIFT, DEPEVT_EP_MASK);
    uint32_t status = DWC3_FIELD_GET(ev, DEPEVT_STATUS_SHIFT, DEPEVT_STATUS_MASK);

    if (ep >= USB_EP_END) {
        LOG_ERR("Event for unknown endpoint 0x%08x", ev);
        return;
    }

    switch (DWC3_FIELD_GET(ev, DEPEVT_TYPE_SHIFT, DEPEVT_TYPE_MASK)) {
    case DEPEVT_XFERCOMPLETE:
        eps[ep].active = false;
        eps[ep].complete = true;
        if (ep == USB_EP0_OUT && ep0.setup_armed) {
            /* A new control transfer: forget the last one's XferNotReady */
            eps[USB_EP0_OUT].not_ready = false;
            eps[USB_EP0_IN].not_ready = false;
        } else if (ep == USB_EP0_OUT || ep == USB_EP0_IN) {
            /* XferNotReady(Data) during a data stage is to be ignored, only
             * one after it counts (RM 11.2.2.5.4.2) */
            eps[ep].not_ready = false;
        }
        break;
    case DEPEVT_XFERNOTREADY:
        eps[ep].not_ready = true;
        eps[ep].not_ready_stage = status & DEPEVT_STATUS_CTRL_STAGE_MASK;
        break;
    case DEPEVT_EPCMDCMPLT:
        eps[ep].cmd_complete = true;
        break;
    default:
        LOG_DBG("%s: event 0x%08x", ep_to_str(ep), ev);
        break;
    }
}

static void dwc3_read_events(void)
{
    uint32_t count = mmio_read_32(base + DWC3_GEVNTCOUNT) & GEVNTCOUNT_EVNTCOUNT_MASK;

    if (count == 0)
        return;

    arch_invalidate_cache_range((uintptr_t)ev_buffer, sizeof(ev_buffer));

    for (uint32_t i = 0; i < count / sizeof(uint32_t); i++) {
        dwc3_record_event(ev_buffer[ev_index]);
        ev_index = (ev_index + 1) % DWC3_EV_BUFFER_ENTRIES;
        mmio_write_32(base + DWC3_GEVNTCOUNT, sizeof(uint32_t));
    }
}

/* Start a one-TRB transfer of 'size' bytes into or from 'buf' ('length' of them valid) */
static int dwc3_start_trb(usb_ep_t ep, void *buf, size_t length, size_t size, uint32_t trbctl)
{
    struct dwc3_trb *trb = &trbs[ep];
    uintptr_t trb_addr = (uintptr_t)trb;
    uintptr_t buf_addr = (uintptr_t)buf;
    int rc;

    if (buf != NULL && length > 0)
        arch_clean_cache_range(buf_addr, length);

    trb->bptrl = (uint32_t)buf_addr;
    trb->bptrh = (uint32_t)((uint64_t)buf_addr >> 32);
    trb->size = size & TRB_SIZE_BUFSIZ_MASK;
    /* One TRB per transfer: CHN=0, LST=1, CSP=0. HWO last */
    trb->control = TRB_CTRL_TRBCTL(trbctl) | TRB_CTRL_LST | TRB_CTRL_IOC | TRB_CTRL_HWO;
    arch_clean_cache_range(trb_addr, sizeof(*trb));

    eps[ep].complete = false;
    eps[ep].reset = false;
    eps[ep].buf = buf;
    eps[ep].length = length;

    /* Parameter 0: TD address high, parameter 1: low */
    rc = dwc3_command(
        ep, DEPCMD_DEPSTRTXFER, (uint32_t)((uint64_t)trb_addr >> 32), (uint32_t)trb_addr, 0);

    if (rc != PB_OK)
        return rc;

    eps[ep].rsc_idx = DWC3_FIELD_GET(
        mmio_read_32(base + DWC3_DEPCMD(ep)), DEPCMD_XFERRSCIDX_SHIFT, DEPCMD_XFERRSCIDX_MASK);
    eps[ep].active = true;

    return PB_OK;
}

/* End an active transfer and wait for the command complete event (RM 11.2.2.7.7) */
static int dwc3_end_transfer(usb_ep_t ep)
{
    struct pb_timeout to;
    int rc;

    if (!eps[ep].active)
        return PB_OK;

    eps[ep].cmd_complete = false;
    rc = dwc3_command(ep,
                      DEPCMD_DEPENDXFER | DEPCMD_CMDIOC | DEPCMD_HIPRI_FORCERM |
                          DEPCMD_COMMANDPARAM(eps[ep].rsc_idx),
                      0,
                      0,
                      0);

    if (rc != PB_OK)
        return rc;

    pb_timeout_init_us(&to, DWC3_CMD_TIMEOUT_us);

    while (!eps[ep].cmd_complete) {
        if (pb_timeout_has_expired(&to)) {
            LOG_ERR("%s: End Transfer timeout", ep_to_str(ep));
            return -PB_ERR_TIMEOUT;
        }
        dwc3_read_events();
    }

    eps[ep].active = false;
    return PB_OK;
}

static int dwc3_arm_setup(void)
{
    int rc;

    rc = dwc3_start_trb(
        USB_EP0_OUT, &setup_pkt, DWC3_SETUP_PKT_SIZE, DWC3_SETUP_PKT_SIZE, TRBCTL_CONTROL_SETUP);

    if (rc != PB_OK)
        return rc;

    ep0.setup_armed = true;
    ep0.in_control = false;
    ep0.data_stage = false;
    ep0.aborted = false;
    return PB_OK;
}

/* USB reset (RM 11.2.2.5.1.2) */
static void dwc3_handle_reset(void)
{
    LOG_DBG("USB reset");

    for (usb_ep_t ep = USB_EP1_OUT; ep < USB_EP_END; ep++) {
        if (eps[ep].active) {
            (void)dwc3_end_transfer(ep);
            eps[ep].reset = true;
        }
    }

    /* The control endpoint isn't affected by the reset: end a data or
     * status stage in progress and go back to the Setup stage */
    if (ep0.in_control) {
        (void)dwc3_end_transfer(USB_EP0_IN);
        if (!ep0.setup_armed)
            (void)dwc3_end_transfer(USB_EP0_OUT);
        ep0.aborted = true;
    }

    mmio_clrsetbits_32(base + DWC3_DCFG, DCFG_DEVADDR_MASK, DCFG_DEVADDR(0));
}

/* Connect done (RM 11.2.2.5.1.3) */
static void dwc3_handle_connect_done(void)
{
    uint32_t speed =
        DWC3_FIELD_GET(mmio_read_32(base + DWC3_DSTS), DSTS_CONNECTSPD_SHIFT, DSTS_CONNECTSPD_MASK);

    LOG_INFO("Connected, %s speed", speed == DWC3_SPEED_HIGH ? "high" : "full");

    mmio_clrsetbits_32(
        base + DWC3_GCTL, GCTL_RAMCLKSEL_MASK, GCTL_RAMCLKSEL(GCTL_RAMCLKSEL_BUS_CLK));

    ep0.mps = DWC3_EP0_MPS_HS_FS;
    (void)dwc3_config_ep(USB_EP0_OUT, DEPCFG_CONFIG_ACT_MODIFY, ep0.mps, DEPCFG_EP_TYPE_CONTROL);
    (void)dwc3_config_ep(USB_EP0_IN, DEPCFG_CONFIG_ACT_MODIFY, ep0.mps, DEPCFG_EP_TYPE_CONTROL);
}

/* Read the events and act on the device events */
static void dwc3_handle_events(void)
{
    dwc3_read_events();

    if (reset_pending) {
        reset_pending = false;
        dwc3_handle_reset();
    }

    if (connect_done_pending) {
        connect_done_pending = false;
        dwc3_handle_connect_done();
    }

    if (disconnect_pending) {
        disconnect_pending = false;
        LOG_INFO("Disconnected");
        /* RM 11.2.2.5.1.7 */
        mmio_clrsetbits_32(
            base + DWC3_DCTL, DCTL_ULSTCHNGREQ_MASK, DCTL_ULSTCHNGREQ(DCTL_ULSTCHNGREQ_RX_DET));
    }
}

/* Wait for an event on an endpoint 0 direction, false on abort or timeout */
static bool dwc3_ep0_wait(usb_ep_t ep, bool not_ready, unsigned int stage)
{
    struct pb_timeout to;

    pb_timeout_init_us(&to, DWC3_EP0_TIMEOUT_us);

    for (;;) {
        dwc3_handle_events();

        if (ep0.aborted)
            return false;

        if (not_ready) {
            if (eps[ep].not_ready && eps[ep].not_ready_stage == stage) {
                eps[ep].not_ready = false;
                return true;
            }
            /* After a control read data stage that is a multiple of the
             * max packet size and shorter than wLength, the host asks for
             * more: end it with a zero length packet (RM 11.2.2.5.4.2) */
            if (stage == DEPEVT_CTRL_STAGE_STATUS && ep0.data_stage && eps[USB_EP0_IN].not_ready &&
                eps[USB_EP0_IN].not_ready_stage == DEPEVT_CTRL_STAGE_DATA) {
                eps[USB_EP0_IN].not_ready = false;
                if (dwc3_start_trb(USB_EP0_IN, NULL, 0, 0, TRBCTL_NORMAL) != PB_OK)
                    return false;
            }
        } else if (eps[ep].complete) {
            return true;
        }

        if (pb_timeout_has_expired(&to)) {
            LOG_ERR("%s: control transfer timeout", ep_to_str(ep));
            (void)dwc3_end_transfer(ep);
            return false;
        }
    }
}

/* The control transfer is over (done or aborted): back to the Setup stage */
static int dwc3_ep0_finish(void)
{
    if (ep0.aborted) {
        (void)dwc3_end_transfer(USB_EP0_IN);
        if (!ep0.setup_armed)
            (void)dwc3_end_transfer(USB_EP0_OUT);
    }

    if (ep0.setup_armed) {
        ep0.in_control = false;
        ep0.aborted = false;
        return PB_OK;
    }

    return dwc3_arm_setup();
}

static int dwc3_udc_xfer_start(usb_ep_t ep, void *buf, size_t length)
{
    if (ep >= USB_EP_END)
        return -PB_ERR_PARAM;

    if (ep == USB_EP0_IN || ep == USB_EP0_OUT) {
        /* Data stage of a control transfer */
        if (!ep0.in_control || ep0.aborted)
            return PB_OK;
        ep0.data_stage = true;
        return dwc3_start_trb(ep, buf, length, length, TRBCTL_CONTROL_DATA);
    }

    if (eps[ep].mps == 0) {
        LOG_ERR("%s is not configured", ep_to_str(ep));
        return -PB_ERR_STATE;
    }

    /* An OUT buffer descriptor must be a multiple of the max packet size
     * (RM 11.2.2.5.2.3.3). The host sends at most 'length' bytes, so the
     * rounded up size never makes the controller write past them. */
    size_t size = length;

    if (!EP_IS_IN(ep))
        size = ((length + eps[ep].mps - 1) / eps[ep].mps) * eps[ep].mps;

    return dwc3_start_trb(ep, buf, length, size, TRBCTL_NORMAL);
}

static int dwc3_udc_xfer_complete(usb_ep_t ep)
{
    if (ep >= USB_EP_END)
        return -PB_ERR_PARAM;

    if (ep == USB_EP0_IN || ep == USB_EP0_OUT) {
        if (!ep0.in_control || ep0.aborted)
            return PB_OK;
        if (!dwc3_ep0_wait(ep, false, 0))
            ep0.aborted = true;
        return PB_OK;
    }

    dwc3_handle_events();

    if (eps[ep].reset)
        return -PB_ERR_IO;

    if (!eps[ep].complete)
        return -PB_ERR_AGAIN;

    if (!EP_IS_IN(ep) && eps[ep].length > 0) {
        /* Data from the host: drop lines the CPU may have fetched during DMA */
        arch_invalidate_cache_range((uintptr_t)eps[ep].buf, eps[ep].length);
    }

    return PB_OK;
}

static void dwc3_udc_xfer_cancel(usb_ep_t ep)
{
    if (ep < USB_EP_END)
        (void)dwc3_end_transfer(ep);
}

/*
 * Status stage. usbd calls it with EP0_IN after a request without data
 * stage, and with EP0_OUT after a control read. A call when no status
 * stage is due (usbd's SetIdle asks for both) does nothing.
 */
static int dwc3_udc_xfer_zlp(usb_ep_t ep)
{
    uint32_t trbctl = ep0.data_stage ? TRBCTL_CONTROL_STATUS3 : TRBCTL_CONTROL_STATUS2;

    if (ep != USB_EP0_IN && ep != USB_EP0_OUT)
        return -PB_ERR_PARAM;

    if (!ep0.in_control)
        return PB_OK;

    if (!ep0.aborted && dwc3_ep0_wait(ep, true, DEPEVT_CTRL_STAGE_STATUS)) {
        if (dwc3_start_trb(ep, NULL, 0, 0, trbctl) != PB_OK || !dwc3_ep0_wait(ep, false, 0))
            ep0.aborted = true;
    } else {
        ep0.aborted = true;
    }

    if (ep0.aborted)
        LOG_DBG("Control transfer aborted");

    return dwc3_ep0_finish();
}

static int dwc3_udc_poll_setup_pkt(struct usb_setup_packet *pkt)
{
    int rc;

    dwc3_handle_events();

    if (ep0.in_control) {
        if (!ep0.aborted) {
            /* usbd didn't finish the last request (unsupported): stall it */
            LOG_DBG("Stall EP0");
            (void)dwc3_end_transfer(USB_EP0_IN);
            (void)dwc3_command(USB_EP0_OUT, DEPCMD_DEPSSTALL, 0, 0, 0);
        }
        ep0.aborted = true;
        rc = dwc3_ep0_finish();
        if (rc != PB_OK)
            return rc;
    }

    if (!ep0.setup_armed) {
        rc = dwc3_arm_setup();
        if (rc != PB_OK)
            return rc;
    }

    if (!eps[USB_EP0_OUT].complete)
        return -PB_ERR_AGAIN;

    eps[USB_EP0_OUT].complete = false;
    ep0.setup_armed = false;
    ep0.in_control = true;
    ep0.data_stage = false;
    ep0.aborted = false;

    arch_invalidate_cache_range((uintptr_t)&setup_pkt, sizeof(setup_pkt));
    memcpy(pkt, &setup_pkt, sizeof(*pkt));
    return PB_OK;
}

/* SetAddress (RM 11.2.2.5.1.4): the status stage follows in xfer_zlp */
static int dwc3_udc_set_address(uint16_t addr)
{
    mmio_clrsetbits_32(base + DWC3_DCFG, DCFG_DEVADDR_MASK, DCFG_DEVADDR(addr));
    return PB_OK;
}

/* SetConfiguration (RM 11.2.2.5.1.5) */
static int dwc3_udc_set_configuration(const struct usb_endpoint_descriptor *descs, size_t n)
{
    uint32_t enabled = DALEPENA_EP(USB_EP0_OUT) | DALEPENA_EP(USB_EP0_IN);
    int rc;

    mmio_write_32(base + DWC3_DALEPENA, enabled);

    for (usb_ep_t ep = USB_EP1_OUT; ep < USB_EP_END; ep++) {
        rc = dwc3_end_transfer(ep);
        if (rc != PB_OK)
            return rc;
        eps[ep].mps = 0;
    }

    /* Re-initialize the TXFIFO allocation, then the transfer resources */
    rc = dwc3_config_ep(USB_EP0_IN, DEPCFG_CONFIG_ACT_MODIFY, ep0.mps, DEPCFG_EP_TYPE_CONTROL);
    if (rc != PB_OK)
        return rc;

    rc = dwc3_command(USB_EP0_OUT,
                      DEPCMD_DEPSTARTCFG | DEPCMD_COMMANDPARAM(DEPSTARTCFG_XFERRSCIDX_CONFIG),
                      0,
                      0,
                      0);
    if (rc != PB_OK)
        return rc;

    for (size_t i = 0; i < n; i++) {
        uint8_t addr = descs[i].bEndpointAddress;
        usb_ep_t ep = (addr & USB_EP_ADDR_NUMBER_MASK) * 2 + ((addr & USB_EP_ADDR_DIR_IN) ? 1 : 0);

        if (ep < USB_EP1_OUT || ep >= USB_EP_END) {
            LOG_ERR("Bad endpoint address 0x%02x", addr);
            return -PB_ERR_PARAM;
        }

        LOG_DBG("Configure %s, %u bytes", ep_to_str(ep), descs[i].wMaxPacketSize);

        rc = dwc3_config_ep(ep,
                            DEPCFG_CONFIG_ACT_INIT,
                            descs[i].wMaxPacketSize,
                            descs[i].bmAttributes & DEPCFG_EP_TYPE_MASK);
        if (rc != PB_OK)
            return rc;

        rc = dwc3_command(ep, DEPCMD_DEPXFERCFG, DEPXFERCFG_P0_NUMXFERRES, 0, 0);
        if (rc != PB_OK)
            return rc;

        enabled |= DALEPENA_EP(ep);
    }

    mmio_write_32(base + DWC3_DALEPENA, enabled);
    return PB_OK;
}

/* Power on or soft reset initialization (RM 11.2.2.5.1.1) */
static int dwc3_udc_init(void)
{
    uint32_t caplength = mmio_read_32(base + DWC3_CAPLENGTH);
    int rc;

    LOG_INFO("HCI version 0x%x",
             (unsigned int)DWC3_FIELD_GET(
                 caplength, CAPLENGTH_HCIVERSION_SHIFT, CAPLENGTH_HCIVERSION_MASK));

    memset(eps, 0, sizeof(eps));
    memset(&ep0, 0, sizeof(ep0));
    ev_index = 0;
    reset_pending = false;
    connect_done_pending = false;
    disconnect_pending = false;

    /* Core and PHY soft reset, then device mode (RM GCTL[PRTCAPDIR]) */
    mmio_clrsetbits_32(base + DWC3_DCTL, DCTL_RUN_STOP, 0);
    mmio_clrsetbits_32(base + DWC3_GCTL, 0, GCTL_CORESOFTRESET);
    mmio_clrsetbits_32(base + DWC3_GUSB3PIPECTL, 0, GUSB3PIPECTL_PHYSOFTRST);
    mmio_clrsetbits_32(base + DWC3_GUSB2PHYCFG, 0, GUSB2PHYCFG_PHYSOFTRST);
    pb_delay_us(DWC3_PHY_RESET_DELAY_us);
    mmio_clrsetbits_32(base + DWC3_GUSB3PIPECTL, GUSB3PIPECTL_PHYSOFTRST, 0);
    mmio_clrsetbits_32(base + DWC3_GUSB2PHYCFG, GUSB2PHYCFG_PHYSOFTRST, 0);
    pb_delay_us(DWC3_PHY_RESET_DELAY_us);
    mmio_clrsetbits_32(base + DWC3_GCTL,
                       GCTL_CORESOFTRESET | GCTL_PRTCAPDIR_MASK | GCTL_SCALEDOWN_MASK |
                           GCTL_BYPSSETADDR,
                       GCTL_PRTCAPDIR(GCTL_PRTCAPDIR_DEVICE) | GCTL_DSBLCLKGTNG);

    mmio_clrsetbits_32(base + DWC3_DCTL, 0, DCTL_CSFTRST);
    rc = dwc3_wait_clear(DWC3_DCTL, DCTL_CSFTRST, DWC3_RESET_TIMEOUT_us);
    if (rc != PB_OK) {
        LOG_ERR("Device soft reset timeout");
        return rc;
    }

    /* Endpoint commands need the USB2 PHY out of suspend */
    mmio_clrsetbits_32(base + DWC3_GUSB2PHYCFG, GUSB2PHYCFG_SUSPENDUSB20, 0);

    /* Event buffer, enabled by the GEVNTCOUNT write (RM 11.2.2.5.2.2) */
    memset(ev_buffer, 0, sizeof(ev_buffer));
    arch_clean_invalidate_cache_range((uintptr_t)ev_buffer, sizeof(ev_buffer));
    mmio_write_32(base + DWC3_GEVNTADRLO, (uint32_t)(uintptr_t)ev_buffer);
    mmio_write_32(base + DWC3_GEVNTADRHI, (uint32_t)((uint64_t)(uintptr_t)ev_buffer >> 32));
    mmio_write_32(base + DWC3_GEVNTSIZ,
                  GEVNTSIZ_EVNTINTRPTMASK | (DWC3_EV_BUFFER_BYTES & GEVNTSIZ_EVENTSIZ_MASK));
    mmio_write_32(base + DWC3_GEVNTCOUNT, 0);

    /* pb's descriptors are USB 2.0: high-speed at most */
    mmio_clrsetbits_32(base + DWC3_DCFG,
                       DCFG_DEVSPD_MASK | DCFG_DEVADDR_MASK,
                       DCFG_DEVSPD(DWC3_SPEED_HIGH) | DCFG_DEVADDR(0));

    mmio_write_32(base + DWC3_DEVTEN,
                  DEVTEN_ERRTICERREVTEN | DEVTEN_ULSTCNGEN | DEVTEN_CONNECTDONEEVTEN |
                      DEVTEN_USBRSTEVTEN | DEVTEN_DISSCONNEVTEN);

    rc = dwc3_command(USB_EP0_OUT,
                      DEPCMD_DEPSTARTCFG | DEPCMD_COMMANDPARAM(DEPSTARTCFG_XFERRSCIDX_INIT),
                      0,
                      0,
                      0);
    if (rc != PB_OK)
        return rc;

    ep0.mps = DWC3_EP0_MPS_INIT;

    for (usb_ep_t ep = USB_EP0_OUT; ep <= USB_EP0_IN; ep++) {
        rc = dwc3_config_ep(ep, DEPCFG_CONFIG_ACT_INIT, ep0.mps, DEPCFG_EP_TYPE_CONTROL);
        if (rc != PB_OK)
            return rc;

        rc = dwc3_command(ep, DEPCMD_DEPXFERCFG, DEPXFERCFG_P0_NUMXFERRES, 0, 0);
        if (rc != PB_OK)
            return rc;
    }

    rc = dwc3_arm_setup();
    if (rc != PB_OK)
        return rc;

    mmio_write_32(base + DWC3_DALEPENA, DALEPENA_EP(USB_EP0_OUT) | DALEPENA_EP(USB_EP0_IN));
    mmio_clrsetbits_32(base + DWC3_DCTL, 0, DCTL_RUN_STOP);

    return PB_OK;
}

/* Device-initiated disconnect (RM 11.2.2.5.1.8) */
static int dwc3_udc_stop(void)
{
    for (usb_ep_t ep = USB_EP0_OUT; ep < USB_EP_END; ep++)
        (void)dwc3_end_transfer(ep);

    mmio_clrsetbits_32(base + DWC3_DCTL, DCTL_RUN_STOP, 0);

    struct pb_timeout to;
    pb_timeout_init_us(&to, DWC3_HALT_TIMEOUT_us);

    while (!(mmio_read_32(base + DWC3_DSTS) & DSTS_DEVCTRLHLT)) {
        if (pb_timeout_has_expired(&to)) {
            LOG_ERR("Device halt timeout");
            return -PB_ERR_TIMEOUT;
        }
    }

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
