/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The Cadence USB3 device controller's driver (cdns3_udc_core.c) and its
 * i.MX glue (imx_cdns3_udc.c), built for the host: what it gives the
 * controller for a transfer, and that the bytes are the caller's when the
 * controller has run it. The controller is a
 * register file and the few lines of a model below, which follow a ring from
 * EP_TRADDR and move each TRB's bytes; nothing of the bus or of the
 * registers' timing. It reads and writes RAM behind the model of the D-cache
 * (fakes/dcache.h, drivers.md 4.2), so a clean or an invalidate that is
 * missing is a wrong byte, and a write of the controller's into memory the
 * driver didn't begin, or into a line the driver has dirty, fails the test.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/usb/cdns3_udc_core.h>
#include <drivers/usb/imx_cdns3_udc.h>
#include <pb/dma.h>
#include <pb/pb.h>
#include <string.h>

#include "fakes/dcache.h"
#include "fakes/plat.h"
#include "fakes/regs.h"

/* The MEK's controller */
#define CDNS3_BASE      0x5b120000u
#define USB_STS         (CDNS3_BASE + 0x20004)
#define USB_ISTS        (CDNS3_BASE + 0x20018)
#define EP_SEL          (CDNS3_BASE + 0x2001c)
#define EP_TRADDR       (CDNS3_BASE + 0x20020)
#define EP_CMD          (CDNS3_BASE + 0x20028)
#define EP_STS          (CDNS3_BASE + 0x2002c)
#define EP_ISTS         (CDNS3_BASE + 0x20040)
#define USB_STS_INRST   BIT(10)
#define EP_CMD_REQ_CMPL BIT(5)
#define EP_CMD_DRDY     BIT(6)
#define EP_CMD_DFLUSH   BIT(7)
#define EP_STS_SETUP    BIT(0)
#define EP_STS_IOC      BIT(2)
#define ISTS_DIS2I      BIT(17)
#define ISTS_U2RESI     BIT(18)

/* A TRB, and what the driver puts in one and in its ring (cdns3_udc_core.c) */
#define TRB_SIZE        12
#define TRB_MAX_LENGTH  (127 * 1024)
#define TRB_COUNT       128
/* The longest transfer: the ring's TRBs but its stop gap */
#define XFER_MAX        ((TRB_COUNT - 1) * TRB_MAX_LENGTH)
#define TRB_FLAG_CYCLE  BIT(0)

#define LINE            PB_DMA_ALIGN

/* A transfer's buffer that is only an address: one in the MEK's RAM, which
 * the driver queues and cleans and never reads */
#define NO_BUF          ((void *)(uintptr_t)0x90000000u)
#define NO_BUF_SIZE     ((size_t)TRB_COUNT * TRB_MAX_LENGTH)

/* The caller's buffers, which own their lines (usb.md 2.3), with a
 * neighbour on each side */
static uint8_t arena[3 * TRB_MAX_LENGTH + 2 * LINE] __aligned(LINE);
static uint8_t host[3 * TRB_MAX_LENGTH];

const char *ep_to_str(usb_ep_t ep)
{
    (void)ep;
    return "EP";
}

/* The glue's PHY, and the stack it registers with */
int imx_salvo_phy_init(uintptr_t base)
{
    (void)base;
    return PB_OK;
}

static const struct usbd_hal_ops *hal_ops;

int usbd_init_hal_ops(const struct usbd_hal_ops *ops)
{
    hal_ops = ops;
    return PB_OK;
}

/* The MEK's glue, and its status bit */
#define NON_CORE_BASE      0x5b110000u
#define PHY_BASE           0x5b160000u
#define USB3_CORE_STATUS   (NON_CORE_BASE + 0x0c)
#define DEV_POWER_ON_READY BIT(13)

/* EP_TRADDR is the selected endpoint's: an index for each direction */
static uint32_t traddr[32];
static unsigned int selected;
/* A flush, or a status stage, that doesn't end */
static bool flush_hangs;
static bool status_stage_hangs;

