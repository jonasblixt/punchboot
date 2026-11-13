/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Command mode's transport on USB (usb.md 2): the six functions that command
 * mode calls, cm_transport_*(), as the class driver (pb_dev_cls.c) provides
 * them on the stack (usbd.c), over a fake controller. What each one starts
 * in the layers below, and nothing of how a controller behaves on the bus:
 * that is the conformance suite's (F-287) and the bench's.
 *
 * The stack keeps its controller and its state in statics, so the order of
 * the tests matters: no controller first, then not configured, then
 * configured.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/usb/pb_dev_cls.h>
#include <drivers/usb/usbd.h>
#include <pb/cm.h>
#include <pb/device_uuid.h>
#include <pb/pb.h>
#include <string.h>
#include <uuid.h>

#define DEVICE_UUID           "02e49231-756e-35ee-a982-378e5ba866a9"

/* bmRequestType and bRequest of the two requests that make a device ready */
#define REQ_SET_ADDRESS       0x05
#define REQ_SET_CONFIGURATION 0x09

int device_uuid(uuid_t uu)
{
    return uuid_parse(DEVICE_UUID, uu);
}

/* The fake controller */
static struct {
    int init_calls;
    int stop_calls;
    int start_calls;
    usb_ep_t start_ep;
    void *start_buf;
    size_t start_length;
    int start_rc;
    int complete_calls;
    usb_ep_t complete_ep;
    int complete_rc;
    int cancel_calls;
    bool setup_pending;
    struct usb_setup_packet setup;
    int set_address_calls;
    uint16_t address;
    int set_configuration_calls;
    size_t no_of_eps;
    uint8_t ep_addr[2];
    int zlp_calls;
} hal;

static int fake_init(void)
{
    hal.init_calls++;
    return PB_OK;
}

static int fake_stop(void)
{
    hal.stop_calls++;
    return PB_OK;
}

static int fake_xfer_start(usb_ep_t ep, void *buf, size_t length)
{
    hal.start_calls++;
    hal.start_ep = ep;
    hal.start_buf = buf;
    hal.start_length = length;
    return hal.start_rc;
}

static int fake_xfer_complete(usb_ep_t ep)
{
    hal.complete_calls++;
    hal.complete_ep = ep;
    return hal.complete_rc;
}

static void fake_xfer_cancel(usb_ep_t ep)
{
    (void)ep;
    hal.cancel_calls++;
}

static int fake_poll_setup_pkt(struct usb_setup_packet *pkt)
{
    if (!hal.setup_pending)
        return -PB_ERR_AGAIN;

    hal.setup_pending = false;
    *pkt = hal.setup;
    return PB_OK;
}

static int fake_set_configuration(const struct usb_endpoint_descriptor *eps, size_t no_of_eps)
{
    hal.set_configuration_calls++;
    hal.no_of_eps = no_of_eps;
    for (size_t i = 0; i < no_of_eps && i < ARRAY_SIZE(hal.ep_addr); i++)
        hal.ep_addr[i] = eps[i].bEndpointAddress;
    return PB_OK;
}

static int fake_set_address(uint16_t addr)
{
    hal.set_address_calls++;
    hal.address = addr;
    return PB_OK;
}

static int fake_ep0_xfer_zlp(usb_ep_t ep)
{
    (void)ep;
    hal.zlp_calls++;
    return PB_OK;
}

static const struct usbd_hal_ops fake_hal = {
    .init = fake_init,
    .stop = fake_stop,
    .xfer_start = fake_xfer_start,
    .xfer_complete = fake_xfer_complete,
    .xfer_cancel = fake_xfer_cancel,
    .poll_setup_pkt = fake_poll_setup_pkt,
    .set_configuration = fake_set_configuration,
    .set_address = fake_set_address,
    .ep0_xfer_zlp = fake_ep0_xfer_zlp,
};

/* The host sends a request without a data stage */
static void host_request(uint8_t request, uint16_t value)
{
    memset(&hal.setup, 0, sizeof(hal.setup));
    hal.setup.bRequest = request;
    hal.setup.wValue = value;
    hal.setup_pending = true;
}

static int setup(void **state)
{
    (void)state;
    memset(&hal, 0, sizeof(hal));
    return 0;
}

/* The controller is the platform's to register, in plat_cm_init()
 * (platform.md 3.10). Without one the transport doesn't start and nothing
 * else is called through */
static void test_no_controller(void **state)
{
    static uint8_t buf[512];
    (void)state;

    assert_int_equal(cm_transport_init(), -PB_ERR_IO);
    assert_int_equal(cm_transport_connect(), -PB_ERR_IO);
    assert_int_equal(cm_transport_read(buf, sizeof(buf)), -PB_ERR_IO);
    assert_int_equal(cm_transport_write(buf, sizeof(buf)), -PB_ERR_IO);
    assert_int_equal(cm_transport_complete(), -PB_ERR_IO);
    assert_int_equal(cm_transport_disconnect(), -PB_ERR_IO);
}

