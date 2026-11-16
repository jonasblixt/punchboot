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
 *    the page's end, the others from their start, and retires it; or the
 *    host's transfer ends early, with a short packet or a descriptor error.
 *    ENDPTSTAT follows the primes, flushes and retires, and a test can make
 *    a prime fail or a flush be refused (RT1060 RM 42.5.6.4, 42.5.6.6).
 *    Nothing of the bus, of endpoint 0's setup or of the registers' timing:
 *    that is the bench's (usb.md 8).
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

/* The driver's longest transfer and its transfer descriptors */
#define MAX_LENGTH   (4 * 1024 * 1024)
#define NO_OF_DTDS   ((MAX_LENGTH + 4 * PAGE - 1) / (4 * PAGE) + 1)
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

/* The endpoints that are primed: what ENDPTSTAT reads (RT1060 RM 42.7.37) */
static uint32_t primed;
/* USBSTS, write one to clear */
static uint32_t usbsts;

/* USBSTS's USB interrupt: a transfer completed */
#define USBSTS_UI BIT_32(0)

/*
 * How the controller misbehaves, for a test: endpoints whose prime fails
 * (ENDPTPRIME clears and ENDPTSTAT stays clear, RM 42.5.6.4.2.2) or never
 * clears; flushes it refuses (ENDPTSTAT stays set, RM 42.5.6.6.5) or never
 * ends; a reset that never ends; prime bits that the controller holds for a
 * number of reads (its own prime of the next descriptor, RM 42.7.35); a
 * tripwire that trips on every read; a transfer the host takes at once,
 * when the endpoint is primed; what happens on a read of ENDPTCOMPLETE,
 * between the driver's read of USBSTS and its write; ENDPTSTAT bits that
 * the controller sets only after a number of reads (a slow prime); and
 * bits of endpoints that were never enabled, which stay set in ENDPTPRIME,
 * ENDPTFLUSH and ENDPTSTAT.
 */
static uint32_t stat_late;
static unsigned int stat_late_reads;
static uint32_t phantom;
static uint32_t prime_fails;
static uint32_t prime_stuck;
static unsigned int flush_refusals;
static uint32_t flush_stuck;
static bool reset_stuck;
static uint32_t prime_busy;
static unsigned int prime_busy_reads;
static bool sutw_trips;
static int run_on_prime = -1;
static void (*on_complete_read)(void);
/* A test's own hook on a read, after the model's */
static uint64_t (*test_on_read)(uintptr_t addr, uint64_t value);

static size_t controller_run(usb_ep_t ep, uint8_t *stream, size_t size);

int usbd_init_hal_ops(const struct usbd_hal_ops *ops_)
{
    ops = ops_;
    return PB_OK;
}

static void set_primed(uint32_t value)
{
    primed = value;
    fake_regs_set(BASE + CI_ENDPTSTAT, primed);
}

static void set_usbsts(uint32_t value)
{
    usbsts = value;
    fake_regs_set(BASE + CI_USBSTS, usbsts);
}

/* What the registers do with a write: the bits that clear themselves, and
 * the status registers, which are written to clear. A prime sets the
 * endpoint's ENDPTSTAT bit, a flush clears it */
static uint64_t on_write(uintptr_t addr, uint64_t value)
{
    uint32_t bits = (uint32_t)value;

    switch (addr - BASE) {
    case CI_USBCMD:
        return reset_stuck ? value : (value & ~(uint64_t)USBCMD_RST);
    case CI_ENDPTPRIME:
        set_primed(primed | (bits & ~prime_fails & ~prime_stuck));

        if (run_on_prime >= 0 && (primed & bits))
            assert_int_equal(controller_run((usb_ep_t)run_on_prime, NULL, 0), 0);

        return bits & prime_stuck;
    case CI_ENDPTFLUSH:
        if (flush_refusals > 0 && (primed & bits))
            flush_refusals--;
        else
            set_primed(primed & ~bits);

        return bits & flush_stuck;
    case CI_USBSTS:
        usbsts &= ~bits;
        return usbsts;
    case CI_ENDPTCOMPLETE:
    case CI_ENDPTSETUPSTAT:
        return 0;
    default:
        return value;
    }
}

