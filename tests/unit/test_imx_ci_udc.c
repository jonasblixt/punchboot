/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The ChipIdea device controller's driver (imx_ci_udc.c), built for the
 * host: what it queues for a transfer, and that the bytes are the caller's
 * when the controller has run it. Two small models stand in for the
 * hardware:
 *
 *  - The controller's side of a transfer (i.MX 6UL RM 54.4.5, 54.4.6.6): it
 *    follows the endpoint's queue head to the transfer descriptors and moves
 *    each one's bytes through its five pages, page 0 from its offset up to
 *    the page's end, the others from their start, and retires it. Nothing
 *    of the bus, of endpoint 0's setup or of the registers' timing: that is
 *    the bench's (usb.md 8).
 *  - A data cache (drivers.md 4.2, fakes/dcache.h). The controller reads and
 *    writes RAM, the driver the cache. A clean that is missing gives the
 *    controller old bytes, an invalidate that is missing or late gives them
 *    to the caller, and the controller writes only what the driver began:
 *    its queue heads from init to stop, a transfer's descriptors and an OUT
 *    transfer's buffers while the transfer runs.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/usb/imx_ci_udc.h>
#include <drivers/usb/usbd.h>
#include <pb/dma.h>
#include <pb/pb.h>
#include <string.h>

#include "fakes/dcache.h"
#include "fakes/plat.h"
#include "fakes/regs.h"
#include "imx_ci_udc_private.h"

/* jiffy's controller */
#define BASE         0x02184000u
#define PAGE         DTD_PAGE_SIZE
#define EP_IS_IN(ep) ((ep) & 1)

/* The driver's transfer descriptors with fakes/imx_ci_udc/config.h */
#define NO_OF_DTDS   (1 + CONFIG_CM_BUF_SIZE_KiB / 20)
#define NO_OF_DQHS   16
#define LINE         PB_DMA_ALIGN

/* The controller reads and writes RAM, behind the driver's cache */
static void ram_read(uintptr_t addr, void *dst, size_t length)
{
    dcache_dev_read(addr, dst, length);
}

static void ram_write(uintptr_t addr, const void *src, size_t length)
{
    dcache_dev_write(addr, src, length);
}

/*
 * The controller
 */

static const struct usbd_hal_ops *ops;
static bool one_xfer;
static uint32_t primed;

int usbd_init_hal_ops(const struct usbd_hal_ops *ops_)
{
    ops = ops_;
    return PB_OK;
}

void usbd_hal_one_xfer(void)
{
    one_xfer = true;
}

/* What the registers do with a write: the bits that clear themselves, and
 * the status registers, which are written to clear */
static uint64_t on_write(uintptr_t addr, uint64_t value)
{
    switch (addr - BASE) {
    case CI_USBCMD:
        return value & ~(uint64_t)USBCMD_RST;
    case CI_ENDPTPRIME:
        primed |= (uint32_t)value;
        return 0;
    case CI_ENDPTFLUSH:
    case CI_USBSTS:
    case CI_ENDPTCOMPLETE:
    case CI_ENDPTSETUPSTAT:
        return 0;
    default:
        return value;
    }
}

static uint32_t ep_bit(usb_ep_t ep)
{
    return EP_IS_IN(ep) ? ENDPT_TX(ep >> 1) : ENDPT_RX(ep >> 1);
}

/* The transfer descriptor the endpoint's queue head points at, as the
 * driver wrote it */
static const struct imx_ci_udc_transfer_head *first_dtd(usb_ep_t ep)
{
    const struct imx_ci_udc_queue_head *qh =
        (const void *)(uintptr_t)fake_regs_get(BASE + CI_ENDPTLISTADDR);

    return (const void *)(uintptr_t)qh[ep].next;
}

/*
 * The controller executes what was primed on 'ep': 'stream' is what the host
 * sends on an OUT endpoint, and what it receives on an IN endpoint. Returns
 * the number of bytes that moved.
 */