/* usb.md 2.1: init starts the class, the stack and the controller, and
 * doesn't wait for a host. Again when the transport is restarted */
static void test_init_starts_the_controller(void **state)
{
    (void)state;

    assert_int_equal(usbd_init_hal_ops(&fake_hal), PB_OK);
    assert_int_equal(cm_transport_init(), PB_OK);
    assert_int_equal(hal.init_calls, 1);
    assert_int_equal(hal.start_calls, 0);

    assert_int_equal(cm_transport_init(), PB_OK);
    assert_int_equal(hal.init_calls, 2);
}

/* usb.md 2.2: connect is polled. Not yet while the host hasn't configured
 * the device, ready once it has, with pb's two bulk endpoints (usb.md 6) */
static void test_connect_is_ready_when_configured(void **state)
{
    (void)state;

    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);

    host_request(REQ_SET_ADDRESS, 7);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);
    assert_int_equal(hal.set_address_calls, 1);
    assert_int_equal(hal.address, 7);

    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);

    host_request(REQ_SET_CONFIGURATION, 1);
    assert_int_equal(cm_transport_connect(), PB_OK);
    assert_int_equal(hal.set_configuration_calls, 1);
    assert_int_equal(hal.no_of_eps, 2);
    assert_int_equal(hal.ep_addr[0], USB_EP_ADDR_IN | 1);
    assert_int_equal(hal.ep_addr[1], 2);
}

/* usb.md 2.3: a read is a transfer from the host on bulk OUT 2, a write one
 * to the host on bulk IN 1, of the caller's buffer and length. They return
 * at once, with the controller's answer */
static void test_read_and_write_start_a_transfer(void **state)
{
    static uint8_t buf[1024];
    (void)state;

    assert_int_equal(cm_transport_read(buf, 512), PB_OK);
    assert_int_equal(hal.start_calls, 1);
    assert_int_equal(hal.start_ep, USB_EP2_OUT);
    assert_ptr_equal(hal.start_buf, buf);
    assert_int_equal(hal.start_length, 512);
    assert_int_equal(hal.complete_calls, 0);

    assert_int_equal(cm_transport_write(buf, sizeof(buf)), PB_OK);
    assert_int_equal(hal.start_calls, 2);
    assert_int_equal(hal.start_ep, USB_EP1_IN);
    assert_ptr_equal(hal.start_buf, buf);
    assert_int_equal(hal.start_length, sizeof(buf));
    assert_int_equal(hal.complete_calls, 0);

    hal.start_rc = -PB_ERR_IO;
    assert_int_equal(cm_transport_read(buf, 512), -PB_ERR_IO);
    assert_int_equal(cm_transport_write(buf, 512), -PB_ERR_IO);
}

/* usb.md 2.4: complete polls the transfer that was started, on its
 * endpoint, and gives the controller's answer: done, not yet, or lost */
static void test_complete_polls_the_transfer(void **state)
{
    static uint8_t buf[512];
    (void)state;

    assert_int_equal(cm_transport_read(buf, sizeof(buf)), PB_OK);
    hal.complete_rc = -PB_ERR_AGAIN;
    assert_int_equal(cm_transport_complete(), -PB_ERR_AGAIN);
    assert_int_equal(hal.complete_calls, 1);
    assert_int_equal(hal.complete_ep, USB_EP2_OUT);
    hal.complete_rc = PB_OK;
    assert_int_equal(cm_transport_complete(), PB_OK);
    assert_int_equal(hal.complete_calls, 2);

    assert_int_equal(cm_transport_write(buf, sizeof(buf)), PB_OK);
    hal.complete_rc = -PB_ERR_IO;
    assert_int_equal(cm_transport_complete(), -PB_ERR_IO);
    assert_int_equal(hal.complete_ep, USB_EP1_IN);
}

/* usb.md 2.5: disconnect is the controller's stop */
static void test_disconnect_stops_the_controller(void **state)
{
    (void)state;

    assert_int_equal(cm_transport_disconnect(), PB_OK);
    assert_int_equal(hal.stop_calls, 1);
    assert_int_equal(hal.init_calls, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_no_controller, setup),
        cmocka_unit_test_setup(test_init_starts_the_controller, setup),
        cmocka_unit_test_setup(test_connect_is_ready_when_configured, setup),
        cmocka_unit_test_setup(test_read_and_write_start_a_transfer, setup),
        cmocka_unit_test_setup(test_complete_polls_the_transfer, setup),
        cmocka_unit_test_setup(test_disconnect_stops_the_controller, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
