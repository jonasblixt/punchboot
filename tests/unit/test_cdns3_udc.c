/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The Cadence USB3 device controller's driver (cdns3_udc_core.c), built for
 * the host: what it gives the controller for a transfer, and that the bytes
 * are the caller's when the controller has run it. The controller is a
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
#include <pb/dma.h>
#include <pb/pb.h>
#include <string.h>

#include "fakes/dcache.h"
#include "fakes/regs.h"

/* The MEK's controller */
#define CDNS3_BASE      0x5b110000u
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

/* A TRB, and what the driver puts in one and in its ring (cdns3_udc_core.c) */
#define TRB_SIZE        12
#define TRB_MAX_LENGTH  (127 * 1024)
#define TRB_COUNT       128
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

/* EP_TRADDR is the selected endpoint's: an index for each direction */
static uint32_t traddr[32];
static unsigned int selected;

/* What the registers do with a write: the status registers are written to
 * clear, and a flush ends at once */
static uint64_t on_write(uintptr_t addr, uint64_t value)
{
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
        return value & ~(uint64_t)(EP_CMD_DFLUSH | EP_CMD_REQ_CMPL);
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
        cmocka_unit_test_setup(test_cancel_gives_the_buffer_back, setup),
        cmocka_unit_test_setup(test_setup_packet, setup),
        cmocka_unit_test_setup(test_init_again, setup),
    };

    return cmocka_run_group_tests(tests, group_setup, NULL);
}