static size_t controller_run(usb_ep_t ep, uint8_t *stream, size_t size)
{
    struct imx_ci_udc_queue_head qh;
    struct imx_ci_udc_transfer_head dtd;
    uintptr_t dtd_addr;
    size_t pos = 0;

    assert_true(primed & ep_bit(ep));
    primed &= ~ep_bit(ep);

    ram_read(fake_regs_get(BASE + CI_ENDPTLISTADDR) + ep * sizeof(qh), &qh, sizeof(qh));
    dtd_addr = qh.next;

    for (unsigned int count = 0;; count++) {
        size_t total;

        assert_true(count < NO_OF_DTDS);
        assert_int_equal(dtd_addr & 0x1f, 0);
        ram_read(dtd_addr, &dtd, sizeof(dtd));
        assert_true(dtd.token & DTD_TOKEN_STATUS_ACTIVE);
        total = (dtd.token & DTD_TOKEN_TOTAL_BYTES_MASK) >> DTD_TOKEN_TOTAL_BYTES_SHIFT;

        for (int n = 0; n < DTD_NO_OF_PAGES && total > 0; n++) {
            /* Only page 0 has an offset */
            uintptr_t addr = (n == 0) ? dtd.page[n] : (dtd.page[n] & ~DTD_PAGE_OFFSET_MASK);
            size_t chunk = PAGE - (addr & DTD_PAGE_OFFSET_MASK);

            if (chunk > total)
                chunk = total;

            assert_true(pos + chunk <= size);

            if (EP_IS_IN(ep))
                ram_read(addr, stream + pos, chunk);
            else
                ram_write(addr, stream + pos, chunk);

            pos += chunk;
            total -= chunk;
        }

        if (total > 0)
            fail_msg("A transfer descriptor with %zu bytes more than its pages hold", total);

        /* Retired: not active, and no bytes left */
        dtd.token &= ~(DTD_TOKEN_STATUS_ACTIVE | DTD_TOKEN_TOTAL_BYTES_MASK);
        ram_write(dtd_addr + offsetof(struct imx_ci_udc_transfer_head, token),
                  &dtd.token,
                  sizeof(dtd.token));

        if (dtd.next & DTD_NEXT_T)
            break;

        dtd_addr = dtd.next;
    }

    return pos;
}

/*
 * The tests
 */

/* Buffers at any offset in a page, with room for the longest transfer */
#define MAX_LENGTH (NO_OF_DTDS * DTD_MAX_BYTES)

static uint8_t arena[MAX_LENGTH + 4 * PAGE] __attribute__((aligned(PAGE)));
static uint8_t host[MAX_LENGTH + PAGE];

static int setup(void **state)
{
    (void)state;

    fake_regs_reset();
    fake_regs_on_write = on_write;
    primed = 0;
    one_xfer = false;
    ops = NULL;

    assert_int_equal(imx_ci_udc_init(BASE), PB_OK);
    assert_non_null(ops);
    assert_int_equal(ops->init(), PB_OK);

    /* A running controller has the queue heads, and nothing else */
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);

    return 0;
}

/* The driver keeps what it gave the controller between the tests, and so
 * does the cache's model */
static int group_setup(void **state)
{
    (void)state;

    dcache_reset();
    return 0;
}

static void fill(uint8_t *p, size_t length, unsigned int seed)
{
    for (size_t i = 0; i < length; i++)
        p[i] = (uint8_t)(seed + i * 13 + (i >> 8) * 7);
}

/* A transfer from the host of 'length' bytes into a buffer that starts
 * 'offset' bytes into a page (on a line: the caller's buffers own their
 * lines, usb.md 2.3). Every byte arrives, and none after them is touched */
static void transfer_out(size_t offset, size_t length)
{
    uint8_t *bfr = arena + PAGE + offset;
    size_t whole_lines = (length + LINE - 1) & ~(size_t)(LINE - 1);

    fill(host, length, (unsigned int)(offset + length));
    memset(arena, 0xee, sizeof(arena));

    assert_int_equal(ops->xfer_start(USB_EP2_OUT, bfr, length), PB_OK);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_AGAIN);
    assert_int_equal(controller_run(USB_EP2_OUT, host, length), length);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), PB_OK);

    if (memcmp(bfr, host, length) != 0)
        fail_msg("OUT, offset 0x%zx, %zu bytes: not what the host sent", offset, length);

    /* The transfer is done: the buffer is the caller's again */
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);

    /* Before the buffer and after its last line: the model fails a write of
     * the controller's there, and the driver's copy stays inside too */
    for (size_t i = 0; i < sizeof(arena); i++) {
        if (arena + i >= bfr && arena + i < bfr + whole_lines)
            continue;
        if (arena[i] != 0xee)
            fail_msg("OUT, offset 0x%zx, %zu bytes: byte %zu of the arena was written",
                     offset,
                     length,
                     i);
    }
}

