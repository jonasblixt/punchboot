/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The USB stack (usbd.c) and pb's device class (pb_dev_cls.c) with a host on
 * the bus: what a host sees on endpoint 0 (usb.md 3, 6), and what command
 * mode's transport (usb.md 2) does when the host sends requests, moves a
 * part of a transfer, resets the bus or is plugged in again. The controller
 * is a fake that plays the host (fakes/usb_host.h); the three drivers have no
 * model and are the bench's.
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

#include "fakes/usb_host.h"

#define DEVICE_UUID  "02e49231-756e-35ee-a982-378e5ba866a9"
#define PACKET       512

/* The class's strings (usb.md 6) */
#define STR_LANGUAGE 0
#define STR_PRODUCT  1
#define STR_SERIAL   2

#define NIL_UUID     "00000000-0000-0000-0000-000000000000"

/* The platform's unique id can't be read (a fuse read that fails): what is
 * in 'uu' then is undefined, and here it is nothing a serial number may be */
static bool device_uuid_fails;

int device_uuid(uuid_t uu)
{
    if (device_uuid_fails) {
        memset(uu, 0xa5, sizeof(uuid_t));
        return -PB_ERR_IO;
    }

    return uuid_parse(DEVICE_UUID, uu);
}

/* Command mode's buffers own their lines (usb.md 2.3) */
static uint8_t buf[8 * PACKET] __attribute__((aligned(64)));
static uint8_t data[sizeof(buf)];
static uint8_t got[sizeof(buf)];

static const struct usb_host_config one_xfer = { .one_xfer = true };

/* The transport, started on a controller: with NULL one whose endpoints are
 * independent, as the contract asks for (usb.md 4.3) */
static void start(const struct usb_host_config *cfg)
{
    usb_host_reset(cfg);
    assert_int_equal(usbd_init_hal_ops(usb_host_hal_ops()), PB_OK);
    if (cfg != NULL && cfg->one_xfer)
        usbd_hal_one_xfer();
    assert_int_equal(cm_transport_init(), PB_OK);
    assert_true(usb_host_state()->attached);

    for (size_t i = 0; i < sizeof(data); i++)
        data[i] = (uint8_t)(i * 7 + (i >> 8));

    memset(buf, 0xaa, sizeof(buf));
    memset(got, 0x55, sizeof(got));
}

static void host_get_descriptor(uint8_t type, uint8_t index, uint16_t length)
{
    usb_host_request(
        USB_HOST_REQ_IN, USB_HOST_GET_DESCRIPTOR, USB_DESC_VALUE(type, index), 0, length);
}

static void host_set_address(uint16_t address)
{
    usb_host_request(0, USB_HOST_SET_ADDRESS, address, 0, 0);
}

static void host_set_configuration(uint16_t value)
{
    usb_host_request(0, USB_HOST_SET_CONFIGURATION, value, 0, 0);
}

/* The request was answered: its data stage is 'length' bytes and the status
 * stage is done */
static void assert_answered(size_t length)
{
    assert_true(usb_host_reply()->taken);
    assert_true(usb_host_reply()->status);
    assert_int_equal(usb_host_reply()->length, length);
}

/* What a host does with a new device, as Linux: the device descriptor, an
 * address, the configuration, then it configures the device. Ready after the
 * last, and not before (usb.md 2.2) */
static void enumerate(void)
{
    host_get_descriptor(USB_DESC_TYPE_DEVICE, 0, 64);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);
    assert_answered(18);

    host_set_address(5);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);
    assert_answered(0);

    host_get_descriptor(USB_DESC_TYPE_CONFIG, 0, 9);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);
    assert_answered(9);

    host_get_descriptor(USB_DESC_TYPE_CONFIG, 0, 32);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);
    assert_answered(32);

    host_set_configuration(1);
    assert_int_equal(cm_transport_connect(), PB_OK);
    assert_answered(0);
    assert_true(usb_host_state()->configured);
}