/* What the registers do with a write: the status registers are written to
 * clear, and a flush and a status stage end at once */
static uint64_t on_write(uintptr_t addr, uint64_t value)
{
    uint64_t sticky = (flush_hangs ? EP_CMD_DFLUSH : 0) |
                      (status_stage_hangs ? EP_CMD_REQ_CMPL : 0);

    switch (addr) {
    case EP_SEL:
        selected = (unsigned int)(((value & 0xf) << 1) | ((value >> 7) & 1));
        return value;
    case EP_TRADDR:
        traddr[selected] = (uint32_t)value;
        return value;
    case USB_ISTS:
    case EP_ISTS:
    case EP_STS:
        return 0;
    case EP_CMD:
        return value & ~(uint64_t)((EP_CMD_DFLUSH | EP_CMD_REQ_CMPL) & ~sticky);
    default:
        return value;
    }
}

/* The controller runs the ring of 'ep': 'stream' is what the host sends on
 * an OUT endpoint, and what it receives on an IN endpoint. It stops at the
 * first TRB whose cycle bit is clear. Returns the bytes that moved */
static size_t controller_run(usb_ep_t ep, uint8_t *stream, size_t size)
{
    uintptr_t trb_addr = traddr[ep];
    bool in = (ep % 2) != 0;
    size_t pos = 0;

    assert_true(trb_addr != 0);

    for (size_t n = 0; n <= TRB_COUNT; n++) {
        uint32_t trb[3];
        size_t length;

        dcache_dev_read(trb_addr + n * TRB_SIZE, trb, sizeof(trb));

        if (!(trb[2] & TRB_FLAG_CYCLE))
            return pos;

        length = trb[1] & 0x1ffff;
        assert_true(pos + length <= size);

        if (in)
            dcache_dev_read(trb[0], stream + pos, length);
        else
            dcache_dev_write(trb[0], stream + pos, length);

        pos += length;
    }

    fail_msg("A ring without an end");
    return 0;
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
    dcache_no_memory((uintptr_t)NO_BUF, NO_BUF_SIZE);
    return 0;
}

/* The controller is set up: the setup packet's transfer is queued on
 * endpoint 0, and of the driver's memory the controller has that ring and
 * the packet's buffer */
static int setup(void **state)
{
    (void)state;

    fake_regs_reset();
    fake_regs_on_write = on_write;
    flush_hangs = false;
    status_stage_hangs = false;
    /* Time goes on while the driver polls */
    fake_us_tick_step = 1;
    fake_regs_set(USB_STS, USB_STS_INRST);
    cdns3_udc_core_set_base(CDNS3_BASE);
    assert_int_equal(cdns3_udc_core_init(), PB_OK);
    assert_int_equal(dma_host_in_open(), 2);
    return 0;
}

/* drivers.md 4.2, item 1: everything the device follows is cleaned before it
 * is started, a descriptor list with its terminating entry. The controller
 * stops at the first TRB whose cycle bit is clear, which the driver writes
 * after the transfer's last one (F-184: the clean left it out, and for some
 * counts its flags are in a line of their own, where memory still had the
 * TRB of a longer transfer before). The model's controller reads the ring
 * from RAM, and fails on a line that wasn't cleaned */