/* A transfer to the host of 'length' bytes that the caller has just written,
 * so they are in the cache only. The host gets every byte, in order */
static void transfer_in(size_t offset, size_t length)
{
    uint8_t *bfr = arena + PAGE + offset;

    memset(arena, 0xee, sizeof(arena));
    fill(bfr, length, (unsigned int)(offset + length + 1));
    memset(host, 0, sizeof(host));

    assert_int_equal(ops->xfer_start(USB_EP1_IN, bfr, length), PB_OK);
    assert_int_equal(ops->xfer_complete(USB_EP1_IN), -PB_ERR_AGAIN);
    assert_int_equal(controller_run(USB_EP1_IN, host, length), length);
    assert_int_equal(ops->xfer_complete(USB_EP1_IN), PB_OK);

    if (memcmp(host, bfr, length) != 0)
        fail_msg("IN, offset 0x%zx, %zu bytes: not what the caller wrote", offset, length);
}

/* usb.md 4: the driver registers its operations, and says that it has one
 * transfer for all its endpoints (usb.md 8, F-203) */
static void test_registration(void **state)
{
    (void)state;

    assert_non_null(ops->init);
    assert_non_null(ops->stop);
    assert_non_null(ops->xfer_start);
    assert_non_null(ops->xfer_complete);
    assert_non_null(ops->xfer_cancel);
    assert_non_null(ops->poll_setup_pkt);
    assert_non_null(ops->set_configuration);
    assert_non_null(ops->set_address);
    assert_non_null(ops->ep0_xfer_zlp);
    assert_true(one_xfer);
}

/* usb.md 2.3, 4.3: transfers of every size around a page and around a
 * transfer descriptor's five, in a buffer that starts on a page as command
 * mode's two large buffers do */
static void test_page_aligned_buffer(void **state)
{
    static const size_t lengths[] = {
        1,
        64,
        512,
        PAGE - 1,
        PAGE,
        PAGE + 1,
        2 * PAGE,
        DTD_MAX_BYTES - 1,
        DTD_MAX_BYTES,
        DTD_MAX_BYTES + 1,
        2 * DTD_MAX_BYTES + 512,
        MAX_LENGTH,
    };
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(lengths); i++) {
        transfer_out(0, lengths[i]);
        transfer_in(0, lengths[i]);
    }
}

/* usb.md 2.3: a write of 0 bytes sends one empty packet: a descriptor with
 * no bytes, which is also endpoint 0's status stage */
static void test_zero_length(void **state)
{
    const struct imx_ci_udc_transfer_head *dtd;
    (void)state;

    assert_int_equal(ops->xfer_start(USB_EP1_IN, NULL, 0), PB_OK);
    dtd = first_dtd(USB_EP1_IN);
    assert_int_equal(dtd->token, DTD_TOKEN_STATUS_ACTIVE | DTD_TOKEN_IOC);
    assert_int_equal(dtd->next, DTD_NEXT_T);
    assert_int_equal(controller_run(USB_EP1_IN, host, 0), 0);
    assert_int_equal(ops->xfer_complete(USB_EP1_IN), PB_OK);
}

/* usb.md 4.3: a length the driver can't queue is refused, not shortened */
static void test_too_long(void **state)
{
    (void)state;

    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena, MAX_LENGTH + 1), -PB_ERR_MEM);
    assert_int_equal(ops->xfer_start(USB_EP_END, arena, 512), -PB_ERR_PARAM);
}

/* A result, and every other write from a buffer that doesn't start on a
 * page and fits in one: it goes through the driver's bounce buffer */
static void test_write_from_inside_a_page(void **state)
{
    static const size_t offsets[] = { 0x40, 0x200, 0xe00, 0xfc0 };
    static const size_t lengths[] = { 1, 64, 512, 513, PAGE - 1, PAGE };
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(offsets); i++) {
        for (size_t j = 0; j < ARRAY_SIZE(lengths); j++)
            transfer_in(offsets[i], lengths[j]);
    }
}

