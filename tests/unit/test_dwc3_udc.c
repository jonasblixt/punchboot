/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The DWC3 device controller's driver (dwc3_udc.c), built for the host: what
 * it gives the controller of pb's memory, and that the bytes are the
 * caller's when the controller has run a transfer (drivers.md 4, usb.md 4.3
 * and 4.4). The controller is a register file and a few lines of a model:
 * a command ends at once, a started transfer is one TRB whose bytes the
 * test moves, and an event is a word in the event buffer. Nothing of the
 * bus, of the control transfer's stages beyond the setup packet, or of the
 * registers' timing: that is the bench's (usb.md 8).
 *
 * The model reads and writes RAM behind the model of the D-cache
 * (fakes/dcache.h): a clean or an invalidate that is missing is a wrong
 * byte, and a write of the controller's into memory the driver didn't
 * begin, or into a line the driver has dirty, fails the test.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/usb/dwc3_udc.h>
#include <drivers/usb/usbd.h>
#include <pb/dma.h>
#include <pb/pb.h>
#include <string.h>

#include "dwc3_udc_private.h"
#include "fakes/dcache.h"
#include "fakes/plat.h"
#include "fakes/regs.h"

/* The 8MP EVK's controller */
#define BASE       0x38100000u
#define EV_ENTRIES 64
#define LINE       PB_DMA_ALIGN

#define EV_EP(ep, type) \
    (((uint32_t)(ep) << DEPEVT_EP_SHIFT) | ((uint32_t)(type) << DEPEVT_TYPE_SHIFT))

/* Of the driver's memory a controller that runs has the event buffer, and
 * with the Setup TRB started that TRB and the setup packet */
#define IDLE_RANGES 3

static const struct usbd_hal_ops *ops;

/* The TRB each endpoint was started with, and where the next event goes */
static uint64_t trb_addr[USB_EP_END];
static unsigned int ev_write;
static uint32_t ev_pending; /* Bytes of events the driver hasn't handled */

/* The caller's buffers, which own their lines (usb.md 2.3), with a
 * neighbour on each side */
static uint8_t arena[4096 + 2 * LINE] __aligned(LINE);
static uint8_t host[4096];

int usbd_init_hal_ops(const struct usbd_hal_ops *ops_)
{
    ops = ops_;
    return PB_OK;
}

const char *ep_to_str(usb_ep_t ep)
{
    (void)ep;
    return "EP";
}

/* The controller writes an event and counts it */
static void post_event(uint32_t ev)
{
    uintptr_t buffer = (uintptr_t)fake_regs_get(BASE + DWC3_GEVNTADRLO);

    dcache_dev_write_32(buffer + 4 * ev_write, ev);
    ev_write = (ev_write + 1) % EV_ENTRIES;
    ev_pending += 4;
    fake_regs_set(BASE + DWC3_GEVNTCOUNT, ev_pending);
}

/* What the registers do with a write: a soft reset and a command end at
 * once, a Start Transfer names the endpoint's TRB, an End Transfer that
 * asks for it gets its event, and GEVNTCOUNT counts down */
static uint64_t on_write(uintptr_t addr, uint64_t value)
{
    uintptr_t off = addr - BASE;

    if (off == DWC3_DCTL)
        return value & ~(uint64_t)DCTL_CSFTRST;

    if (off == DWC3_GEVNTCOUNT) {
        /* The driver's write: this many bytes are handled */
        assert_true(value <= ev_pending);
        ev_pending -= (uint32_t)value;
        return ev_pending;
    }

    for (usb_ep_t ep = USB_EP0_OUT; ep < USB_EP_END; ep++) {
        if (off != DWC3_DEPCMD(ep))
            continue;

        if ((value & DEPCMD_CMDTYP_MASK) == DEPCMD_DEPSTRTXFER) {
            trb_addr[ep] = (fake_regs_get(BASE + DWC3_DEPCMDPAR0(ep)) << 32) |
                           fake_regs_get(BASE + DWC3_DEPCMDPAR1(ep));
        } else if ((value & DEPCMD_CMDTYP_MASK) == DEPCMD_DEPENDXFER && (value & DEPCMD_CMDIOC)) {
            post_event(EV_EP(ep, DEPEVT_EPCMDCMPLT));
        }

        return value & ~(uint64_t)DEPCMD_CMDACT;
    }

    return value;
}