/* A string descriptor's text, as ASCII */
static const char *reply_string(void)
{
    static char str[128];
    const struct usb_host_reply *r = usb_host_reply();
    size_t n = 0;

    assert_true(r->length >= 2);
    assert_int_equal(r->data[0], r->length);
    assert_int_equal(r->data[1], USB_DESC_TYPE_STRING);

    for (size_t i = 2; i + 1 < r->length; i += 2) {
        assert_int_equal(r->data[i + 1], 0);
        str[n++] = (char)r->data[i];
    }

    str[n] = 0;
    return str;
}

/* usb.md 4: every operation is mandatory. A driver that leaves one out
 * isn't registered, and the stack calls nothing of it: the transport has no
 * controller (F-287: three were called unchecked) */
static void test_every_operation_is_mandatory(void **state)
{
    typedef void (*op_t)(void);

    union {
        struct usbd_hal_ops ops;
        op_t op[sizeof(struct usbd_hal_ops) / sizeof(op_t)];
    } u;

    (void)state;

    assert_int_equal(sizeof(u.ops), sizeof(u.op));

    for (size_t i = 0; i < ARRAY_SIZE(u.op); i++) {
        usb_host_reset(NULL);
        u.ops = *usb_host_hal_ops();
        u.op[i] = NULL;

        assert_int_equal(usbd_init_hal_ops(&u.ops), -PB_ERR_PARAM);
        assert_int_equal(cm_transport_init(), -PB_ERR_IO);
        assert_int_equal(cm_transport_connect(), -PB_ERR_IO);
        assert_int_equal(cm_transport_read(buf, PACKET), -PB_ERR_IO);
        assert_int_equal(cm_transport_complete(), -PB_ERR_IO);
        assert_int_equal(cm_transport_disconnect(), -PB_ERR_IO);
        assert_int_equal(usb_host_state()->init_calls, 0);
    }

    assert_int_equal(usbd_init_hal_ops(NULL), -PB_ERR_PARAM);
    assert_int_equal(cm_transport_init(), -PB_ERR_IO);
}

/* usb.md 2.2: connect is polled and answers "not yet" until the host has
 * configured the device. A bus reset on the way is no error: the host starts
 * over, and so does the device, with address 0 */
static void test_connect(void **state)
{
    (void)state;
    start(NULL);

    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);

    host_get_descriptor(USB_DESC_TYPE_DEVICE, 0, 64);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);
    assert_answered(18);

    usb_host_bus_reset();
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);

    host_set_address(5);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);
    assert_answered(0);
    assert_int_equal(usb_host_state()->address, 5);

    usb_host_bus_reset();
    assert_int_equal(usb_host_state()->address, 0);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);

    host_get_descriptor(USB_DESC_TYPE_DEVICE, 0, 18);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);
    assert_answered(18);

    host_set_address(9);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);
    assert_int_equal(usb_host_state()->address, 9);

    host_get_descriptor(USB_DESC_TYPE_CONFIG, 0, 32);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);
    assert_false(usb_host_state()->configured);

    host_set_configuration(1);
    assert_int_equal(cm_transport_connect(), PB_OK);
    assert_answered(0);
    assert_true(usb_host_state()->configured);
    assert_int_equal(usb_host_state()->set_configuration_calls, 1);
    assert_int_equal(usb_host_state()->unanswered, 0);
}

/* usb.md 3.2: SET_CONFIGURATION takes the one configuration there is, or 0,
 * which deconfigures (F-287: the value wasn't looked at, and any configured
 * the device). One the device doesn't have is refused: nothing is set up,
 * the device isn't ready, and the request gets no status stage (the stall
 * that the contract asks for is F-138's) */
static void test_set_configuration_value(void **state)
{
    (void)state;
    start(NULL);

    host_set_address(5);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);

    host_set_configuration(2);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);
    assert_true(usb_host_reply()->taken);
    assert_false(usb_host_reply()->status);
    assert_false(usb_host_state()->configured);
    assert_int_equal(usb_host_state()->set_configuration_calls, 0);

    host_set_configuration(0x0101);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);
    assert_false(usb_host_reply()->status);
    assert_int_equal(usb_host_state()->set_configuration_calls, 0);

    /* Deconfigured, which it was: answered, and not ready */
    host_set_configuration(0);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);
    assert_answered(0);
    assert_false(usb_host_state()->configured);
    assert_int_equal(usb_host_state()->set_configuration_calls, 0);

    host_set_configuration(1);
    assert_int_equal(cm_transport_connect(), PB_OK);
    assert_answered(0);
    assert_true(usb_host_state()->configured);
    assert_int_equal(usb_host_state()->set_configuration_calls, 1);
}