static uint64_t on_read(uintptr_t addr, uint64_t value)
{
    switch (addr - BASE) {
    case CI_ENDPTPRIME:
        if (prime_busy_reads > 0) {
            prime_busy_reads--;
            value |= prime_busy;
        } else {
            value &= ~(uint64_t)prime_busy;
            prime_busy = 0;
        }
        value |= phantom;
        break;
    case CI_ENDPTFLUSH:
        value |= phantom;
        break;
    case CI_ENDPTSTAT:
        value = primed | phantom;

        if (stat_late_reads > 0) {
            stat_late_reads--;
            value &= ~(uint64_t)stat_late;
        }
        break;
    case CI_USBCMD:
        if (sutw_trips)
            value &= ~(uint64_t)USBCMD_SUTW;
        break;
    case CI_ENDPTCOMPLETE:
        if (on_complete_read != NULL)
            on_complete_read();
        break;
    default:
        break;
    }

    if (test_on_read != NULL)
        value = test_on_read(addr, value);

    return value;
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

/* How the host's transfer goes on after 'limit' bytes, in controller_exec() */
enum host_end {
    /* It sends nothing more for now, at a descriptor's start: that one
     * stays active, and the endpoint primed */
    HOST_PAUSE,
    /* A short or empty packet: the controller retires the descriptor with
     * the bytes left in Total Bytes, "a successful transfer completion", and
     * goes on to the next one, which stays active (RM 42.5.6.4.1) */
    HOST_SHORT,
    /* The descriptor gets 'status' (an overflow's Data Buffer Error, RM
     * 42.5.6.4.1; Halted; a Transaction Error), Active only when 'status'
     * has it, and the controller flushes the endpoint */
    HOST_ERROR,
};

/*
 * The controller executes what was primed on 'ep': 'stream' is what the host
 * sends on an OUT endpoint, and what it receives on an IN endpoint, 'size'
 * its room. The host's transfer goes on as 'end' says after 'limit' bytes.
 * 'resume' goes on after a pause, past the descriptors retired before it.
 * Returns the number of bytes that moved.
 */
static size_t controller_exec(usb_ep_t ep,
                              uint8_t *stream,
                              size_t size,
                              size_t limit,
                              enum host_end end,
                              uint32_t status,
                              bool resume)
{
    struct imx_ci_udc_queue_head qh;
    struct imx_ci_udc_transfer_head dtd;
    uintptr_t dtd_addr;
    size_t pos = 0;

    assert_true(primed & ep_bit(ep));

    ram_read(fake_regs_get(BASE + CI_ENDPTLISTADDR) + ep * sizeof(qh), &qh, sizeof(qh));
    dtd_addr = qh.next;

    for (unsigned int count = 0;; count++) {
        size_t total;
        size_t left;

        assert_true(count < NO_OF_DTDS);
        assert_int_equal(dtd_addr & 0x1f, 0);
        ram_read(dtd_addr, &dtd, sizeof(dtd));

        if (resume && !(dtd.token & DTD_TOKEN_STATUS_ACTIVE)) {
            assert_false(dtd.next & DTD_NEXT_T);
            dtd_addr = dtd.next;
            continue;
        }

        resume = false;
        assert_true(dtd.token & DTD_TOKEN_STATUS_ACTIVE);
        total = (dtd.token & DTD_TOKEN_TOTAL_BYTES_MASK) >> DTD_TOKEN_TOTAL_BYTES_SHIFT;
        left = total;

        /* A packet doesn't span two descriptors (RM 54.4.6.4.1, RT1060 RM
         * 42.5.6.4.1): every one but the last holds whole packets, 512
         * bytes in high speed (F-438) */
        if (!(dtd.next & DTD_NEXT_T) && total % 512 != 0)
            fail_msg("A transfer descriptor of %zu bytes, not whole packets, before another",
                     total);

        for (int n = 0; n < DTD_NO_OF_PAGES && left > 0 && pos < limit; n++) {
            /* Only page 0 has an offset */
            uintptr_t addr = (n == 0) ? dtd.page[n] : (dtd.page[n] & ~DTD_PAGE_OFFSET_MASK);
            size_t chunk = PAGE - (addr & DTD_PAGE_OFFSET_MASK);

            if (chunk > left)
                chunk = left;

            if (chunk > limit - pos)
                chunk = limit - pos;

            assert_true(pos + chunk <= size);

            if (EP_IS_IN(ep))
                ram_read(addr, stream + pos, chunk);
            else
                ram_write(addr, stream + pos, chunk);

            pos += chunk;
            left -= chunk;
        }

        if (left > 0 && pos < limit)
            fail_msg("A transfer descriptor with %zu bytes more than its pages hold", left);

        /* The host's transfer goes on as 'end' says */
        if (left > 0) {
            if (end == HOST_PAUSE) {
                assert_int_equal(left, total);
                return pos;
            }

            dtd.token &= ~(DTD_TOKEN_STATUS_ACTIVE | DTD_TOKEN_TOTAL_BYTES_MASK);
            dtd.token |= DTD_TOKEN_TOTAL_BYTES(left);

            if (end == HOST_ERROR)
                dtd.token |= status;

            ram_write(dtd_addr + offsetof(struct imx_ci_udc_transfer_head, token),
                      &dtd.token,
                      sizeof(dtd.token));

            if (end == HOST_ERROR || (dtd.next & DTD_NEXT_T))
                set_primed(primed & ~ep_bit(ep));

            return pos;
        }

        /* Retired: not active, and no bytes left */
        dtd.token &= ~(DTD_TOKEN_STATUS_ACTIVE | DTD_TOKEN_TOTAL_BYTES_MASK);
        ram_write(dtd_addr + offsetof(struct imx_ci_udc_transfer_head, token),
                  &dtd.token,
                  sizeof(dtd.token));

        if (dtd.next & DTD_NEXT_T)
            break;

        dtd_addr = dtd.next;
    }

    /* The last descriptor is retired: the endpoint isn't primed */
    set_primed(primed & ~ep_bit(ep));
    return pos;
}

/* The whole transfer that was primed on 'ep' */
static size_t controller_run(usb_ep_t ep, uint8_t *stream, size_t size)
{
    return controller_exec(ep, stream, size, SIZE_MAX, HOST_PAUSE, 0, false);
}

/*
 * The tests
 */

/* Buffers at any offset in a page, with room for the longest transfer */
static uint8_t arena[MAX_LENGTH + 4 * PAGE] __attribute__((aligned(PAGE)));
static uint8_t host[MAX_LENGTH + PAGE];

static int setup(void **state)
{
    (void)state;

    fake_regs_reset();
    fake_regs_on_write = on_write;
    fake_regs_on_read = on_read;
    set_primed(0);
    set_usbsts(0);
    prime_fails = 0;
    prime_stuck = 0;
    flush_refusals = 0;
    flush_stuck = 0;
    reset_stuck = false;
    prime_busy = 0;
    prime_busy_reads = 0;
    sutw_trips = false;
    stat_late = 0;
    stat_late_reads = 0;
    phantom = 0;
    run_on_prime = -1;
    on_complete_read = NULL;
    test_on_read = NULL;
    fake_us_tick_step = 0;
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

/* usb.md 4: the driver registers its operations */
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

/* usb.md 2.3, 4.3: a length above the driver's limit is refused, not
 * shortened, whatever the buffer's start (F-287: the limit followed the
 * descriptors' room for the buffer, -PB_ERR_MEM) */
static void test_too_long(void **state)
{
    (void)state;

    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena, MAX_LENGTH + 1), -PB_ERR_PARAM);
    assert_int_equal(ops->xfer_start(USB_EP1_IN, arena + 0x200, MAX_LENGTH + 1), -PB_ERR_PARAM);
    assert_int_equal(ops->xfer_start(USB_EP_END, arena, 512), -PB_ERR_PARAM);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
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
        MAX_LENGTH,
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

    ops->xfer_cancel(USB_EP2_OUT);

    /* The longest transfer when the first descriptor's page 0 is shortest:
     * a write from a buffer that isn't on a line, one byte before a page */
    transfer_in(PAGE - 1, MAX_LENGTH);
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

/* A second SETUP that arrives once the first is copied: the second read of
 * USBCMD in a poll is the tripwire's check after the copy */
static unsigned int usbcmd_reads;
static const struct usb_setup_packet late_setup = {
    .bRequestType = 0x00,
    .bRequest = 9,
    .wValue = 1,
};

static uint64_t setup_after_the_copy(uintptr_t addr, uint64_t value)
{
    if (addr == BASE + CI_USBCMD && ++usbcmd_reads == 2) {
        uintptr_t qh = (uintptr_t)fake_regs_get(BASE + CI_ENDPTLISTADDR) +
                       USB_EP0_OUT * sizeof(struct imx_ci_udc_queue_head);

        ram_write(
            qh + offsetof(struct imx_ci_udc_queue_head, setup), &late_setup, sizeof(late_setup));
        fake_regs_set(BASE + CI_ENDPTSETUPSTAT, ENDPTSETUPSTAT_EP(0));
    }

    return value;
}

/* RT1060 RM 42.5.6.4.2.1, 6UL RM 54.4.6.4.2 (F-138): the setup status is
 * cleared first, then the packet is copied under the tripwire. A SETUP
 * during the copy trips it, one after it sets the status again for the
 * next poll: neither is acknowledged unread */
static void test_setup_status_cleared_before_the_copy(void **state)
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
    size_t clear = SIZE_MAX;
    size_t sutw = SIZE_MAX;
    size_t start;
    (void)state;

    ram_write(qh + offsetof(struct imx_ci_udc_queue_head, setup),
              &get_descriptor,
              sizeof(get_descriptor));
    fake_regs_set(BASE + CI_ENDPTSETUPSTAT, ENDPTSETUPSTAT_EP(0));
    usbcmd_reads = 0;
    test_on_read = setup_after_the_copy;
    start = fake_reg_log_len;
    assert_int_equal(ops->poll_setup_pkt(&pkt), PB_OK);
    test_on_read = NULL;
    assert_memory_equal(&pkt, &get_descriptor, sizeof(pkt));

    for (size_t i = start; i < fake_reg_log_len; i++) {
        if (fake_reg_log[i].addr == BASE + CI_ENDPTSETUPSTAT && clear == SIZE_MAX)
            clear = i;
        else if (fake_reg_log[i].addr == BASE + CI_USBCMD &&
                 (fake_reg_log[i].value & USBCMD_SUTW) && sutw == SIZE_MAX)
            sutw = i;
    }

    assert_true(clear < sutw);

    /* The late SETUP is the next poll's */
    assert_int_equal(ops->poll_setup_pkt(&pkt), PB_OK);
    assert_memory_equal(&pkt, &late_setup, sizeof(pkt));
    assert_int_equal(ops->poll_setup_pkt(&pkt), -PB_ERR_AGAIN);
}