static void test_the_terminating_trb_is_cleaned(void **state)
{
    (void)state;

    /* The longest transfer first: after it every TRB has its cycle bit in
     * RAM, and a shorter one's terminating TRB must get there */
    assert_int_equal(
        cdns3_udc_core_xfer_start(USB_EP2_OUT, NO_BUF, (TRB_COUNT - 1) * TRB_MAX_LENGTH), PB_OK);

    for (size_t count = 1; count < TRB_COUNT; count++) {
        uint32_t trb[3];
        uintptr_t ring;

        assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, NO_BUF, count * TRB_MAX_LENGTH),
                         PB_OK);
        assert_int_equal(fake_regs_get(EP_CMD), EP_CMD_DRDY);

        /* The ring: 'count' TRBs with the cycle bit, then one without */
        ring = traddr[USB_EP2_OUT];
        assert_true(ring != 0);

        for (size_t i = 0; i < count; i++) {
            dcache_dev_read(ring + i * TRB_SIZE, trb, sizeof(trb));
            assert_true(trb[2] & TRB_FLAG_CYCLE);
        }

        dcache_dev_read(ring + count * TRB_SIZE, trb, sizeof(trb));
        assert_int_equal(trb[2], 0);
    }

    cdns3_udc_core_xfer_cancel(USB_EP2_OUT);
    assert_int_equal(dma_host_in_open(), 2);
}

/* usb.md 4.3: a length above what the driver can queue is -PB_ERR_PARAM,
 * and nothing is written. A transfer of 128 TRBs wrote its stop gap past
 * the 128 of the ring (F-205): the longest is a TRB shorter, with the stop
 * gap the ring's last */
static void test_a_transfer_longer_than_the_ring_is_refused(void **state)
{
    uint32_t trb[3];
    uintptr_t ring;
    size_t writes;
    (void)state;

    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, NO_BUF, XFER_MAX), PB_OK);
    ring = traddr[USB_EP2_OUT];
    dcache_dev_read(ring + (TRB_COUNT - 2) * TRB_SIZE, trb, sizeof(trb));
    assert_true(trb[2] & TRB_FLAG_CYCLE);
    dcache_dev_read(ring + (TRB_COUNT - 1) * TRB_SIZE, trb, sizeof(trb));
    assert_int_equal(trb[2], 0);
    cdns3_udc_core_xfer_cancel(USB_EP2_OUT);

    writes = fake_reg_log_len;
    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, NO_BUF, XFER_MAX + 1), -PB_ERR_PARAM);
    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP1_IN, NO_BUF, TRB_COUNT * TRB_MAX_LENGTH),
                     -PB_ERR_PARAM);
    assert_int_equal(fake_reg_log_len, writes);
    assert_int_equal(dma_host_in_open(), 2);
}

/* Endpoint 0's reply goes through the driver's bounce buffer of 512 bytes,
 * which a longer one overflowed (F-205): it is refused, -PB_ERR_PARAM */
static void test_an_ep0_reply_longer_than_the_bounce_buffer_is_refused(void **state)
{
    static uint8_t reply[513];
    (void)state;

    fill(reply, sizeof(reply), 3);
    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP0_IN, reply, sizeof(reply)), -PB_ERR_PARAM);
    assert_int_equal(dma_host_in_open(), 2);

    memset(host, 0, 512);
    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP0_IN, reply, 512), PB_OK);
    assert_int_equal(controller_run(USB_EP0_IN, host, 512), 512);
    fake_regs_set(EP_STS, EP_STS_IOC);
    assert_int_equal(cdns3_udc_core_xfer_complete(USB_EP0_IN), PB_OK);
    assert_memory_equal(host, reply, 512);
}

/* drivers.md 3: every wait is bounded and returns -PB_ERR_TIMEOUT. The
 * controller's reset that doesn't end fails init; init again when it does */
static void test_the_reset_wait_times_out(void **state)
{
    (void)state;

    fake_regs_set(USB_STS, 0);
    assert_int_equal(cdns3_udc_core_init(), -PB_ERR_TIMEOUT);

    fake_regs_set(USB_STS, USB_STS_INRST);
    assert_int_equal(cdns3_udc_core_init(), PB_OK);
    assert_int_equal(dma_host_in_open(), 2);
}

/* The status stage that the host never takes: -PB_ERR_TIMEOUT */
static void test_the_status_stage_wait_times_out(void **state)
{
    (void)state;

    status_stage_hangs = true;
    assert_int_equal(cdns3_udc_core_xfer_zlp(USB_EP0_IN), -PB_ERR_TIMEOUT);

    status_stage_hangs = false;
    assert_int_equal(cdns3_udc_core_xfer_zlp(USB_EP0_IN), PB_OK);
}