/* drivers.md 4.2, item 4: invalidate before the CPU's own writes, never
 * after. A read into a buffer that doesn't start on a page and fits in one
 * comes through the bounce buffer, and the driver's copy into the caller's
 * buffer is in the cache only (F-134: it then invalidated the buffer, which
 * drops the copy). Every command is read so: jiffy's command buffer is 0x200
 * into a page. What is queued for it is one descriptor with the bounce
 * buffer's page, from its start */
static void test_read_into_a_page(void **state)
{
    static const size_t offsets[] = { 0x40, 0x200, 0xe00, 0xfc0 };
    static const size_t lengths[] = { 1, 64, 512, 513, PAGE - 1, PAGE };
    const struct imx_ci_udc_transfer_head *dtd;
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(offsets); i++) {
        for (size_t j = 0; j < ARRAY_SIZE(lengths); j++)
            transfer_out(offsets[i], lengths[j]);
    }

    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena + PAGE + 0x200, 512), PB_OK);
    dtd = first_dtd(USB_EP2_OUT);
    assert_int_equal(dtd->next, DTD_NEXT_T);
    assert_int_equal(dtd->token,
                     DTD_TOKEN_STATUS_ACTIVE | DTD_TOKEN_IOC | DTD_TOKEN_TOTAL_BYTES(512));
    assert_int_equal(dtd->page[0] & DTD_PAGE_OFFSET_MASK, 0);
    assert_true((uintptr_t)dtd->page[0] < (uintptr_t)arena ||
                (uintptr_t)dtd->page[0] >= (uintptr_t)arena + sizeof(arena));
}

/* usb.md 4.3: a buffer that doesn't start on a page and is longer than what
 * is left of its page. The head goes through the bounce buffer and the rest
 * straight to the caller's pages. The controller fills page 0 from its
 * offset up to the page's end before it goes on to page 1, so the head ends
 * at the bounce buffer's end (F-131: it started at the bounce buffer's
 * start, and the controller took a whole page there; F-205: and every page
 * after it was the bounce buffer again). A descriptor whose page 0 is short
 * holds less than five pages */
static void test_buffer_from_inside_a_page_over_several(void **state)
{
    static const size_t offsets[] = { 0x40, 0x200, 0xe00, 0xfc0 };
    static const size_t lengths[] = {
        PAGE + 1,          2 * PAGE,      2 * PAGE + 512,    DTD_MAX_BYTES - PAGE,
        DTD_MAX_BYTES - 1, DTD_MAX_BYTES, DTD_MAX_BYTES + 1, 2 * DTD_MAX_BYTES + 512,
        MAX_LENGTH - PAGE,
    };
    const struct imx_ci_udc_transfer_head *dtd;
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(offsets); i++) {
        for (size_t j = 0; j < ARRAY_SIZE(lengths); j++) {
            transfer_out(offsets[i], lengths[j]);
            transfer_in(offsets[i], lengths[j]);
        }
    }

    /* What is queued: 0x200 of the head at the bounce buffer's end, then the
     * caller's pages */
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena + 2 * PAGE - 0x200, 3 * PAGE), PB_OK);
    dtd = first_dtd(USB_EP2_OUT);
    assert_int_equal(dtd->next, DTD_NEXT_T);
    assert_int_equal(dtd->token,
                     DTD_TOKEN_STATUS_ACTIVE | DTD_TOKEN_IOC | DTD_TOKEN_TOTAL_BYTES(3 * PAGE));
    assert_int_equal(dtd->page[0] & DTD_PAGE_OFFSET_MASK, PAGE - 0x200);
    assert_int_equal(dtd->page[1], (uint32_t)(uintptr_t)(arena + 2 * PAGE));
    assert_int_equal(dtd->page[2], (uint32_t)(uintptr_t)(arena + 3 * PAGE));
    assert_int_equal(dtd->page[3], (uint32_t)(uintptr_t)(arena + 4 * PAGE));

    /* More than the descriptors hold when the first one's page 0 is short */
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena + 2 * PAGE - 0x200, MAX_LENGTH),
                     -PB_ERR_MEM);
}

