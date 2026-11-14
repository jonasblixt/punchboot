/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * See usb_host.h
 */

#include "usb_host.h"

#include <pb/pb.h>
#include <string.h>

#define EP_IS_IN(ep)   ((ep) & 1)
#define EP_IS_BULK(ep) ((ep) >= USB_EP1_OUT && (ep) < USB_EP_END)

struct xfer {
    bool active; /* Started, and neither done, cancelled nor ended */
    bool reset; /* A bus reset ended it */
    bool clobbered; /* Endpoint 0 took its place */
    uint8_t *buf;
    size_t length;
    size_t done; /* Bytes that moved */
};

static struct usb_host_config config;
static struct usb_host_state state;
static struct usb_host_reply reply;
static struct xfer xfers[USB_EP_END];
static struct usb_setup_packet setup;
static bool setup_pending;
static bool in_control; /* A SETUP was taken and its status stage isn't done */
static int next_start_rc;
static int next_poll_rc;
static int next_status_rc;

/* What the host sent on bulk OUT and no transfer took yet */
static uint8_t out_fifo[64 * 1024];
static size_t out_fifo_length;

/* Off the bus, or a bus reset: what the controller forgets */
static void controller_reset(void)
{
    memset(xfers, 0, sizeof(xfers));
    state.configured = false;
    state.address = 0;
    setup_pending = false;
    in_control = false;
    out_fifo_length = 0;
}

/* Moves what the host sent into the transfer that waits for it */
static void pump(void)
{
    for (usb_ep_t ep = USB_EP1_OUT; ep < USB_EP_END; ep++) {
        struct xfer *x = &xfers[ep];
        size_t n = x->length - x->done;

        if (EP_IS_IN(ep) || !x->active)
            continue;

        if (n > out_fifo_length)
            n = out_fifo_length;

        if (n == 0)
            continue;

        memcpy(x->buf + x->done, out_fifo, n);
        x->done += n;
        out_fifo_length -= n;
        memmove(out_fifo, out_fifo + n, out_fifo_length);
    }
}

/* One transfer for the controller: one on endpoint 0 takes the place of a
 * bulk transfer that still runs */
static void ep0_takes_the_transfer(void)
{
    if (!config.one_xfer)
        return;

    for (usb_ep_t ep = USB_EP1_OUT; ep < USB_EP_END; ep++) {
        if (xfers[ep].active) {
            xfers[ep].active = false;
            xfers[ep].clobbered = true;
            state.clobbered++;
        }
    }
}

static int hal_init(void)
{
    state.init_calls++;
    controller_reset();
    state.attached = true;
    return PB_OK;
}

static int hal_stop(void)
{
    state.stop_calls++;
    controller_reset();
    state.attached = false;
    return PB_OK;
}

static int hal_xfer_start(usb_ep_t ep, void *buf, size_t length)
{
    struct xfer *x;
    int rc = next_start_rc;

    if (ep >= USB_EP_END)
        return -PB_ERR_PARAM;

    if (!EP_IS_BULK(ep)) {
        /* The data stage of a control transfer */
        ep0_takes_the_transfer();

        if (in_control && EP_IS_IN(ep) && length > 0) {
            reply.length = length < sizeof(reply.data) ? length : sizeof(reply.data);
            memcpy(reply.data, buf, reply.length);
        }

        return PB_OK;
    }

    state.bulk_starts++;
    next_start_rc = PB_OK;

    if (rc != PB_OK)
        return rc;

    if (!state.configured)
        return -PB_ERR_STATE;

    x = &xfers[ep];
    memset(x, 0, sizeof(*x));
    x->active = true;
    x->buf = buf;
    x->length = length;
    pump();

    return PB_OK;
}

static int hal_xfer_complete(usb_ep_t ep)
{
    struct xfer *x;

    if (ep >= USB_EP_END)
        return -PB_ERR_PARAM;

    if (!EP_IS_BULK(ep))
        return PB_OK;

    x = &xfers[ep];

    if (x->reset && config.reports_reset)
        return -PB_ERR_IO;

    if (x->clobbered) {
        /* Endpoint 0's transfer is done, which is all the driver looks at */
        x->clobbered = false;
        return PB_OK;
    }

    if (!x->active)
        return -PB_ERR_AGAIN;

    pump();

    if (x->done < x->length)
        return -PB_ERR_AGAIN;

    x->active = false;
    return PB_OK;
}

static void hal_xfer_cancel(usb_ep_t ep)
{
    state.cancel_calls++;

    if (ep < USB_EP_END)
        memset(&xfers[ep], 0, sizeof(xfers[ep]));
}