/* The controller runs the transfer that was started on 'ep': 'stream' is
 * what the host sends on an OUT endpoint, and what it receives on an IN
 * endpoint. It gives the TRB back and reports the transfer complete */
static void controller_run(usb_ep_t ep, uint8_t *stream, size_t length)
{
    uint32_t trb[4];
    uint64_t buf;

    assert_true(trb_addr[ep] != 0);
    dcache_dev_read((uintptr_t)trb_addr[ep], trb, sizeof(trb));
    assert_true(trb[3] & TRB_CTRL_HWO);
    assert_true((trb[2] & TRB_SIZE_BUFSIZ_MASK) >= length);
    buf = ((uint64_t)trb[1] << 32) | trb[0];

    if (ep & 1)
        dcache_dev_read((uintptr_t)buf, stream, length);
    else
        dcache_dev_write((uintptr_t)buf, stream, length);

    trb[2] -= (uint32_t)length;
    trb[3] &= ~TRB_CTRL_HWO;
    dcache_dev_write((uintptr_t)trb_addr[ep], trb, sizeof(trb));
    post_event(EV_EP(ep, DEPEVT_XFERCOMPLETE));
}

static void fill(uint8_t *p, size_t length, unsigned int seed)
{
    for (size_t i = 0; i < length; i++)
        p[i] = (uint8_t)(seed + i * 13 + (i >> 8) * 7);
}

/* The driver keeps what it gave the controller between the tests, and so
 * does the cache's model */
static int group_setup(void **state)
{
    (void)state;

    dcache_reset();
    return 0;
}

static int setup(void **state)
{
    (void)state;

    fake_regs_reset();
    fake_regs_on_write = on_write;
    /* Halted when the driver stops it */
    fake_regs_set(BASE + DWC3_DSTS, DSTS_DEVCTRLHLT);
    fake_us_tick_step = 1;
    memset(trb_addr, 0, sizeof(trb_addr));
    ev_write = 0;
    ev_pending = 0;
    ops = NULL;

    assert_int_equal(imx_dwc3_udc_init(BASE), PB_OK);
    assert_non_null(ops);
    assert_int_equal(ops->init(), PB_OK);
    assert_int_equal(dma_host_in_open(), IDLE_RANGES);
    return 0;
}

static int teardown(void **state)
{
    (void)state;

    fake_us_tick_step = 0;
    return 0;
}

static void configure(void)
{
    static const struct usb_endpoint_descriptor eps[] = {
        { .bLength = 7,
          .bDescriptorType = USB_DESC_TYPE_ENDPOINT,
          .bEndpointAddress = USB_EP_ADDR_IN | 1,
          .bmAttributes = USB_EP_ATTR_BULK,
          .wMaxPacketSize = 512 },
        { .bLength = 7,
          .bDescriptorType = USB_DESC_TYPE_ENDPOINT,
          .bEndpointAddress = 2,
          .bmAttributes = USB_EP_ATTR_BULK,
          .wMaxPacketSize = 512 },
    };

    assert_int_equal(ops->set_configuration(eps, ARRAY_SIZE(eps)), PB_OK);
}

/* usb.md 4.4, drivers.md 4.2, 3 and 5: the controller writes a setup packet
 * into the driver's buffer when the host sends one, and the driver reads it
 * from RAM. The buffer owns its line and is the controller's while the
 * Setup TRB is started (it was 8 bytes at the start of a line, with
 * whatever the linker put behind them, and every SETUP invalidated the
 * line). The events are read from RAM too */
static void test_setup_packet(void **state)
{
    static const struct usb_setup_packet requests[] = {
        { .bRequestType = 0x80, .bRequest = 6, .wValue = 0x0100, .wLength = 18 },
        { .bRequestType = 0x00, .bRequest = 9, .wValue = 0x0001 },
    };
    struct usb_setup_packet pkt;
    (void)state;

    assert_int_equal(ops->poll_setup_pkt(&pkt), -PB_ERR_AGAIN);

    for (size_t i = 0; i < ARRAY_SIZE(requests); i++) {
        uint8_t packet[sizeof(pkt)];

        memcpy(packet, &requests[i], sizeof(packet));
        memset(&pkt, 0, sizeof(pkt));
        controller_run(USB_EP0_OUT, packet, sizeof(packet));
        assert_int_equal(ops->poll_setup_pkt(&pkt), PB_OK);
        assert_memory_equal(&pkt, &requests[i], sizeof(pkt));

        /* The packet is the driver's until the Setup TRB is started again */
        assert_int_equal(dma_host_in_open(), IDLE_RANGES - 2);

        /* A request nobody answers is stalled, and the next SETUP is
         * waited for */
        assert_int_equal(ops->poll_setup_pkt(&pkt), -PB_ERR_AGAIN);
        assert_int_equal(dma_host_in_open(), IDLE_RANGES);
    }
}