/* usb.md 6, fixed like the wire: the ids, one configuration with one vendor
 * specific interface, bulk IN 1 and bulk OUT 2, 512 byte packets */
static void test_descriptors(void **state)
{
    static const uint8_t device[] = {
        0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x09,
        0x12, 0x19, 0x20, 0x00, 0x00, 0x01, 0x01, 0x02, 0x01,
    };
    static const uint8_t config[] = {
        0x09, 0x02, 0x20, 0x00, 0x01, 0x01, 0x00, 0x80, 0xfa, /* Configuration 1 */
        0x09, 0x04, 0x00, 0x00, 0x02, 0xff, 0xff, 0xff, 0x00, /* Interface 0 */
        0x07, 0x05, 0x81, 0x02, 0x00, 0x02, 0x00, /* Bulk IN 1 */
        0x07, 0x05, 0x02, 0x02, 0x00, 0x02, 0x00, /* Bulk OUT 2 */
    };
    static const uint8_t qualifier[] = {
        0x0a, 0x06, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x01, 0x00,
    };
    (void)state;
    start(NULL);

    host_get_descriptor(USB_DESC_TYPE_DEVICE, 0, 64);
    (void)cm_transport_connect();
    assert_answered(sizeof(device));
    assert_memory_equal(usb_host_reply()->data, device, sizeof(device));

    host_get_descriptor(USB_DESC_TYPE_CONFIG, 0, 255);
    (void)cm_transport_connect();
    assert_answered(sizeof(config));
    assert_memory_equal(usb_host_reply()->data, config, sizeof(config));

    host_get_descriptor(USB_DESC_TYPE_QUALIFIER, 0, 10);
    (void)cm_transport_connect();
    assert_answered(sizeof(qualifier));
    assert_memory_equal(usb_host_reply()->data, qualifier, sizeof(qualifier));

    /* The endpoints the controller is given are the descriptors' */
    enumerate();
    assert_int_equal(usb_host_state()->no_of_eps, 2);
    assert_int_equal(usb_host_state()->ep_addr[0], 0x81);
    assert_int_equal(usb_host_state()->ep_addr[1], 0x02);
}

/* usb.md 3.2: a reply is at most wLength and at most the descriptor */
static void test_reply_lengths(void **state)
{
    (void)state;
    start(NULL);

    /* The first read of a host that doesn't know endpoint 0's packet yet */
    host_get_descriptor(USB_DESC_TYPE_DEVICE, 0, 8);
    (void)cm_transport_connect();
    assert_answered(8);

    host_get_descriptor(USB_DESC_TYPE_DEVICE, 0, 0xffff);
    (void)cm_transport_connect();
    assert_answered(18);

    /* The configuration alone, to learn wTotalLength */
    host_get_descriptor(USB_DESC_TYPE_CONFIG, 0, 9);
    (void)cm_transport_connect();
    assert_answered(9);

    host_get_descriptor(USB_DESC_TYPE_CONFIG, 0, 0xffff);
    (void)cm_transport_connect();
    assert_answered(32);

    host_get_descriptor(USB_DESC_TYPE_STRING, STR_SERIAL, 2);
    (void)cm_transport_connect();
    assert_answered(2);
    assert_int_equal(usb_host_reply()->data[0], 2 + 2 * 36);

    host_get_descriptor(USB_DESC_TYPE_STRING, STR_SERIAL, 0xffff);
    (void)cm_transport_connect();
    assert_answered(2 + 2 * 36);
}

/* usb.md 3.1, 6: the strings, the device's UUID as its serial number, and
 * what makes Windows bind the device to WinUSB */
