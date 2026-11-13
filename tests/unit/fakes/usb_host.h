/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A fake USB device controller with a host on its bus, for the tests of the
 * stack (usbd.c) and of what is built on it. The controller is a struct
 * usbd_hal_ops as a driver registers it (usb.md 4); the host is what a test
 * plays through the functions below: control requests, bulk transfers of
 * which any number of bytes have moved, a bus reset and a replug.
 *
 * It is no model of a controller. It keeps what the stack may rely on
 * (usb.md 4.3, 4.4) and, where the three drivers differ in what they do
 * today, a switch per difference (struct usb_host_config).
 */

#ifndef TESTS_UNIT_FAKES_USB_HOST_H
#define TESTS_UNIT_FAKES_USB_HOST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <drivers/usb/usbd.h>

/* bmRequestType */
#define USB_HOST_REQ_IN            0x80
#define USB_HOST_REQ_VENDOR        0x40
#define USB_HOST_REQ_CLASS         0x20
#define USB_HOST_REQ_INTERFACE     0x01

/* bRequest (USB 2.0 Table 9-4) */
#define USB_HOST_GET_STATUS        0x00
#define USB_HOST_SET_ADDRESS       0x05
#define USB_HOST_GET_DESCRIPTOR    0x06
#define USB_HOST_GET_CONFIGURATION 0x08
#define USB_HOST_SET_CONFIGURATION 0x09

struct usb_host_config {
    /* One transfer for all endpoints, as the ChipIdea's driver: a transfer
     * on endpoint 0 takes the place of a bulk transfer that wasn't cancelled,
     * whose xfer_complete then reads endpoint 0's result */
    bool one_xfer;
    /* A bus reset ends a bulk transfer that runs with -PB_ERR_IO, as the
     * DWC3's driver. Without it the reset is handled inside the driver, as
     * the other two do: the transfer is flushed and never completes */
    bool reports_reset;
};

/* What the host got for a control request */
struct usb_host_reply {
    bool taken; /* poll_setup_pkt gave the SETUP to the stack */
    bool status; /* The status stage was done: the request is answered */
    size_t length; /* The data stage's bytes */
    uint8_t data[512];
};

/* What the controller was asked to do, and where it is */
struct usb_host_state {
    int init_calls;
    int stop_calls;
    int cancel_calls;
    int bulk_starts; /* xfer_start on a bulk endpoint */
    int set_address_calls;
    int set_configuration_calls;
    int unanswered; /* Requests the stack took and didn't answer */
    int clobbered; /* Bulk transfers that endpoint 0 took the place of */
    bool attached; /* On the bus: between init and stop */
    bool configured; /* The bulk endpoints are set up */
    uint16_t address;
    size_t no_of_eps;
    uint8_t ep_addr[4];
};

/* A controller that isn't started, and a host that has sent nothing */
void usb_host_reset(const struct usb_host_config *cfg);

/* The operations, for usbd_init_hal_ops() */
const struct usbd_hal_ops *usb_host_hal_ops(void);

const struct usb_host_state *usb_host_state(void);

/* The result of the next xfer_start on a bulk endpoint, PB_OK again after it */
void usb_host_fail_next_start(int rc);

/* The host sends a SETUP. The one before it, if the stack didn't take it, is
 * replaced (usb.md 4.4) */
void usb_host_request(uint8_t request_type,
                      uint8_t request,
                      uint16_t value,
                      uint16_t index,
                      uint16_t length);

/* What the host got for the last request */
const struct usb_host_reply *usb_host_reply(void);

/* The host resets the bus: address 0, nothing configured, every transfer
 * ended (usb.md 4.3), a request that wasn't taken is gone */
void usb_host_bus_reset(void);

/* The cable is pulled and put back: no driver sees the unplug today, so for
 * the device it is the bus reset of the new connection */
void usb_host_replug(void);

/* The host sends 'length' bytes on the bulk OUT endpoint. They reach the
 * device when a transfer runs there, in the order they were sent; bytes a
 * cancelled transfer had taken are lost */
void usb_host_send(const void *data, size_t length);

/* The host reads at most 'size' bytes from the bulk IN endpoint, and gets
 * what the transfer that runs there has left. Returns the number of bytes */
size_t usb_host_receive(void *data, size_t size);

#endif // TESTS_UNIT_FAKES_USB_HOST_H