/* usb.md 4.3: a transfer that is given up, or that a bus reset ended, leaves
 * its buffer with the caller, and the next one starts from there */
static void test_cancel_gives_the_buffer_back(void **state)
{
    (void)state;

    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena + 0x200, 512), PB_OK);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_AGAIN);
    /* The descriptors' line, the buffer and the bounce buffer */
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS + 3);
    ops->xfer_cancel(USB_EP2_OUT);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
    transfer_out(0x200, 512);

    /* Started over a transfer that nobody completed */
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena + 0x200, 512), PB_OK);
    transfer_out(0, 3 * PAGE);
    transfer_in(0x40, 64);

    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena, 512), PB_OK);
    assert_int_equal(ops->stop(), PB_OK);
    assert_int_equal(dma_host_in_open(), 0);
}

/* usb.md 4.4, drivers.md 4.2: the controller writes a setup packet into
 * endpoint 0's queue head whenever the host sends one. The queue head is
 * the controller's while it runs, and the driver reads the packet from RAM */
static void test_setup_packet(void **state)
{
    static const struct usb_setup_packet get_descriptor = {
        .bRequestType = 0x80,
        .bRequest = 6,
        .wValue = 0x0100,
        .wLength = 18,
    };
    uintptr_t qh = (uintptr_t)fake_regs_get(BASE + CI_ENDPTLISTADDR) +
                   USB_EP0_OUT * sizeof(struct imx_ci_udc_queue_head);
    struct usb_setup_packet pkt;
    (void)state;

    assert_int_equal(ops->poll_setup_pkt(&pkt), -PB_ERR_AGAIN);

    for (int i = 0; i < 2; i++) {
        memset(&pkt, 0, sizeof(pkt));
        ram_write(qh + offsetof(struct imx_ci_udc_queue_head, setup),
                  &get_descriptor,
                  sizeof(get_descriptor));
        fake_regs_set(BASE + CI_ENDPTSETUPSTAT, ENDPTSETUPSTAT_EP(0));
        assert_int_equal(ops->poll_setup_pkt(&pkt), PB_OK);
        assert_memory_equal(&pkt, &get_descriptor, sizeof(pkt));
        assert_int_equal(ops->poll_setup_pkt(&pkt), -PB_ERR_AGAIN);

        /* The data stage on the same queue head's endpoint pair */
        assert_int_equal(ops->xfer_start(USB_EP0_IN, arena, 18), PB_OK);
        assert_int_equal(controller_run(USB_EP0_IN, host, 18), 18);
        assert_int_equal(ops->xfer_complete(USB_EP0_IN), PB_OK);
    }

    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
}

/* The host resets the bus: USBSTS's reset bit, which the driver reads and
 * clears at its next poll */
static void bus_reset(void)
{
    fake_regs_set(BASE + CI_USBSTS, USBSTS_URI);
}

/* usb.md 2.2, 2.4, 4.3: a bus reset of a device that was configured ends the
 * bulk transfer that runs with -PB_ERR_IO, and with none running it is kept
 * for the next that is started or polled. Before the device is configured
 * it is no error, and endpoint 0 goes on through it: the host enumerates
 * (F-363: the driver flushed, went on and reported nothing) */