static void test_strings(void **state)
{
    static const uint8_t language[] = { 0x04, 0x03, 0x09, 0x04 };
    static const uint8_t msft[] = {
        0x12, 0x03, 'M', 0, 'S', 0, 'F', 0, 'T', 0, '1', 0, '0', 0, '0', 0, 0x01, 0x00,
    };
    static const uint8_t compat_id[] = {
        0x28, 0x00, 0x00, 0x00, /* 40 bytes */
        0x00, 0x01, /* Version 1.0 */
        0x04, 0x00, /* The compatible id descriptor */
        0x01, /* One section */
        0,    0,    0,    0,    0,   0,   0, 0x00, 0x00, /* Interface 0 */
        'W',  'I',  'N',  'U',  'S', 'B', 0, 0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    static const uint8_t status[] = { 0, 0 };
    (void)state;
    start(NULL);
    assert_int_equal(sizeof(compat_id), 40);

    host_get_descriptor(USB_DESC_TYPE_STRING, STR_LANGUAGE, 255);
    (void)cm_transport_connect();
    assert_answered(sizeof(language));
    assert_memory_equal(usb_host_reply()->data, language, sizeof(language));

    host_get_descriptor(USB_DESC_TYPE_STRING, STR_PRODUCT, 255);
    (void)cm_transport_connect();
    assert_answered(2 + 2 * strlen("Punchboot"));
    assert_string_equal(reply_string(), "Punchboot");

    host_get_descriptor(USB_DESC_TYPE_STRING, STR_SERIAL, 255);
    (void)cm_transport_connect();
    assert_answered(2 + 2 * strlen(DEVICE_UUID));
    assert_string_equal(reply_string(), DEVICE_UUID);

    host_get_descriptor(USB_DESC_TYPE_STRING, USB_DESC_STRING_MS_OS, 18);
    (void)cm_transport_connect();
    assert_answered(sizeof(msft));
    assert_memory_equal(usb_host_reply()->data, msft, sizeof(msft));

    /* Microsoft's vendor code from the string, and index 4 */
    usb_host_request(USB_HOST_REQ_IN | USB_HOST_REQ_VENDOR, 1, 0, USB_MS_EXT_COMPAT_ID, 40);
    (void)cm_transport_connect();
    assert_answered(sizeof(compat_id));
    assert_memory_equal(usb_host_reply()->data, compat_id, sizeof(compat_id));

    usb_host_request(USB_HOST_REQ_IN, USB_HOST_GET_STATUS, 0, 0, 2);
    (void)cm_transport_connect();
    assert_answered(sizeof(status));
    assert_memory_equal(usb_host_reply()->data, status, sizeof(status));

    assert_int_equal(usb_host_state()->unanswered, 0);
}

/* A device whose identity can't be read has the nil UUID as its serial
 * number, which is also what DEVICE_IDENTIFIER_READ gives then: a defined
 * string of the fixed length (usb.md 6), and nothing of pb's stack */
static void test_serial_number_without_an_identity(void **state)
{
    (void)state;
    start(NULL);

    device_uuid_fails = true;
    host_get_descriptor(USB_DESC_TYPE_STRING, STR_SERIAL, 255);
    (void)cm_transport_connect();
    device_uuid_fails = false;

    assert_answered(2 + 2 * strlen(NIL_UUID));
    assert_string_equal(reply_string(), NIL_UUID);
}

/* usb.md 2.3, 2.4: a read is done when exactly its length arrived, in as many
 * pieces as the host sends it */
static void test_read(void **state)
{
    (void)state;
    start(NULL);
    enumerate();

    assert_int_equal(cm_transport_read(buf, 4 * PACKET), PB_OK);
    assert_int_equal(cm_transport_complete(), -PB_ERR_AGAIN);

    usb_host_send(data, PACKET);
    assert_int_equal(cm_transport_complete(), -PB_ERR_AGAIN);

    usb_host_send(data + PACKET, 3 * PACKET);
    assert_int_equal(cm_transport_complete(), PB_OK);
    assert_memory_equal(buf, data, 4 * PACKET);

    /* What the host sent beyond it stays for the next read */
    usb_host_send(data, 2 * PACKET);
    assert_int_equal(cm_transport_read(buf, PACKET), PB_OK);
    assert_int_equal(cm_transport_complete(), PB_OK);
    assert_memory_equal(buf, data, PACKET);
    assert_int_equal(cm_transport_read(buf, PACKET), PB_OK);
    assert_int_equal(cm_transport_complete(), PB_OK);
    assert_memory_equal(buf, data + PACKET, PACKET);
}

/* usb.md 2.3, 2.4: a write is done when the host has read all of it */
static void test_write(void **state)
{
    (void)state;
    start(NULL);
    enumerate();

    memcpy(buf, data, 4 * PACKET);
    assert_int_equal(cm_transport_write(buf, 4 * PACKET), PB_OK);
    assert_int_equal(cm_transport_complete(), -PB_ERR_AGAIN);

    assert_int_equal(usb_host_receive(got, PACKET), PACKET);
    assert_int_equal(cm_transport_complete(), -PB_ERR_AGAIN);

    assert_int_equal(usb_host_receive(got + PACKET, sizeof(got) - PACKET), 3 * PACKET);
    assert_int_equal(cm_transport_complete(), PB_OK);
    assert_memory_equal(got, data, 4 * PACKET);
}

/* usb.md 3.2: hosts send requests at any time. One that comes while the
 * device waits for a command (a read of one packet, of which nothing has
 * arrived) is answered, and the command is read after it. On both kinds of
 * controller: with independent endpoints, and with one transfer for all */
static void request_while_waiting_for_a_command(const struct usb_host_config *cfg)
{
    start(cfg);
    enumerate();

    assert_int_equal(cm_transport_read(buf, PACKET), PB_OK);
    assert_int_equal(cm_transport_complete(), -PB_ERR_AGAIN);

    host_get_descriptor(USB_DESC_TYPE_STRING, STR_SERIAL, 255);
    assert_int_equal(cm_transport_complete(), -PB_ERR_AGAIN);
    assert_answered(2 + 2 * strlen(DEVICE_UUID));
    assert_string_equal(reply_string(), DEVICE_UUID);

    usb_host_send(data, PACKET);
    assert_int_equal(cm_transport_complete(), PB_OK);
    assert_memory_equal(buf, data, PACKET);
    assert_int_equal(usb_host_state()->clobbered, 0);
}

static void test_request_while_waiting_for_a_command(void **state)
{
    (void)state;

    request_while_waiting_for_a_command(NULL);
    assert_int_equal(usb_host_state()->cancel_calls, 0);
    assert_int_equal(usb_host_state()->bulk_starts, 1);

    /* One transfer for the controller: the read is given up for the
     * request and started again, with nothing lost */
    request_while_waiting_for_a_command(&one_xfer);
    assert_int_equal(usb_host_state()->cancel_calls, 1);
    assert_int_equal(usb_host_state()->bulk_starts, 2);
}

/* usb.md 3.2, 4.3: a request doesn't disturb a bulk transfer. The stack
 * answers on endpoint 0 and the read goes on where it was (F-203: it was
 * cancelled and started again from its first byte, so what had arrived was
 * lost and the rest landed at the buffer's start) */
static void test_request_during_a_read(void **state)
{
    (void)state;
    start(NULL);
    enumerate();

    assert_int_equal(cm_transport_read(buf, 4 * PACKET), PB_OK);
    usb_host_send(data, 2 * PACKET);
    assert_int_equal(cm_transport_complete(), -PB_ERR_AGAIN);

    host_get_descriptor(USB_DESC_TYPE_STRING, STR_SERIAL, 255);
    assert_int_equal(cm_transport_complete(), -PB_ERR_AGAIN);
    assert_answered(2 + 2 * strlen(DEVICE_UUID));

    usb_host_send(data + 2 * PACKET, 2 * PACKET);
    assert_int_equal(cm_transport_complete(), PB_OK);
    assert_memory_equal(buf, data, 4 * PACKET);
    assert_int_equal(usb_host_state()->cancel_calls, 0);
    assert_int_equal(usb_host_state()->bulk_starts, 1);
}

/* The same for a write: the host gets every byte once, in order */
static void test_request_during_a_write(void **state)
{
    (void)state;
    start(NULL);
    enumerate();

    memcpy(buf, data, 4 * PACKET);
    assert_int_equal(cm_transport_write(buf, 4 * PACKET), PB_OK);
    assert_int_equal(usb_host_receive(got, 2 * PACKET), 2 * PACKET);
    assert_int_equal(cm_transport_complete(), -PB_ERR_AGAIN);

    host_get_descriptor(USB_DESC_TYPE_DEVICE, 0, 18);
    assert_int_equal(cm_transport_complete(), -PB_ERR_AGAIN);
    assert_answered(18);

    assert_int_equal(usb_host_receive(got + 2 * PACKET, 2 * PACKET), 2 * PACKET);
    assert_int_equal(cm_transport_complete(), PB_OK);
    assert_memory_equal(got, data, 4 * PACKET);
    assert_int_equal(usb_host_state()->cancel_calls, 0);
}

/* A controller with one transfer for all its endpoints (the ChipIdea's
 * driver, usb.md 8) can't keep a bulk transfer while endpoint 0 is
 * answered. Where a part of it may have moved, the stack doesn't start it
 * again from its first byte: the transfer fails, which is "lost" (2.4) */
static void test_request_during_a_transfer_with_one_xfer(void **state)
{
    (void)state;
    start(&one_xfer);
    enumerate();

    assert_int_equal(cm_transport_read(buf, 4 * PACKET), PB_OK);
    usb_host_send(data, 2 * PACKET);
    assert_int_equal(cm_transport_complete(), -PB_ERR_AGAIN);

    host_get_descriptor(USB_DESC_TYPE_STRING, STR_SERIAL, 255);
    assert_int_equal(cm_transport_complete(), -PB_ERR_IO);
    assert_int_equal(usb_host_state()->bulk_starts, 1);
    assert_int_equal(usb_host_state()->clobbered, 0);

    /* A write too: the host may have read a part of it */
    start(&one_xfer);
    enumerate();

    assert_int_equal(cm_transport_write(buf, 4 * PACKET), PB_OK);
    host_get_descriptor(USB_DESC_TYPE_DEVICE, 0, 18);
    assert_int_equal(cm_transport_complete(), -PB_ERR_IO);
    assert_int_equal(usb_host_state()->bulk_starts, 1);
    assert_int_equal(usb_host_state()->clobbered, 0);
}

/* One transfer for the controller: a transfer that was done when the stack
 * saw the request isn't given up. The command the host sent just before its
 * request is read, and the request is answered */
static void test_request_after_the_transfer_with_one_xfer(void **state)
{
    (void)state;
    start(&one_xfer);
    enumerate();

    assert_int_equal(cm_transport_read(buf, PACKET), PB_OK);
    usb_host_send(data, PACKET);
    host_get_descriptor(USB_DESC_TYPE_STRING, STR_SERIAL, 255);
    assert_int_equal(cm_transport_complete(), PB_OK);
    assert_memory_equal(buf, data, PACKET);
    assert_answered(2 + 2 * strlen(DEVICE_UUID));
    assert_int_equal(usb_host_state()->cancel_calls, 0);
    assert_int_equal(usb_host_state()->bulk_starts, 1);

    /* Also one of several packets */
    assert_int_equal(cm_transport_read(buf, 4 * PACKET), PB_OK);
    usb_host_send(data, 4 * PACKET);
    host_get_descriptor(USB_DESC_TYPE_DEVICE, 0, 18);
    assert_int_equal(cm_transport_complete(), PB_OK);
    assert_memory_equal(buf, data, 4 * PACKET);
    assert_answered(18);
    assert_int_equal(usb_host_state()->clobbered, 0);
}

/* usb.md 2.4, 4.3: a bus reset that the controller reports as the error of
 * the transfer that ran is "lost" */
static void test_reported_bus_reset_is_lost(void **state)
{
    static const struct usb_host_config reports_reset = { .reports_reset = true };
    (void)state;

    start(&reports_reset);
    enumerate();

    assert_int_equal(cm_transport_read(buf, PACKET), PB_OK);
    assert_int_equal(cm_transport_complete(), -PB_ERR_AGAIN);

    usb_host_bus_reset();
    assert_int_equal(cm_transport_complete(), -PB_ERR_IO);
}

/* usb.md 2.1, 2.2, 3.2: a session that was lost leaves nothing behind. The
 * transport is started again and is ready when the host has configured the
 * device again, not at the host's first request (F-201: the next read was
 * then started on an endpoint that isn't set up, and the board reset) */
static void test_restart_after_lost_starts_over(void **state)
{
    static const struct usb_host_config reports_reset = { .reports_reset = true };
    (void)state;

    start(&reports_reset);
    enumerate();

    assert_int_equal(cm_transport_read(buf, PACKET), PB_OK);
    usb_host_bus_reset();
    assert_int_equal(cm_transport_complete(), -PB_ERR_IO);

    /* Command mode restarts the transport (command-mode.md 3.1) */
    assert_int_equal(cm_transport_init(), PB_OK);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);
    enumerate();

    assert_int_equal(cm_transport_read(buf, PACKET), PB_OK);
    usb_host_send(data, PACKET);
    assert_int_equal(cm_transport_complete(), PB_OK);
    assert_memory_equal(buf, data, PACKET);
}