static int hal_poll_setup_pkt(struct usb_setup_packet *pkt)
{
    int rc = next_poll_rc;

    next_poll_rc = PB_OK;

    if (rc != PB_OK)
        return rc;

    if (in_control) {
        /* No status stage: the host waits out its timeout, or gets a stall */
        state.unanswered++;
        in_control = false;
    }

    if (!setup_pending)
        return -PB_ERR_AGAIN;

    setup_pending = false;
    in_control = true;
    reply.taken = true;
    *pkt = setup;
    return PB_OK;
}

static int hal_set_configuration(const struct usb_endpoint_descriptor *eps, size_t no_of_eps)
{
    state.set_configuration_calls++;
    state.no_of_eps = no_of_eps;

    for (size_t i = 0; i < no_of_eps && i < ARRAY_SIZE(state.ep_addr); i++)
        state.ep_addr[i] = eps[i].bEndpointAddress;

    /* Called again it starts the endpoints over (usb.md 4.4) */
    for (usb_ep_t ep = USB_EP1_OUT; ep < USB_EP_END; ep++)
        memset(&xfers[ep], 0, sizeof(xfers[ep]));

    state.configured = true;
    return PB_OK;
}

static int hal_set_address(uint16_t addr)
{
    state.set_address_calls++;
    state.address = addr;
    return PB_OK;
}

static int hal_ep0_xfer_zlp(usb_ep_t ep)
{
    int rc = next_status_rc;

    ep0_takes_the_transfer();
    next_status_rc = PB_OK;

    if (rc != PB_OK) {
        /* The host didn't take it: the control transfer is over, unanswered */
        in_control = false;
        return rc;
    }

    if (in_control) {
        reply.status = true;
        reply.status_ep = ep;
        in_control = false;
    } else {
        /* A controller primes it all the same, and the host never takes
         * it: the ChipIdea's waits for it */
        state.stray_status++;
    }

    return PB_OK;
}

static const struct usbd_hal_ops hal_ops = {
    .init = hal_init,
    .stop = hal_stop,
    .xfer_start = hal_xfer_start,
    .xfer_complete = hal_xfer_complete,
    .xfer_cancel = hal_xfer_cancel,
    .poll_setup_pkt = hal_poll_setup_pkt,
    .set_configuration = hal_set_configuration,
    .set_address = hal_set_address,
    .ep0_xfer_zlp = hal_ep0_xfer_zlp,
};

void usb_host_reset(const struct usb_host_config *cfg)
{
    memset(&config, 0, sizeof(config));
    memset(&state, 0, sizeof(state));
    memset(&reply, 0, sizeof(reply));
    next_start_rc = PB_OK;
    next_poll_rc = PB_OK;
    next_status_rc = PB_OK;
    controller_reset();

    if (cfg != NULL)
        config = *cfg;
}

const struct usbd_hal_ops *usb_host_hal_ops(void)
{
    return &hal_ops;
}

const struct usb_host_state *usb_host_state(void)
{
    return &state;
}

void usb_host_fail_next_start(int rc)
{
    next_start_rc = rc;
}

void usb_host_fail_next_poll(int rc)
{
    next_poll_rc = rc;
}

void usb_host_fail_next_status(int rc)
{
    next_status_rc = rc;
}

void usb_host_request(uint8_t request_type,
                      uint8_t request,
                      uint16_t value,
                      uint16_t index,
                      uint16_t length)
{
    memset(&reply, 0, sizeof(reply));
    setup.bRequestType = request_type;
    setup.bRequest = request;
    setup.wValue = value;
    setup.wIndex = index;
    setup.wLength = length;
    setup_pending = true;
}

const struct usb_host_reply *usb_host_reply(void)
{
    return &reply;
}

void usb_host_bus_reset(void)
{
    bool reset[USB_EP_END];

    for (usb_ep_t ep = 0; ep < USB_EP_END; ep++)
        reset[ep] = xfers[ep].active;

    controller_reset();

    for (usb_ep_t ep = 0; ep < USB_EP_END; ep++)
        xfers[ep].reset = reset[ep];
}

void usb_host_replug(void)
{
    usb_host_bus_reset();
}

void usb_host_send(const void *data, size_t length)
{
    if (length > sizeof(out_fifo) - out_fifo_length)
        length = sizeof(out_fifo) - out_fifo_length;

    if (length == 0)
        return;

    memcpy(out_fifo + out_fifo_length, data, length);
    out_fifo_length += length;
    pump();
}

size_t usb_host_receive(void *data, size_t size)
{
    for (usb_ep_t ep = USB_EP1_OUT; ep < USB_EP_END; ep++) {
        struct xfer *x = &xfers[ep];
        size_t n = x->length - x->done;

        if (!EP_IS_IN(ep) || !x->active)
            continue;

        if (n > size)
            n = size;

        if (n > 0)
            memcpy(data, x->buf + x->done, n);

        x->done += n;
        return n;
    }

    return 0;
}