static void test_bus_reset_is_reported(void **state)
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
    struct usb_setup_packet pkt;
    (void)state;

    /* Not configured: a host resets the bus when it enumerates */
    bus_reset();
    assert_int_equal(ops->poll_setup_pkt(&pkt), -PB_ERR_AGAIN);
    assert_int_equal(fake_regs_get(BASE + CI_USBSTS), 0);
    assert_int_equal(ops->set_configuration(eps, ARRAY_SIZE(eps)), PB_OK);
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena, 512), PB_OK);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_AGAIN);

    /* During a bulk transfer */
    bus_reset();
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_IO);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_IO);
    assert_int_equal(ops->xfer_start(USB_EP1_IN, arena, 512), -PB_ERR_IO);

    /* Endpoint 0 answers the host's new enumeration */
    assert_int_equal(ops->xfer_start(USB_EP0_IN, arena, 18), PB_OK);
    assert_int_equal(controller_run(USB_EP0_IN, host, 18), 18);
    assert_int_equal(ops->xfer_complete(USB_EP0_IN), PB_OK);

    /* Configured again, it is a new session */
    assert_int_equal(ops->set_configuration(eps, ARRAY_SIZE(eps)), PB_OK);
    transfer_out(0x200, 512);

    /* With no transfer running, seen while endpoint 0 is polled: kept */
    bus_reset();
    assert_int_equal(ops->poll_setup_pkt(&pkt), -PB_ERR_AGAIN);
    assert_int_equal(fake_regs_get(BASE + CI_USBSTS), 0);
    assert_int_equal(ops->xfer_start(USB_EP1_IN, arena, 512), -PB_ERR_IO);
    assert_int_equal(ops->xfer_complete(USB_EP1_IN), -PB_ERR_IO);

    /* The transport is restarted: nothing is kept (usb.md 4.1) */
    assert_int_equal(ops->init(), PB_OK);
    bus_reset();
    assert_int_equal(ops->poll_setup_pkt(&pkt), -PB_ERR_AGAIN);
    assert_int_equal(ops->set_configuration(eps, ARRAY_SIZE(eps)), PB_OK);
    transfer_out(0x200, 512);

    /* And after a stop */
    bus_reset();
    assert_int_equal(ops->poll_setup_pkt(&pkt), -PB_ERR_AGAIN);
    assert_int_equal(ops->stop(), PB_OK);
    assert_int_equal(ops->init(), PB_OK);
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena, 512), PB_OK);
}

/* The host takes endpoint 0's status stage when the driver polls the
 * controller next */
static uint64_t host_takes_the_status_stage(uintptr_t addr, uint64_t value)
{
    if (addr == BASE + CI_USBSTS && (primed & ep_bit(USB_EP0_IN)))
        assert_int_equal(controller_run(USB_EP0_IN, host, 0), 0);

    return value;
}

/* usb.md 4.4, drivers.md 3: the status stage is waited for with a bound. A
 * host that doesn't take it (it left the control transfer, or was
 * unplugged) gets the stage given up after the bound, -PB_ERR_TIMEOUT, and
 * the next transfer works (F-422, F-138: the driver waited for it until the
 * watchdog) */
static void test_status_stage_is_bounded(void **state)
{
    (void)state;

    /* A fiftieth of the bound per poll: the register log holds the polls */
    fake_us_tick_step = 10000;

    /* Taken */
    fake_regs_on_read = host_takes_the_status_stage;
    assert_int_equal(ops->ep0_xfer_zlp(USB_EP0_IN), PB_OK);
    assert_int_equal(primed, 0);
    fake_regs_on_read = NULL;

    /* Not taken: given up after the bound, and not before */
    unsigned int start = fake_us_tick;
    size_t flushes = fake_regs_writes(BASE + CI_ENDPTFLUSH);

    assert_int_equal(ops->ep0_xfer_zlp(USB_EP0_OUT), -PB_ERR_TIMEOUT);
    assert_true(fake_us_tick - start >= 500000);
    assert_true(fake_us_tick - start < 600000);
    assert_int_equal(fake_regs_writes(BASE + CI_ENDPTFLUSH), flushes + 1);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);

    fake_us_tick_step = 0;
    transfer_out(0x200, 512);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_registration, setup),
        cmocka_unit_test_setup(test_page_aligned_buffer, setup),
        cmocka_unit_test_setup(test_zero_length, setup),
        cmocka_unit_test_setup(test_too_long, setup),
        cmocka_unit_test_setup(test_write_from_inside_a_page, setup),
        cmocka_unit_test_setup(test_read_into_a_page, setup),
        cmocka_unit_test_setup(test_buffer_from_inside_a_page_over_several, setup),
        cmocka_unit_test_setup(test_cancel_gives_the_buffer_back, setup),
        cmocka_unit_test_setup(test_setup_packet, setup),
        cmocka_unit_test_setup(test_bus_reset_is_reported, setup),
        cmocka_unit_test_setup(test_status_stage_is_bounded, setup),
    };

    return cmocka_run_group_tests(tests, group_setup, NULL);
}