/* usb.md 3.2: the stack keeps the configuration, and it is cleared when the
 * bus is reset or the controller is stopped: also without a restart, the
 * device isn't ready until the host configures it */
static void test_bus_reset_and_stop_clear_the_configuration(void **state)
{
    static const struct usb_host_config reports_reset = { .reports_reset = true };
    (void)state;

    start(&reports_reset);
    enumerate();

    assert_int_equal(cm_transport_read(buf, PACKET), PB_OK);
    usb_host_bus_reset();
    assert_int_equal(cm_transport_complete(), -PB_ERR_IO);
    enumerate();

    assert_int_equal(cm_transport_disconnect(), PB_OK);
    host_get_descriptor(USB_DESC_TYPE_DEVICE, 0, 64);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);
}

/* usb.md 2.4, 4.3: a replug or a bus reset inside a session is "lost", also
 * on a controller that handles the reset inside its driver and reports
 * nothing (the ChipIdea's and the cdns3's today). The host then enumerates
 * the device again, and a new address is how the stack sees it (F-363: it
 * answered, and the next host had the session of the one before, with its
 * authentication) */
static void test_replug_in_a_session_is_lost(void **state)
{
    (void)state;
    start(NULL);
    enumerate();

    assert_int_equal(cm_transport_read(buf, PACKET), PB_OK);
    assert_int_equal(cm_transport_complete(), -PB_ERR_AGAIN);

    usb_host_replug();
    assert_int_equal(cm_transport_complete(), -PB_ERR_AGAIN);

    /* A descriptor is read by any program at any time: no sign of it */
    host_get_descriptor(USB_DESC_TYPE_DEVICE, 0, 64);
    assert_int_equal(cm_transport_complete(), -PB_ERR_AGAIN);
    assert_answered(18);

    usb_host_bus_reset();
    host_set_address(7);
    assert_int_equal(cm_transport_complete(), -PB_ERR_IO);

    /* No answer: the address and the configuration are the next session's,
     * and the device leaves the bus when the transport is restarted (2.1) */
    assert_true(usb_host_reply()->taken);
    assert_false(usb_host_reply()->status);
    assert_int_equal(usb_host_state()->address, 0);
    assert_int_equal(usb_host_state()->set_address_calls, 1);

    /* Command mode restarts the transport, and the host starts over */
    assert_int_equal(cm_transport_init(), PB_OK);
    enumerate();
    assert_int_equal(cm_transport_read(buf, PACKET), PB_OK);
    usb_host_send(data, PACKET);
    assert_int_equal(cm_transport_complete(), PB_OK);
}