/* pb's two bulk endpoints (usb.md 6) */
static const struct usb_endpoint_descriptor bulk_eps[] = {
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

/* The host resets the bus: USBSTS's reset bit, which the driver reads and
 * clears at its next poll */
static void bus_reset(void)
{
    set_usbsts(usbsts | USBSTS_URI);
}

/* usb.md 2.2, 2.4, 4.3: a bus reset of a device that was configured ends the
 * bulk transfer that runs with -PB_ERR_IO, and with none running it is kept
 * for the next that is started or polled. Before the device is configured
 * it is no error, and endpoint 0 goes on through it: the host enumerates
 * (F-363: the driver flushed, went on and reported nothing) */
static void test_bus_reset_is_reported(void **state)
{
    struct usb_setup_packet pkt;
    (void)state;

    /* Not configured: a host resets the bus when it enumerates */
    bus_reset();
    assert_int_equal(ops->poll_setup_pkt(&pkt), -PB_ERR_AGAIN);
    assert_int_equal(fake_regs_get(BASE + CI_USBSTS), 0);
    assert_int_equal(ops->set_configuration(bulk_eps, ARRAY_SIZE(bulk_eps)), PB_OK);
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
    assert_int_equal(ops->set_configuration(bulk_eps, ARRAY_SIZE(bulk_eps)), PB_OK);
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
    assert_int_equal(ops->set_configuration(bulk_eps, ARRAY_SIZE(bulk_eps)), PB_OK);
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
    test_on_read = host_takes_the_status_stage;
    assert_int_equal(ops->ep0_xfer_zlp(USB_EP0_IN), PB_OK);
    assert_int_equal(primed, 0);
    test_on_read = NULL;

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

/* What the controller sees of the transfer primed on 'ep', in RAM: its
 * queue head and its descriptors */
static size_t snapshot(usb_ep_t ep, uint8_t *dst, size_t size)
{
    struct imx_ci_udc_queue_head qh;
    struct imx_ci_udc_transfer_head dtd;
    size_t n = sizeof(qh);

    ram_read(fake_regs_get(BASE + CI_ENDPTLISTADDR) + ep * sizeof(qh), &qh, sizeof(qh));
    assert_true(n <= size);
    memcpy(dst, &qh, sizeof(qh));

    for (uintptr_t addr = qh.next;; addr = dtd.next) {
        ram_read(addr, &dtd, sizeof(dtd));
        assert_true(n + sizeof(dtd) <= size);
        memcpy(dst + n, &dtd, sizeof(dtd));
        n += sizeof(dtd);

        if (dtd.next & DTD_NEXT_T)
            return n;
    }
}

/* The device descriptor a host asks for, from a buffer that isn't on a
 * line or a page, as the stack's are */
static const struct {
    uint8_t pad[3];
    uint8_t desc[18];
} __attribute__((aligned(PAGE))) device_descriptor = {
    .desc = { 18, 1, 0, 2, 0xff, 0xff, 0xff, 64, 0x09, 0x12, 0x19, 0x20, 0, 1, 1, 1, 2, 1 },
};

/* A host's request on endpoint 0, answered as the stack does: the SETUP,
 * a data stage of the device descriptor and the status stage. Each
 * transfer's result is given once (usb.md 4.3; F-287) */
static void answer_a_request(void)
{
    static const struct usb_setup_packet get_descriptor = {
        .bRequestType = 0x80,
        .bRequest = 6,
        .wValue = 0x0100,
        .wLength = 64,
    };
    uintptr_t qh = (uintptr_t)fake_regs_get(BASE + CI_ENDPTLISTADDR) +
                   USB_EP0_OUT * sizeof(struct imx_ci_udc_queue_head);
    struct usb_setup_packet pkt;
    uint8_t reply[64];

    ram_write(qh + offsetof(struct imx_ci_udc_queue_head, setup),
              &get_descriptor,
              sizeof(get_descriptor));
    fake_regs_set(BASE + CI_ENDPTSETUPSTAT, ENDPTSETUPSTAT_EP(0));
    assert_int_equal(ops->poll_setup_pkt(&pkt), PB_OK);
    assert_memory_equal(&pkt, &get_descriptor, sizeof(pkt));

    assert_int_equal(ops->xfer_start(USB_EP0_IN, (void *)device_descriptor.desc, 18), PB_OK);
    assert_int_equal(ops->xfer_complete(USB_EP0_IN), -PB_ERR_AGAIN);
    assert_int_equal(controller_run(USB_EP0_IN, reply, sizeof(reply)), 18);
    assert_memory_equal(reply, device_descriptor.desc, 18);
    assert_int_equal(ops->xfer_complete(USB_EP0_IN), PB_OK);
    assert_int_equal(ops->xfer_complete(USB_EP0_IN), -PB_ERR_STATE);

    assert_int_equal(ops->xfer_start(USB_EP0_OUT, NULL, 0), PB_OK);
    assert_int_equal(controller_run(USB_EP0_OUT, reply, 0), 0);
    assert_int_equal(ops->xfer_complete(USB_EP0_OUT), PB_OK);
    assert_int_equal(ops->xfer_complete(USB_EP0_OUT), -PB_ERR_STATE);
}

/* usb.md 3.2, 4.3: a request on endpoint 0 during a bulk transfer changes
 * nothing of it. Endpoint 0 has its own descriptor and bounce buffer: what
 * the controller follows for the bulk transfer, its queue head and its
 * descriptors, stays as it was, the endpoint stays primed, and the bytes
 * come whole after the request (F-203: the driver had one transfer for all
 * endpoints, and the stack gave the bulk transfer up or failed it). Both
 * directions, from a buffer inside a page over several pages: the head
 * through the bulk endpoints' bounce buffer */
static void test_request_during_a_bulk_transfer(void **state)
{
    static uint8_t before[1024];
    static uint8_t after[1024];
    const size_t length = 3 * PAGE + 0x200;
    uint8_t *bfr = arena + PAGE + 0x200;
    size_t n;
    (void)state;

    for (int i = 0; i < 2; i++) {
        usb_ep_t ep = (i == 0) ? USB_EP2_OUT : USB_EP1_IN;

        memset(arena, 0xee, sizeof(arena));
        memset(host, 0, sizeof(host));
        fill(EP_IS_IN(ep) ? bfr : host, length, 7 + i);

        assert_int_equal(ops->xfer_start(ep, bfr, length), PB_OK);
        assert_int_equal(ops->xfer_complete(ep), -PB_ERR_AGAIN);
        n = snapshot(ep, before, sizeof(before));

        answer_a_request();

        assert_true(primed & ep_bit(ep));
        assert_int_equal(snapshot(ep, after, sizeof(after)), n);
        assert_memory_equal(before, after, n);
        assert_int_equal(ops->xfer_complete(ep), -PB_ERR_AGAIN);

        assert_int_equal(controller_run(ep, host, length), length);
        assert_int_equal(ops->xfer_complete(ep), PB_OK);
        assert_memory_equal(bfr, host, length);
        assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
    }
}

/* The same when the bulk transfer is done when the request comes and its
 * result is polled after it: a command read into the bulk endpoints' bounce
 * buffer (jiffy's command buffer is 0x200 into a page) isn't overwritten by
 * endpoint 0's reply, which bounces too */
static void test_request_before_the_result(void **state)
{
    uint8_t *bfr = arena + PAGE + 0x200;
    (void)state;

    memset(arena, 0xee, sizeof(arena));
    fill(host, 512, 3);

    assert_int_equal(ops->xfer_start(USB_EP2_OUT, bfr, 512), PB_OK);
    assert_int_equal(controller_run(USB_EP2_OUT, host, 512), 512);

    answer_a_request();

    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), PB_OK);
    assert_memory_equal(bfr, host, 512);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
}