/* usb.md 2.3, 4.3: a transfer from the host arrives in the caller's buffer,
 * all of it and nothing outside the buffer's lines, and a transfer to the
 * host is what the caller has just written */
static void test_bulk_transfers(void **state)
{
    static const size_t lengths[] = { 1, 64, 512, 513, 1024, 4096 };
    uint8_t *buf = arena + LINE;
    (void)state;

    configure();

    for (size_t i = 0; i < ARRAY_SIZE(lengths); i++) {
        size_t length = lengths[i];
        size_t whole_lines = PB_DMA_LINES(length);

        /* From the host */
        fill(host, length, (unsigned int)length);
        memset(arena, 0xee, sizeof(arena));
        assert_int_equal(ops->xfer_start(USB_EP2_OUT, buf, length), PB_OK);
        assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_AGAIN);
        /* The TRB and the buffer */
        assert_int_equal(dma_host_in_open(), IDLE_RANGES + 2);
        controller_run(USB_EP2_OUT, host, length);
        assert_int_equal(ops->xfer_complete(USB_EP2_OUT), PB_OK);
        assert_memory_equal(buf, host, length);
        assert_int_equal(dma_host_in_open(), IDLE_RANGES);

        for (size_t n = 0; n < sizeof(arena); n++) {
            if (n >= LINE && n < LINE + whole_lines)
                continue;
            if (arena[n] != 0xee)
                fail_msg("OUT, %zu bytes: byte %zu of the arena was written", length, n);
        }

        /* To the host */
        fill(buf, length, (unsigned int)length + 1);
        memset(host, 0, length);
        assert_int_equal(ops->xfer_start(USB_EP1_IN, buf, length), PB_OK);
        controller_run(USB_EP1_IN, host, length);
        assert_int_equal(ops->xfer_complete(USB_EP1_IN), PB_OK);
        assert_memory_equal(buf, host, length);
        assert_int_equal(dma_host_in_open(), IDLE_RANGES);
    }
}

/* usb.md 4.3: a transfer that is given up, or that the next one on its
 * endpoint replaces, leaves its buffer with the caller */
static void test_cancel_gives_the_buffer_back(void **state)
{
    uint8_t *buf = arena + LINE;
    (void)state;

    configure();

    assert_int_equal(ops->xfer_start(USB_EP2_OUT, buf, 512), PB_OK);
    assert_int_equal(dma_host_in_open(), IDLE_RANGES + 2);
    ops->xfer_cancel(USB_EP2_OUT);
    assert_int_equal(dma_host_in_open(), IDLE_RANGES);

    /* And a transfer after it works */
    fill(host, 512, 9);
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, buf, 512), PB_OK);
    controller_run(USB_EP2_OUT, host, 512);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), PB_OK);
    assert_memory_equal(buf, host, 512);
}

/* drivers.md 2.3: a stopped controller has nothing of pb's memory, and it
 * can be set up again, also without a stop (usb.md 4.1) */
static void test_stop_and_init_again(void **state)
{
    (void)state;

    configure();
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena + LINE, 512), PB_OK);
    assert_int_equal(ops->stop(), PB_OK);
    assert_int_equal(dma_host_in_open(), 0);

    ev_write = 0;
    assert_int_equal(ops->init(), PB_OK);
    assert_int_equal(dma_host_in_open(), IDLE_RANGES);

    configure();
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena + LINE, 512), PB_OK);
    ev_write = 0;
    assert_int_equal(ops->init(), PB_OK);
    assert_int_equal(dma_host_in_open(), IDLE_RANGES);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_setup_packet, setup, teardown),
        cmocka_unit_test_setup_teardown(test_bulk_transfers, setup, teardown),
        cmocka_unit_test_setup_teardown(test_cancel_gives_the_buffer_back, setup, teardown),
        cmocka_unit_test_setup_teardown(test_stop_and_init_again, setup, teardown),
    };

    return cmocka_run_group_tests(tests, group_setup, NULL);
}