/* The same for a host that configures the device again, or deconfigures it
 * (usb.md 2.4, 3.2), and on a controller with one transfer for all
 * endpoints */
static void test_set_configuration_in_a_session_is_lost(void **state)
{
    (void)state;
    start(NULL);
    enumerate();

    assert_int_equal(cm_transport_read(buf, 4 * PACKET), PB_OK);
    usb_host_send(data, PACKET);
    host_set_configuration(1);
    assert_int_equal(cm_transport_complete(), -PB_ERR_IO);
    assert_false(usb_host_reply()->status);
    assert_int_equal(usb_host_state()->set_configuration_calls, 1);

    /* Nothing is configured after it (usb.md 3.2) */
    host_get_descriptor(USB_DESC_TYPE_DEVICE, 0, 64);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);

    start(NULL);
    enumerate();
    assert_int_equal(cm_transport_read(buf, PACKET), PB_OK);
    host_set_configuration(0);
    assert_int_equal(cm_transport_complete(), -PB_ERR_IO);

    start(&one_xfer);
    enumerate();
    assert_int_equal(cm_transport_read(buf, PACKET), PB_OK);
    host_set_address(7);
    assert_int_equal(cm_transport_complete(), -PB_ERR_IO);
    assert_int_equal(usb_host_state()->cancel_calls, 0);
    assert_int_equal(usb_host_state()->bulk_starts, 1);
}