/* usb.md 2.4, 4.3: xfer_complete gives a transfer's result once, for the
 * endpoint it was started on. With none started there, after the result,
 * and after a cancel it is -PB_ERR_STATE (F-287: it ignored its endpoint
 * and returned PB_OK again) */
static void test_result_once(void **state)
{
    (void)state;

    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_STATE);
    assert_int_equal(ops->xfer_complete(USB_EP0_IN), -PB_ERR_STATE);

    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena, 512), PB_OK);
    assert_int_equal(ops->xfer_complete(USB_EP1_IN), -PB_ERR_STATE);
    assert_int_equal(ops->xfer_complete(USB_EP0_IN), -PB_ERR_STATE);
    assert_int_equal(controller_run(USB_EP2_OUT, host, 512), 512);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), PB_OK);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_STATE);

    assert_int_equal(ops->xfer_start(USB_EP1_IN, arena, 512), PB_OK);
    ops->xfer_cancel(USB_EP1_IN);
    assert_int_equal(ops->xfer_complete(USB_EP1_IN), -PB_ERR_STATE);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
}

/* usb.md 4.3: endpoint 0's transfers fit its bounce buffer, and a longer
 * one is refused, not shortened */
static void test_ep0_length(void **state)
{
    (void)state;

    assert_int_equal(ops->xfer_start(USB_EP0_IN, arena + 0x40, 513), -PB_ERR_PARAM);

    memset(host, 0, sizeof(host));
    fill(arena + 0x40, 512, 5);
    assert_int_equal(ops->xfer_start(USB_EP0_IN, arena + 0x40, 512), PB_OK);
    assert_int_equal(controller_run(USB_EP0_IN, host, 512), 512);
    assert_int_equal(ops->xfer_complete(USB_EP0_IN), PB_OK);
    assert_memory_equal(host, arena + 0x40, 512);
}

/* The host's supply, as the controller sees it: OTGSC's B session valid */
static void vbus(bool on)
{
    fake_regs_set(BASE + CI_OTGSC, on ? OTGSC_BSV : 0);
}

/* usb.md 2.2, 2.4, 4.3: an unplug of a device that was configured ends the
 * bulk transfer that runs with -PB_ERR_IO, every endpoint is flushed, and
 * with none running it is kept for the next transfer. Before the device is
 * configured it is no error. A board whose VBUS doesn't reach the
 * controller never sees it, and nothing changes there (F-363: an unplug
 * wasn't reported, the transfer waited for a host that was gone) */
static void test_unplug_is_reported(void **state)
{
    struct usb_setup_packet pkt;
    (void)state;

    /* Before the device is configured: no error */
    vbus(true);
    vbus(false);
    assert_int_equal(ops->poll_setup_pkt(&pkt), -PB_ERR_AGAIN);
    vbus(true);
    assert_int_equal(ops->set_configuration(bulk_eps, ARRAY_SIZE(bulk_eps)), PB_OK);
    transfer_out(0x200, 512);

    /* During a bulk transfer */
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena + 0x200, 3 * PAGE), PB_OK);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_AGAIN);
    vbus(false);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_IO);
    assert_int_equal(primed, 0);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_IO);
    assert_int_equal(ops->xfer_start(USB_EP1_IN, arena, 512), -PB_ERR_IO);

    /* Plugged in again: the host enumerates, and configures a new session */
    vbus(true);
    bus_reset();
    assert_int_equal(ops->poll_setup_pkt(&pkt), -PB_ERR_AGAIN);
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena, 512), -PB_ERR_IO);
    assert_int_equal(ops->set_configuration(bulk_eps, ARRAY_SIZE(bulk_eps)), PB_OK);
    transfer_out(0x200, 512);

    /* With no transfer running, seen while endpoint 0 is polled: kept */
    vbus(false);
    assert_int_equal(ops->poll_setup_pkt(&pkt), -PB_ERR_AGAIN);
    assert_int_equal(ops->xfer_start(USB_EP1_IN, arena, 512), -PB_ERR_IO);
    assert_int_equal(ops->xfer_complete(USB_EP1_IN), -PB_ERR_IO);

    /* The transport is restarted: nothing is kept (usb.md 4.1) */
    assert_int_equal(ops->init(), PB_OK);
    vbus(true);
    assert_int_equal(ops->set_configuration(bulk_eps, ARRAY_SIZE(bulk_eps)), PB_OK);
    transfer_out(0x200, 512);

    /* VBUS doesn't reach the controller: nothing is reported, as before */
    assert_int_equal(ops->init(), PB_OK);
    vbus(false);
    assert_int_equal(ops->set_configuration(bulk_eps, ARRAY_SIZE(bulk_eps)), PB_OK);
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena + 0x200, 512), PB_OK);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_AGAIN);
    assert_int_equal(controller_run(USB_EP2_OUT, host, 512), 512);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), PB_OK);
}

/* The ENDPTFLUSH writes in the log from 'start' */
static size_t flushes_since(size_t start)
{
    size_t n = 0;

    for (size_t i = start; i < fake_reg_log_len; i++) {
        if (fake_reg_log[i].addr == BASE + CI_ENDPTFLUSH)
            n++;
    }

    return n;
}