/* A flush that doesn't end: the cancel can't return it, so the transfer
 * stays the controller's and the next one is -PB_ERR_TIMEOUT, until init
 * resets the controller (drivers.md 3: nothing of the failed one completes
 * later) */
static void test_the_flush_wait_times_out(void **state)
{
    uint8_t *buf = arena + LINE;
    (void)state;

    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, buf, 512), PB_OK);
    flush_hangs = true;
    cdns3_udc_core_xfer_cancel(USB_EP2_OUT);
    assert_int_equal(dma_host_in_open(), 4);
    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, buf, 512), -PB_ERR_TIMEOUT);
    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP1_IN, buf, 512), -PB_ERR_TIMEOUT);

    flush_hangs = false;
    assert_int_equal(cdns3_udc_core_init(), PB_OK);
    assert_int_equal(dma_host_in_open(), 2);
    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, buf, 512), PB_OK);
    cdns3_udc_core_xfer_cancel(USB_EP2_OUT);
    assert_int_equal(dma_host_in_open(), 2);
}

/* F-281, drivers.md 3: the glue's wait for the core's power-on-ready is
 * bounded, -PB_ERR_TIMEOUT from the operation init; when the bit comes, the
 * core is set up */
static void test_the_power_on_ready_wait_times_out(void **state)
{
    static const struct imx_cdns3_udc_config cfg = {
        .base = CDNS3_BASE,
        .non_core_base = NON_CORE_BASE,
        .phy_base = PHY_BASE,
    };
    (void)state;

    hal_ops = NULL;
    assert_int_equal(imx_cdns3_udc_init(&cfg), PB_OK);
    assert_non_null(hal_ops);

    fake_regs_set(USB3_CORE_STATUS, 0);
    assert_int_equal(hal_ops->init(), -PB_ERR_TIMEOUT);

    fake_regs_set(USB3_CORE_STATUS, DEV_POWER_ON_READY);
    assert_int_equal(hal_ops->init(), PB_OK);
    assert_int_equal(dma_host_in_open(), 2);
}

/* The host configures the device: pb's two bulk endpoints */
static void configure(void)
{
    static const struct usb_endpoint_descriptor eps[] = {
        { .bLength = 7,
          .bDescriptorType = 5,
          .bEndpointAddress = 0x81,
          .bmAttributes = 2,
          .wMaxPacketSize = 512 },
        { .bLength = 7,
          .bDescriptorType = 5,
          .bEndpointAddress = 0x02,
          .bmAttributes = 2,
          .wMaxPacketSize = 512 },
    };

    assert_int_equal(cdns3_udc_set_configuration(eps, ARRAY_SIZE(eps)), PB_OK);
}

/* The controller reports a bus event, which the driver sees when the stack
 * polls endpoint 0 */
static void bus_event(uint32_t ists)
{
    struct usb_setup_packet pkt;

    fake_regs_set(USB_ISTS, ists);
    assert_int_equal(cdns3_udc_core_poll_setup_pkt(&pkt), -PB_ERR_AGAIN);
}

/* F-363, usb.md 2.4, 4.3: a bus reset or an unplug of a configured device
 * ends the running bulk transfer with -PB_ERR_IO, every time it is polled,
 * and the next one that is started. Endpoint 0 goes on: the host
 * enumerates again. Before the device is configured it is the host's
 * enumeration and no error (2.2) */