/* usb.md 2.4: what happens while no transfer runs, inside a command, is kept
 * and is the error of the next transfer */
static void test_replug_between_transfers_is_lost(void **state)
{
    (void)state;
    start(NULL);
    enumerate();

    assert_int_equal(cm_transport_read(buf, PACKET), PB_OK);
    usb_host_send(data, PACKET);
    assert_int_equal(cm_transport_complete(), PB_OK);

    usb_host_replug();
    host_set_address(7);

    /* The command's result */
    assert_int_equal(cm_transport_write(buf, PACKET), -PB_ERR_STATE);

    start(NULL);
    enumerate();
    assert_int_equal(cm_transport_read(buf, PACKET), PB_OK);
    usb_host_send(data, PACKET);
    assert_int_equal(cm_transport_complete(), PB_OK);

    /* A host that enumerates without a reset the controller acts on */
    host_set_address(7);
    assert_int_equal(cm_transport_write(buf, PACKET), PB_OK);
    assert_int_equal(cm_transport_complete(), -PB_ERR_IO);
}

/* usb.md 2.3: the controller's refusal of a transfer is the transport's */
static void test_start_error(void **state)
{
    (void)state;
    start(NULL);
    enumerate();

    usb_host_fail_next_start(-PB_ERR_PARAM);
    assert_int_equal(cm_transport_read(buf, PACKET), -PB_ERR_PARAM);
    usb_host_fail_next_start(-PB_ERR_ALIGN);
    assert_int_equal(cm_transport_write(buf, PACKET), -PB_ERR_ALIGN);
}