/* RT1060 RM 42.5.6.4.2.2: a prime is done when ENDPTPRIME's bit is clear
 * and ENDPTSTAT's set. A failed prime (ENDPTSTAT stays clear for the
 * bound, the descriptor active) gives the transfer up with -PB_ERR_IO, and
 * the next transfer works (F-136: the driver didn't look at ENDPTSTAT, and
 * the transfer waited for ever, F-130's symptom) */
static void test_failed_prime(void **state)
{
    unsigned int start;
    (void)state;

    fake_us_tick_step = 100;
    prime_fails = ep_bit(USB_EP2_OUT);
    start = fake_us_tick;
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena + 0x200, 3 * PAGE), -PB_ERR_IO);
    assert_true(fake_us_tick - start >= 10000);
    assert_true(fake_us_tick - start < 20000);
    assert_int_equal(primed, 0);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_STATE);

    prime_fails = 0;
    transfer_out(0x200, 3 * PAGE);
    transfer_in(0x40, 512);
}

/* The same on endpoint 0: with a SETUP or a bus reset waiting, the host
 * left the control transfer (RM 42.5.6.4.2.2), which the stack takes as a
 * host that started over: -PB_ERR_TIMEOUT, as the stage's bound. With
 * neither the controller failed, after the bound: -PB_ERR_IO. A SETUP or a
 * bus reset ends the wait at once */
static void test_failed_prime_on_endpoint_0(void **state)
{
    struct usb_setup_packet pkt;
    unsigned int start;
    (void)state;

    fake_us_tick_step = 100;
    prime_fails = ep_bit(USB_EP0_IN) | ep_bit(USB_EP0_OUT);

    assert_int_equal(ops->ep0_xfer_zlp(USB_EP0_IN), -PB_ERR_IO);
    assert_int_equal(ops->xfer_start(USB_EP0_IN, arena, 18), -PB_ERR_IO);

    fake_regs_set(BASE + CI_ENDPTSETUPSTAT, ENDPTSETUPSTAT_EP(0));
    start = fake_us_tick;
    assert_int_equal(ops->xfer_start(USB_EP0_IN, arena, 18), -PB_ERR_TIMEOUT);
    assert_true(fake_us_tick - start < 1000);
    assert_int_equal(ops->xfer_complete(USB_EP0_IN), -PB_ERR_STATE);
    fake_regs_set(BASE + CI_ENDPTSETUPSTAT, 0);

    bus_reset();
    assert_int_equal(ops->ep0_xfer_zlp(USB_EP0_OUT), -PB_ERR_TIMEOUT);
    assert_int_equal(ops->poll_setup_pkt(&pkt), -PB_ERR_AGAIN);
    assert_int_equal(usbsts, 0);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);

    prime_fails = 0;
    assert_int_equal(ops->xfer_start(USB_EP0_IN, arena, 18), PB_OK);
    assert_int_equal(controller_run(USB_EP0_IN, host, 18), 18);
    assert_int_equal(ops->xfer_complete(USB_EP0_IN), PB_OK);
}

/* A transfer the controller has done before the driver reads ENDPTSTAT: a
 * status stage the host takes at once. ENDPTSTAT is clear, and the prime
 * didn't fail: the descriptor is retired */
static void test_prime_done_at_once(void **state)
{
    (void)state;

    run_on_prime = USB_EP0_IN;
    assert_int_equal(ops->ep0_xfer_zlp(USB_EP0_IN), PB_OK);
    assert_int_equal(primed, 0);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
}

/* drivers.md 3: a prime that doesn't end is given up after the bound,
 * -PB_ERR_TIMEOUT, and the next transfer works */
static void test_prime_is_bounded(void **state)
{
    unsigned int start;
    (void)state;

    size_t writes;

    assert_int_equal(ops->set_configuration(bulk_eps, ARRAY_SIZE(bulk_eps)), PB_OK);
    fake_us_tick_step = 100;
    prime_stuck = ep_bit(USB_EP1_IN);
    start = fake_us_tick;
    writes = fake_reg_log_len;
    assert_int_equal(ops->xfer_start(USB_EP1_IN, arena, 512), -PB_ERR_TIMEOUT);
    assert_true(fake_us_tick - start >= 10000);
    assert_true(fake_us_tick - start < 30000);

    /* The flush is written all the same, and can't be seen done while
     * ENDPTPRIME stays set: the descriptors stay the controller's, the
     * session is lost and nothing is started until init */
    assert_true(flushes_since(writes) >= 1);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS + 1);
    assert_int_equal(ops->xfer_complete(USB_EP1_IN), -PB_ERR_IO);
    assert_int_equal(ops->xfer_start(USB_EP0_IN, arena, 18), -PB_ERR_IO);

    prime_stuck = 0;
    fake_regs_set(BASE + CI_ENDPTPRIME, 0);
    fake_us_tick_step = 0;
    assert_int_equal(ops->init(), PB_OK);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
    transfer_in(0, 512);
}

/* RM 42.7.37: ENDPTSTAT can be set some time after ENDPTPRIME clears. It is
 * polled within the bound, and a prime isn't failed on one read of it
 * (Linux's hw_ep_prime() doesn't trust it at all) */
static void test_prime_status_late(void **state)
{
    (void)state;

    stat_late = ep_bit(USB_EP2_OUT);
    stat_late_reads = 5;
    transfer_out(0x200, 512);
    assert_int_equal(stat_late_reads, 0);
}

/* RM 42.7.35, 42.7.37: a flush isn't done while ENDPTPRIME shows the
 * controller priming the endpoint's next descriptor, when ENDPTSTAT is
 * clear for the moment: the flush is written again */
static void test_flush_sees_a_prime(void **state)
{
    size_t start;
    (void)state;

    assert_int_equal(ops->set_configuration(bulk_eps, ARRAY_SIZE(bulk_eps)), PB_OK);
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena, 3 * DTD_MAX_BYTES), PB_OK);
    prime_busy = ep_bit(USB_EP2_OUT);
    prime_busy_reads = 1;
    start = fake_reg_log_len;
    ops->xfer_cancel(USB_EP2_OUT);
    assert_int_equal(flushes_since(start), 2);
    assert_int_equal(primed, 0);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
    transfer_out(0x200, 512);
}

/* The flushes wait for the endpoints that were enabled only: bits of others
 * that stay set (an endpoint the controller doesn't have) fail neither
 * init, a cancel, a bus reset nor stop. Stop doesn't wait for its flush:
 * the controller's reset after it ends everything, and is the only wait (a
 * failed stop fails the boot) */