static void test_a_bus_reset_of_a_configured_device_is_lost(void **state)
{
    static const uint32_t events[] = { ISTS_U2RESI, ISTS_DIS2I };
    uint8_t *buf = arena + LINE;
    uint8_t reply[18];
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(events); i++) {
        assert_int_equal(cdns3_udc_core_init(), PB_OK);

        /* Enumeration */
        bus_event(events[i]);
        configure();

        assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, buf, 512), PB_OK);
        bus_event(0);
        assert_int_equal(cdns3_udc_core_xfer_complete(USB_EP2_OUT), -PB_ERR_AGAIN);

        bus_event(events[i]);
        assert_int_equal(cdns3_udc_core_xfer_complete(USB_EP2_OUT), -PB_ERR_IO);
        assert_int_equal(cdns3_udc_core_xfer_complete(USB_EP2_OUT), -PB_ERR_IO);
        assert_int_equal(cdns3_udc_core_xfer_start(USB_EP1_IN, buf, 512), -PB_ERR_IO);
        assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, buf, 512), -PB_ERR_IO);

        /* The host's new enumeration is answered. The lost transfer's
         * buffer is the transport's until it is restarted (usb.md 2.3) */
        memset(host, 0, sizeof(reply));
        fill(reply, sizeof(reply), 5);
        assert_int_equal(cdns3_udc_core_xfer_start(USB_EP0_IN, reply, sizeof(reply)), PB_OK);
        assert_int_equal(controller_run(USB_EP0_IN, host, sizeof(reply)), sizeof(reply));
        fake_regs_set(EP_STS, EP_STS_IOC);
        assert_int_equal(cdns3_udc_core_xfer_complete(USB_EP0_IN), PB_OK);
        assert_memory_equal(host, reply, sizeof(reply));

        /* The transport is restarted: init takes the transfer back */
        assert_int_equal(cdns3_udc_core_init(), PB_OK);
        assert_int_equal(dma_host_in_open(), 2);
        assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, buf, 512), PB_OK);
        cdns3_udc_core_xfer_cancel(USB_EP2_OUT);
    }
}

/* A bus reset while no bulk transfer runs, inside a command, is kept: the
 * next transfer's error. A new configuration, init and stop forget it */
static void test_a_bus_reset_between_transfers_is_kept(void **state)
{
    uint8_t *buf = arena + LINE;
    (void)state;

    configure();
    bus_event(ISTS_U2RESI);
    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, buf, 512), -PB_ERR_IO);
    configure();
    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, buf, 512), PB_OK);
    cdns3_udc_core_xfer_cancel(USB_EP2_OUT);

    bus_event(ISTS_U2RESI);
    assert_int_equal(cdns3_udc_core_stop(), PB_OK);
    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, buf, 512), PB_OK);
    cdns3_udc_core_xfer_cancel(USB_EP2_OUT);

    configure();
    bus_event(ISTS_DIS2I);
    assert_int_equal(cdns3_udc_core_init(), PB_OK);
    /* Not configured since: a reset is enumeration again */
    bus_event(ISTS_U2RESI);
    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, buf, 512), PB_OK);
    cdns3_udc_core_xfer_cancel(USB_EP2_OUT);
    assert_int_equal(dma_host_in_open(), 2);
}

/* usb.md 2.3, 4.3: a transfer from the host arrives in the caller's buffer,
 * all of it and nothing outside the buffer's lines, and a transfer to the
 * host is what the caller wrote. Over one TRB and over several */