/* usb.md 2.5: the device leaves the bus */
static void test_disconnect(void **state)
{
    (void)state;
    start(NULL);
    enumerate();

    assert_int_equal(cm_transport_disconnect(), PB_OK);
    assert_int_equal(usb_host_state()->stop_calls, 1);
    assert_false(usb_host_state()->attached);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_every_operation_is_mandatory),
        cmocka_unit_test(test_connect),
        cmocka_unit_test(test_set_configuration_value),
        cmocka_unit_test(test_descriptors),
        cmocka_unit_test(test_reply_lengths),
        cmocka_unit_test(test_strings),
        cmocka_unit_test(test_serial_number_without_an_identity),
        cmocka_unit_test(test_read),
        cmocka_unit_test(test_write),
        cmocka_unit_test(test_request_while_waiting_for_a_command),
        cmocka_unit_test(test_request_during_a_read),
        cmocka_unit_test(test_request_during_a_write),
        cmocka_unit_test(test_request_during_a_transfer_with_one_xfer),
        cmocka_unit_test(test_request_after_the_transfer_with_one_xfer),
        cmocka_unit_test(test_reported_bus_reset_is_lost),
        cmocka_unit_test(test_restart_after_lost_starts_over),
        cmocka_unit_test(test_bus_reset_and_stop_clear_the_configuration),
        cmocka_unit_test(test_replug_in_a_session_is_lost),
        cmocka_unit_test(test_set_configuration_in_a_session_is_lost),
        cmocka_unit_test(test_replug_between_transfers_is_lost),
        cmocka_unit_test(test_start_error),
        cmocka_unit_test(test_disconnect),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