static void test_unused_endpoints_and_stop(void **state)
{
    struct usb_setup_packet pkt;
    unsigned int start;
    (void)state;

    phantom = ENDPT_TX(5) | ENDPT_RX(5) | ENDPT_TX(7);
    assert_int_equal(ops->init(), PB_OK);
    assert_int_equal(ops->set_configuration(bulk_eps, ARRAY_SIZE(bulk_eps)), PB_OK);
    transfer_out(0x200, 512);
    assert_int_equal(ops->xfer_start(USB_EP1_IN, arena, 512), PB_OK);
    ops->xfer_cancel(USB_EP1_IN);
    assert_int_equal(primed, 0);
    transfer_in(0x40, 512);

    bus_reset();
    assert_int_equal(ops->poll_setup_pkt(&pkt), -PB_ERR_AGAIN);

    assert_int_equal(ops->set_configuration(bulk_eps, ARRAY_SIZE(bulk_eps)), PB_OK);
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena, 512), PB_OK);
    flush_stuck = ENDPT_ALL;
    fake_us_tick_step = 100;
    start = fake_us_tick;
    assert_int_equal(ops->stop(), PB_OK);
    assert_true(fake_us_tick - start < 1000);
    assert_int_equal(dma_host_in_open(), 0);
}

/* RM 42.5.6.6.5: a flush the controller refuses (ENDPTSTAT stays set, a
 * packet was in progress) is written again until the endpoint is idle, in
 * a cancel, in a transfer started over one that wasn't completed, and in
 * the flush of endpoint 0 at a SETUP (F-136: one write, and on to the next
 * prime) */
static void test_refused_flush_is_repeated(void **state)
{
    static const struct usb_setup_packet set_address = {
        .bRequestType = 0x00,
        .bRequest = 5,
        .wValue = 7,
    };
    uintptr_t qh = (uintptr_t)fake_regs_get(BASE + CI_ENDPTLISTADDR) +
                   USB_EP0_OUT * sizeof(struct imx_ci_udc_queue_head);
    struct usb_setup_packet pkt;
    size_t start;
    (void)state;

    /* A cancel */
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena + 0x200, 3 * PAGE), PB_OK);
    flush_refusals = 2;
    start = fake_reg_log_len;
    ops->xfer_cancel(USB_EP2_OUT);
    assert_int_equal(flushes_since(start), 3);
    assert_int_equal(primed, 0);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
    transfer_out(0x200, 512);

    /* A transfer started over one that is primed: flushed first */
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena + 0x200, 512), PB_OK);
    flush_refusals = 1;
    start = fake_reg_log_len;
    transfer_out(0, 3 * PAGE);
    assert_int_equal(flushes_since(start), 2);

    /* A SETUP while endpoint 0's status stage of the request before is
     * primed: the stage is flushed before the SETUP is handed on */
    assert_int_equal(ops->xfer_start(USB_EP0_IN, NULL, 0), PB_OK);
    assert_true(primed & ep_bit(USB_EP0_IN));
    ram_write(
        qh + offsetof(struct imx_ci_udc_queue_head, setup), &set_address, sizeof(set_address));
    fake_regs_set(BASE + CI_ENDPTSETUPSTAT, ENDPTSETUPSTAT_EP(0));
    flush_refusals = 1;
    start = fake_reg_log_len;
    assert_int_equal(ops->poll_setup_pkt(&pkt), PB_OK);
    assert_memory_equal(&pkt, &set_address, sizeof(pkt));
    assert_int_equal(flushes_since(start), 2);
    assert_int_equal(primed, 0);
    assert_int_equal(ops->xfer_complete(USB_EP0_IN), -PB_ERR_STATE);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
}

/* drivers.md 3: a flush that the controller never does is given up after
 * the bound. A cancel can't return it: the bulk transfers fail until the
 * device is configured or started again, and endpoint 0's flush at a SETUP
 * returns -PB_ERR_TIMEOUT */
static void test_flush_is_bounded(void **state)
{
    struct usb_setup_packet pkt;
    unsigned int start;
    (void)state;

    fake_us_tick_step = 100;

    /* Refused for ever */
    assert_int_equal(ops->set_configuration(bulk_eps, ARRAY_SIZE(bulk_eps)), PB_OK);
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena, 512), PB_OK);
    flush_refusals = UINT32_MAX;
    start = fake_us_tick;
    ops->xfer_cancel(USB_EP2_OUT);
    assert_true(fake_us_tick - start >= 10000);
    assert_true(fake_us_tick - start < 20000);
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena, 512), -PB_ERR_IO);
    /* Its descriptors' line and its buffer stay the controller's */
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS + 2);

    /* Never ends */
    flush_refusals = 0;
    set_primed(0);
    flush_stuck = ENDPT_TX(0) | ENDPT_RX(0);
    fake_regs_set(BASE + CI_ENDPTSETUPSTAT, ENDPTSETUPSTAT_EP(0));
    start = fake_us_tick;
    assert_int_equal(ops->poll_setup_pkt(&pkt), -PB_ERR_TIMEOUT);
    assert_true(fake_us_tick - start >= 10000);
    assert_true(fake_us_tick - start < 20000);

    /* Started again: the controller is reset */
    flush_stuck = 0;
    fake_regs_set(BASE + CI_ENDPTFLUSH, 0);
    fake_regs_set(BASE + CI_ENDPTSETUPSTAT, 0);
    fake_us_tick_step = 0;
    assert_int_equal(ops->init(), PB_OK);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
    assert_int_equal(ops->set_configuration(bulk_eps, ARRAY_SIZE(bulk_eps)), PB_OK);
    transfer_out(0x200, 512);
}

/* A bus reset as RM 42.5.6.2.1: the setup and complete status cleared,
 * then every prime waited for (here the controller's own prime of a next
 * descriptor) and every endpoint flushed once, not waited for: the
 * controller cancels every prime at a bus reset, and Linux's
 * hw_usb_reset() doesn't wait either (F-136: flushed with a prime running,
 * and the wait ended when the first bit cleared) */
static void test_bus_reset_sequence(void **state)
{
    struct usb_setup_packet pkt;
    size_t start;
    size_t setupstat = SIZE_MAX;
    size_t complete = SIZE_MAX;
    size_t flush = SIZE_MAX;
    (void)state;

    assert_int_equal(ops->set_configuration(bulk_eps, ARRAY_SIZE(bulk_eps)), PB_OK);
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena + 0x200, 3 * PAGE), PB_OK);
    prime_busy = ep_bit(USB_EP2_OUT);
    prime_busy_reads = 3;
    start = fake_reg_log_len;
    bus_reset();
    assert_int_equal(ops->poll_setup_pkt(&pkt), -PB_ERR_AGAIN);

    for (size_t i = fake_reg_log_len; i-- > start;) {
        if (fake_reg_log[i].addr == BASE + CI_ENDPTSETUPSTAT)
            setupstat = i;
        else if (fake_reg_log[i].addr == BASE + CI_ENDPTCOMPLETE)
            complete = i;
        else if (fake_reg_log[i].addr == BASE + CI_ENDPTFLUSH)
            flush = i;
    }

    assert_true(setupstat < complete);
    assert_true(complete < flush);
    assert_int_equal(fake_reg_log[flush].value, ENDPT_ALL);
    assert_int_equal(flushes_since(start), 1);
    assert_int_equal(prime_busy_reads, 0);
    assert_int_equal(primed, 0);
    assert_int_equal(usbsts, 0);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
}