static void test_bulk_transfers(void **state)
{
    static const size_t lengths[] = {
        1, 64, 512, 4096, TRB_MAX_LENGTH, TRB_MAX_LENGTH + 1, 3 * TRB_MAX_LENGTH,
    };
    uint8_t *buf = arena + LINE;
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(lengths); i++) {
        size_t length = lengths[i];
        size_t whole_lines = PB_DMA_LINES(length);

        /* From the host */
        fill(host, length, (unsigned int)length);
        memset(arena, 0xee, sizeof(arena));
        assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, buf, length), PB_OK);
        assert_int_equal(cdns3_udc_core_xfer_complete(USB_EP2_OUT), -PB_ERR_AGAIN);
        assert_int_equal(controller_run(USB_EP2_OUT, host, length), length);
        fake_regs_set(EP_STS, EP_STS_IOC);
        assert_int_equal(cdns3_udc_core_xfer_complete(USB_EP2_OUT), PB_OK);
        assert_memory_equal(buf, host, length);
        assert_int_equal(dma_host_in_open(), 2);

        for (size_t n = 0; n < sizeof(arena); n++) {
            if (n >= LINE && n < LINE + whole_lines)
                continue;
            if (arena[n] != 0xee)
                fail_msg("OUT, %zu bytes: byte %zu of the arena was written", length, n);
        }

        /* To the host, what the caller has just written */
        fill(buf, length, (unsigned int)length + 1);
        memset(host, 0, length);
        assert_int_equal(cdns3_udc_core_xfer_start(USB_EP1_IN, buf, length), PB_OK);
        assert_int_equal(controller_run(USB_EP1_IN, host, length), length);
        fake_regs_set(EP_STS, EP_STS_IOC);
        assert_int_equal(cdns3_udc_core_xfer_complete(USB_EP1_IN), PB_OK);
        assert_memory_equal(buf, host, length);
        assert_int_equal(dma_host_in_open(), 2);
    }
}

/* F-434: an endpoint stays after its transfer's last TRB, and with
 * CONF_DMULT it takes a TRB written there later as its own. After a result
 * on IN, the next OUT transfer's TRBs are not where the IN endpoint is: with
 * one ring for both, its second TRB went out on IN instead of the result */
static void test_in_and_out_have_their_own_rings(void **state)
{
    uint8_t *buf = arena + LINE;
    uint8_t result[512];
    uint32_t trb[3];
    (void)state;

    fill(buf, sizeof(result), 1);
    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP1_IN, buf, sizeof(result)), PB_OK);
    assert_int_equal(controller_run(USB_EP1_IN, result, sizeof(result)), sizeof(result));
    fake_regs_set(EP_STS, EP_STS_IOC);
    assert_int_equal(cdns3_udc_core_xfer_complete(USB_EP1_IN), PB_OK);

    /* Where the IN endpoint is now: the TRB after its result's */
    uintptr_t in_at = traddr[USB_EP1_IN] + TRB_SIZE;

    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, buf, 3 * TRB_MAX_LENGTH), PB_OK);
    assert_true(traddr[USB_EP2_OUT] != traddr[USB_EP1_IN]);

    dcache_dev_read(in_at, trb, sizeof(trb));
    assert_false(trb[2] & TRB_FLAG_CYCLE);

    cdns3_udc_core_xfer_cancel(USB_EP2_OUT);
    assert_int_equal(dma_host_in_open(), 2);
}

/* usb.md 4.3: a transfer that is given up, or that the next one replaces,
 * leaves its buffer with the caller */
static void test_cancel_gives_the_buffer_back(void **state)
{
    uint8_t *buf = arena + LINE;
    (void)state;

    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, buf, 512), PB_OK);
    /* The ring and the buffer, with endpoint 0's two */
    assert_int_equal(dma_host_in_open(), 4);
    cdns3_udc_core_xfer_cancel(USB_EP2_OUT);
    assert_int_equal(dma_host_in_open(), 2);

    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, buf, 512), PB_OK);
    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, buf, 1024), PB_OK);
    assert_int_equal(dma_host_in_open(), 4);
    cdns3_udc_core_xfer_cancel(USB_EP2_OUT);
}

/* usb.md 4.4, drivers.md 4.2, 3 and 5: the controller writes a setup packet
 * into the driver's buffer when the host sends one, and the driver reads it
 * from RAM. The buffer owns its line and is the controller's while the
 * packet's transfer is queued (F-205: it was 8 bytes at the start of a
 * line, with whatever the linker put behind it, and every SETUP invalidated
 * the line): the model fails a write of the controller's into memory that
 * isn't begun, or into a line the driver has dirty. Then the reply, through
 * the driver's bounce buffer, and the next packet */
static void test_setup_packet(void **state)
{
    static const struct usb_setup_packet requests[] = {
        { .bRequestType = 0x80, .bRequest = 6, .wValue = 0x0100, .wLength = 18 },
        { .bRequestType = 0x80, .bRequest = 6, .wValue = 0x0200, .wLength = 32 },
    };
    struct usb_setup_packet pkt;
    uint8_t reply[32];
    (void)state;

    assert_int_equal(cdns3_udc_core_poll_setup_pkt(&pkt), -PB_ERR_AGAIN);

    for (size_t i = 0; i < ARRAY_SIZE(requests); i++) {
        uint8_t packet[sizeof(pkt)];

        /* The setup packet's transfer is one TRB with the driver's buffer */
        memcpy(packet, &requests[i], sizeof(packet));
        assert_int_equal(controller_run(USB_EP0_OUT, packet, sizeof(packet)), sizeof(packet));
        fake_regs_set(EP_ISTS, 1);
        fake_regs_set(EP_STS, EP_STS_SETUP | EP_STS_IOC);

        memset(&pkt, 0, sizeof(pkt));
        assert_int_equal(cdns3_udc_core_poll_setup_pkt(&pkt), PB_OK);
        assert_memory_equal(&pkt, &requests[i], sizeof(pkt));
        assert_int_equal(cdns3_udc_core_poll_setup_pkt(&pkt), -PB_ERR_AGAIN);

        /* The data stage */
        fill(reply, sizeof(reply), (unsigned int)i);
        memset(host, 0, sizeof(reply));
        assert_int_equal(cdns3_udc_core_xfer_start(USB_EP0_IN, reply, requests[i].wLength), PB_OK);
        assert_int_equal(controller_run(USB_EP0_IN, host, sizeof(reply)), requests[i].wLength);
        fake_regs_set(EP_STS, EP_STS_IOC);
        assert_int_equal(cdns3_udc_core_xfer_complete(USB_EP0_IN), PB_OK);
        assert_memory_equal(host, reply, requests[i].wLength);

        /* Again the setup packet's ring and buffer, and nothing else */
        assert_int_equal(dma_host_in_open(), 2);
    }
}

/* The transport is restarted (usb.md 4.1): the controller was reset, and
 * what it had is taken back before it is set up again */
static void test_init_again(void **state)
{
    (void)state;

    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, arena + LINE, 512), PB_OK);
    assert_int_equal(cdns3_udc_core_xfer_start(USB_EP0_IN, host, 18), PB_OK);
    assert_int_equal(dma_host_in_open(), 5);
    assert_int_equal(cdns3_udc_core_init(), PB_OK);
    assert_int_equal(dma_host_in_open(), 2);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_the_terminating_trb_is_cleaned, setup),
        cmocka_unit_test_setup(test_bulk_transfers, setup),
        cmocka_unit_test_setup(test_in_and_out_have_their_own_rings, setup),
        cmocka_unit_test_setup(test_cancel_gives_the_buffer_back, setup),
        cmocka_unit_test_setup(test_setup_packet, setup),
        cmocka_unit_test_setup(test_init_again, setup),
        cmocka_unit_test_setup(test_a_transfer_longer_than_the_ring_is_refused, setup),
        cmocka_unit_test_setup(test_an_ep0_reply_longer_than_the_bounce_buffer_is_refused, setup),
        cmocka_unit_test_setup(test_the_reset_wait_times_out, setup),
        cmocka_unit_test_setup(test_the_status_stage_wait_times_out, setup),
        cmocka_unit_test_setup(test_the_flush_wait_times_out, setup),
        cmocka_unit_test_setup(test_the_power_on_ready_wait_times_out, setup),
        cmocka_unit_test_setup(test_a_bus_reset_of_a_configured_device_is_lost, setup),
        cmocka_unit_test_setup(test_a_bus_reset_between_transfers_is_kept, setup),
    };

    return cmocka_run_group_tests(tests, group_setup, NULL);
}