/* USBSTS is cleared with the value read: a bus reset that comes between the
 * driver's read and its write is seen at the next poll (F-136: the write of
 * all ones cleared it unread, and a configured device's session went on) */
static void reset_between_read_and_write(void)
{
    set_usbsts(usbsts | USBSTS_URI);
    on_complete_read = NULL;
}

static void test_status_written_back_as_read(void **state)
{
    (void)state;

    assert_int_equal(ops->set_configuration(bulk_eps, ARRAY_SIZE(bulk_eps)), PB_OK);
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena, 512), PB_OK);

    set_usbsts(USBSTS_UI);
    on_complete_read = reset_between_read_and_write;
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_AGAIN);
    assert_int_equal(usbsts, USBSTS_URI);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_IO);
    assert_int_equal(usbsts, 0);
}

/* drivers.md 3: the controller's reset, the port's reset and the setup
 * tripwire are waited for with a bound (F-136: for ever) */
static void test_resets_are_bounded(void **state)
{
    struct usb_setup_packet pkt;
    unsigned int start;
    (void)state;

    fake_us_tick_step = 1000;

    reset_stuck = true;
    start = fake_us_tick;
    assert_int_equal(ops->init(), -PB_ERR_TIMEOUT);
    assert_true(fake_us_tick - start >= 250000);
    assert_true(fake_us_tick - start < 300000);
    start = fake_us_tick;
    assert_int_equal(ops->stop(), -PB_ERR_TIMEOUT);
    assert_true(fake_us_tick - start < 300000);
    reset_stuck = false;
    fake_regs_set(BASE + CI_USBCMD, 0);

    fake_regs_set(BASE + CI_PORTSC1, PORTSC1_PR);
    start = fake_us_tick;
    assert_int_equal(ops->init(), -PB_ERR_TIMEOUT);
    assert_true(fake_us_tick - start >= 100000);
    assert_true(fake_us_tick - start < 150000);
    fake_regs_set(BASE + CI_PORTSC1, 0);
    assert_int_equal(ops->init(), PB_OK);

    /* A tripwire that never holds: the SETUP is given up with the timeout,
     * its status was cleared before the copy (F-138), and the host asks
     * again */
    sutw_trips = true;
    fake_regs_set(BASE + CI_ENDPTSETUPSTAT, ENDPTSETUPSTAT_EP(0));
    assert_int_equal(ops->poll_setup_pkt(&pkt), -PB_ERR_TIMEOUT);
    sutw_trips = false;
    assert_int_equal(ops->poll_setup_pkt(&pkt), -PB_ERR_AGAIN);
    fake_regs_set(BASE + CI_ENDPTSETUPSTAT, ENDPTSETUPSTAT_EP(0));
    assert_int_equal(ops->poll_setup_pkt(&pkt), PB_OK);

    fake_us_tick_step = 0;
    transfer_out(0x200, 512);
}

/* RM 42.5.6.4.1, 42.5.6.6.4, usb.md 4.3: a descriptor that halted, or has
 * a Data Buffer Error or a Transaction Error, fails the transfer with
 * -PB_ERR_IO, also while it is still active. The endpoint is flushed, the
 * buffer is the caller's, the result is given once and the next transfer
 * works (F-137: only the last descriptor's Active bit was read, so an
 * overflow returned PB_OK and an error before the last descriptor left the
 * transfer waiting) */
static void test_descriptor_error(void **state)
{
    size_t start;
    (void)state;

    /* An overflow, a packet longer than what is left: the controller sets
     * Data Buffer Error and flushes the endpoint */
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena + 0x200, 3 * PAGE), PB_OK);
    assert_int_equal(controller_exec(USB_EP2_OUT,
                                     host,
                                     sizeof(host),
                                     PAGE + 64,
                                     HOST_ERROR,
                                     DTD_TOKEN_STATUS_DATA_BUFFER_ERR,
                                     false),
                     PAGE + 64);
    start = fake_reg_log_len;
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_IO);
    assert_int_equal(flushes_since(start), 1);
    assert_int_equal(primed, 0);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_STATE);

    /* Halted and still active, on an endpoint the controller left primed:
     * the driver flushes it */
    assert_int_equal(ops->xfer_start(USB_EP1_IN, arena, 512), PB_OK);
    controller_exec(USB_EP1_IN,
                    host,
                    sizeof(host),
                    0,
                    HOST_ERROR,
                    DTD_TOKEN_STATUS_HALTED | DTD_TOKEN_STATUS_ACTIVE,
                    false);
    set_primed(primed | ep_bit(USB_EP1_IN));
    assert_int_equal(ops->xfer_complete(USB_EP1_IN), -PB_ERR_IO);
    assert_int_equal(primed, 0);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);

    /* A Transaction Error in the second of three descriptors, the last
     * still active */
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena, 3 * DTD_MAX_BYTES), PB_OK);
    controller_exec(USB_EP2_OUT,
                    host,
                    sizeof(host),
                    DTD_MAX_BYTES + 512,
                    HOST_ERROR,
                    DTD_TOKEN_STATUS_TRANSACTION_ERR,
                    false);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_IO);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);

    /* An overflow seen in the queue head's overlay only, Halted (RM Table
     * 42-74), the descriptor in memory still active: not polled as
     * -PB_ERR_AGAIN for ever */
    assert_int_equal(ops->xfer_start(USB_EP2_OUT, arena + 0x200, 512), PB_OK);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_AGAIN);
    {
        uintptr_t qh = (uintptr_t)fake_regs_get(BASE + CI_ENDPTLISTADDR) +
                       USB_EP2_OUT * sizeof(struct imx_ci_udc_queue_head);
        uint32_t token = DTD_TOKEN_STATUS_HALTED | DTD_TOKEN_STATUS_DATA_BUFFER_ERR;

        ram_write(qh + offsetof(struct imx_ci_udc_queue_head, token), &token, sizeof(token));
        set_primed(primed & ~ep_bit(USB_EP2_OUT));
    }
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_IO);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_STATE);

    transfer_out(0x200, 3 * PAGE);
    transfer_in(0x40, 512);

    /* The flush after the error fails: the memory stays the controller's
     * and nothing is started until init */
    assert_int_equal(ops->xfer_start(USB_EP1_IN, arena, 512), PB_OK);
    controller_exec(USB_EP1_IN,
                    host,
                    sizeof(host),
                    0,
                    HOST_ERROR,
                    DTD_TOKEN_STATUS_HALTED | DTD_TOKEN_STATUS_ACTIVE,
                    false);
    set_primed(primed | ep_bit(USB_EP1_IN));
    flush_refusals = UINT32_MAX;
    fake_us_tick_step = 100;
    assert_int_equal(ops->xfer_complete(USB_EP1_IN), -PB_ERR_IO);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS + 1);
    assert_int_equal(ops->xfer_start(USB_EP1_IN, arena, 512), -PB_ERR_IO);

    flush_refusals = 0;
    fake_us_tick_step = 0;
    set_primed(0);
    assert_int_equal(ops->init(), PB_OK);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
    transfer_in(0x40, 512);
}

/* usb.md 2.3, 4.3: a read is done at exactly its length, and a short packet
 * before it is -PB_ERR_IO, the endpoint flushed: in the only descriptor, in
 * the first of several (the controller goes on to the next, which stays
 * active and primed), in the last, and an empty packet where a descriptor
 * starts. A read of whole packets that ends at its length is done, as
 * before (F-137: the short descriptor was retired and PB_OK returned, or,
 * before the last, the transfer waited and the host's next transfer went
 * into it) */
static void test_short_packet_is_an_error(void **state)
{
    static const struct {
        size_t length;
        size_t limit;
    } cases[] = {
        { 512, 100 },
        { 3 * DTD_MAX_BYTES, DTD_MAX_BYTES - 100 },
        { 3 * DTD_MAX_BYTES, 2 * DTD_MAX_BYTES + 1000 },
        { 3 * DTD_MAX_BYTES, DTD_MAX_BYTES },
        { 3 * PAGE, PAGE + 1 },
    };

    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        uint8_t *bfr = (i == 4) ? arena + PAGE + 0x200 : arena + PAGE;
        size_t start;

        assert_int_equal(ops->xfer_start(USB_EP2_OUT, bfr, cases[i].length), PB_OK);
        assert_int_equal(
            controller_exec(USB_EP2_OUT, host, sizeof(host), cases[i].limit, HOST_SHORT, 0, false),
            cases[i].limit);
        start = fake_reg_log_len;
        assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_IO);
        assert_int_equal(flushes_since(start), 1);
        assert_int_equal(primed, 0);
        assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
        assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_STATE);
    }

    transfer_out(0, 2 * DTD_MAX_BYTES);
    transfer_out(0x200, 3 * PAGE);
}

/* A transfer polled while the controller is between its descriptors isn't
 * done, and then is, with every byte. The driver reads the descriptors from
 * the first it hasn't seen retired, and the lines of the ones before are
 * its own again */
static void test_poll_between_descriptors(void **state)
{
    const size_t length = 3 * DTD_MAX_BYTES + 512;
    uint8_t *bfr = arena + PAGE;
    size_t pos = 0;
    (void)state;

    memset(arena, 0xee, sizeof(arena));
    fill(host, length, 11);

    assert_int_equal(ops->xfer_start(USB_EP2_OUT, bfr, length), PB_OK);

    for (int i = 0; i < 3; i++) {
        pos += controller_exec(
            USB_EP2_OUT, host + pos, sizeof(host) - pos, DTD_MAX_BYTES, HOST_PAUSE, 0, i > 0);
        assert_int_equal(pos, (size_t)(i + 1) * DTD_MAX_BYTES);
        assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_AGAIN);
        assert_true(primed & ep_bit(USB_EP2_OUT));
    }

    /* Two descriptors to a line: the first line is retired */
    assert_false(dma_host_in_begun(first_dtd(USB_EP2_OUT)));

    pos +=
        controller_exec(USB_EP2_OUT, host + pos, sizeof(host) - pos, SIZE_MAX, HOST_PAUSE, 0, true);
    assert_int_equal(pos, length);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), PB_OK);
    assert_memory_equal(bfr, host, length);
    assert_int_equal(dma_host_in_open(), NO_OF_DQHS);
}

/* RT1060 RM 42.5.6.1 (the 6UL's 54.4.6.1): after the controller's reset its
 * mode is set first, once, then DEVICEADDR and ENDPTLISTADDR, device mode
 * registers, then Run. USBMODE keeps Stream Disable and the setup lockout
 * off (F-140: ENDPTLISTADDR and DEVICEADDR were written before USBMODE) */
static void test_init_order(void **state)
{
    size_t start = fake_reg_log_len;
    size_t rst = SIZE_MAX;
    size_t mode = SIZE_MAX;
    size_t list = SIZE_MAX;
    size_t addr = SIZE_MAX;
    size_t run = SIZE_MAX;
    (void)state;

    assert_int_equal(ops->init(), PB_OK);

    for (size_t i = start; i < fake_reg_log_len; i++) {
        const struct fake_reg_write *w = &fake_reg_log[i];

        if (w->addr == BASE + CI_USBCMD && (w->value & USBCMD_RST)) {
            assert_int_equal(rst, SIZE_MAX);
            rst = i;
        } else if (w->addr == BASE + CI_USBMODE) {
            assert_int_equal(mode, SIZE_MAX);
            assert_int_equal(w->value, USBMODE_SLOM | USBMODE_CM_DEVICE);
            mode = i;
        } else if (w->addr == BASE + CI_ENDPTLISTADDR && list == SIZE_MAX) {
            list = i;
        } else if (w->addr == BASE + CI_DEVICEADDR && addr == SIZE_MAX) {
            addr = i;
        } else if (w->addr == BASE + CI_USBCMD && (w->value & USBCMD_RS) && run == SIZE_MAX) {
            run = i;
        }
    }

    assert_true(rst < mode);
    assert_true(mode < addr);
    assert_true(mode < list);
    assert_true(list < run);
    assert_true(run != SIZE_MAX);
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
        cmocka_unit_test_setup(test_setup_status_cleared_before_the_copy, setup),
        cmocka_unit_test_setup(test_bus_reset_is_reported, setup),
        cmocka_unit_test_setup(test_status_stage_is_bounded, setup),
        cmocka_unit_test_setup(test_request_during_a_bulk_transfer, setup),
        cmocka_unit_test_setup(test_request_before_the_result, setup),
        cmocka_unit_test_setup(test_result_once, setup),
        cmocka_unit_test_setup(test_ep0_length, setup),
        cmocka_unit_test_setup(test_unplug_is_reported, setup),
        cmocka_unit_test_setup(test_failed_prime, setup),
        cmocka_unit_test_setup(test_failed_prime_on_endpoint_0, setup),
        cmocka_unit_test_setup(test_prime_status_late, setup),
        cmocka_unit_test_setup(test_flush_sees_a_prime, setup),
        cmocka_unit_test_setup(test_unused_endpoints_and_stop, setup),
        cmocka_unit_test_setup(test_prime_done_at_once, setup),
        cmocka_unit_test_setup(test_prime_is_bounded, setup),
        cmocka_unit_test_setup(test_refused_flush_is_repeated, setup),
        cmocka_unit_test_setup(test_flush_is_bounded, setup),
        cmocka_unit_test_setup(test_bus_reset_sequence, setup),
        cmocka_unit_test_setup(test_status_written_back_as_read, setup),
        cmocka_unit_test_setup(test_resets_are_bounded, setup),
        cmocka_unit_test_setup(test_descriptor_error, setup),
        cmocka_unit_test_setup(test_short_packet_is_an_error, setup),
        cmocka_unit_test_setup(test_poll_between_descriptors, setup),
        cmocka_unit_test_setup(test_init_order, setup),
    };

    return cmocka_run_group_tests(tests, group_setup, NULL);
}
