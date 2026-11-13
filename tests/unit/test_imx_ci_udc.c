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
 *  - A data cache (drivers.md 4.2). The controller reads and writes RAM,
 *    the driver the cache. Where the two differ is kept per line: a clean
 *    writes the cache's line to RAM, an invalidate drops the cache's line
 *    for RAM's. A clean that is missing gives the controller old bytes, an
 *    invalidate that is missing or late gives them to the caller.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <arch/arch.h>
#include <drivers/usb/imx_ci_udc.h>
#include <drivers/usb/usbd.h>
#include <pb/pb.h>
#include <string.h>

#include "fakes/regs.h"
#include "imx_ci_udc_private.h"

/* jiffy's controller */
#define BASE         0x02184000u
#define PAGE         DTD_PAGE_SIZE
#define EP_IS_IN(ep) ((ep) & 1)

/* The driver's transfer descriptors with fakes/imx_ci_udc/config.h */
#define NO_OF_DTDS   (1 + CONFIG_CM_BUF_SIZE_KiB / 20)

/*
 * The cache
 */

#define LINE         64
#define MAX_LINES    8192 /* A power of two, and twice what a test tracks */

/* A line whose content in RAM isn't, or wasn't always, the cache's. A hash
 * table on the line's address: 'used' is the test's own, since 0 is no
 * address of a line here */
static struct line {
    bool used;
    uintptr_t addr;
    uint8_t ram[LINE];
} lines[MAX_LINES];

static size_t n_lines;

/* RAM and the cache agree everywhere */
static void cache_reset(void)
{
    memset(lines, 0, sizeof(lines));
    n_lines = 0;
}

static struct line *line_get(uintptr_t addr, bool create)
{
    struct line *l;

    addr &= ~(uintptr_t)(LINE - 1);

    for (size_t i = (addr / LINE) % MAX_LINES;; i = (i + 1) % MAX_LINES) {
        l = &lines[i];

        if (!l->used)
            break;
        if (l->addr == addr)
            return l;
    }

    if (!create)
        return NULL;

    /* Until now RAM and the cache agreed */
    assert_true(n_lines < MAX_LINES / 2);
    n_lines++;
    l->used = true;
    l->addr = addr;
    memcpy(l->ram, (const void *)addr, LINE);
    return l;
}

/* From here on what the CPU writes in the range is in the cache only, until
 * it is cleaned */
static void cache_track(const void *p, size_t length)
{
    uintptr_t addr = (uintptr_t)p;

    for (uintptr_t a = addr & ~(uintptr_t)(LINE - 1); a < addr + length; a += LINE)
        (void)line_get(a, true);
}

void arch_clean_cache_range(uintptr_t start, size_t len)
{
    for (uintptr_t a = start & ~(uintptr_t)(LINE - 1); a < start + len; a += LINE) {
        struct line *l = line_get(a, false);

        if (l != NULL)
            memcpy(l->ram, (const void *)a, LINE);
    }
}

void arch_invalidate_cache_range(uintptr_t start, size_t len)
{
    for (uintptr_t a = start & ~(uintptr_t)(LINE - 1); a < start + len; a += LINE) {
        struct line *l = line_get(a, false);

        if (l != NULL)
            memcpy((void *)a, l->ram, LINE);
    }
}

/* The controller reads RAM */
static void ram_read(uintptr_t addr, void *dst, size_t length)
{
    uint8_t *d = dst;

    for (size_t i = 0; i < length; i++) {
        struct line *l = line_get(addr + i, false);

        d[i] = (l != NULL) ? l->ram[(addr + i) % LINE] : *(const uint8_t *)(addr + i);
    }
}

/* The controller writes RAM, which the cache doesn't see */
static void ram_write(uintptr_t addr, const void *src, size_t length)
{
    const uint8_t *s = src;

    for (size_t i = 0; i < length; i++)
        line_get(addr + i, true)->ram[(addr + i) % LINE] = s[i];
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
    cache_reset();
    primed = 0;
    one_xfer = false;
    ops = NULL;

    assert_int_equal(imx_ci_udc_init(BASE), PB_OK);
    assert_non_null(ops);
    assert_int_equal(ops->init(), PB_OK);

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
    cache_reset();
    cache_track(arena, sizeof(arena));

    assert_int_equal(ops->xfer_start(USB_EP2_OUT, bfr, length), PB_OK);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), -PB_ERR_AGAIN);
    assert_int_equal(controller_run(USB_EP2_OUT, host, length), length);
    assert_int_equal(ops->xfer_complete(USB_EP2_OUT), PB_OK);

    if (memcmp(bfr, host, length) != 0)
        fail_msg("OUT, offset 0x%zx, %zu bytes: not what the host sent", offset, length);

    /* The caller's view and RAM, before the buffer and after its last line */
    arch_invalidate_cache_range((uintptr_t)arena, sizeof(arena));

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
    cache_reset();
    cache_track(arena, sizeof(arena));
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

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_registration, setup),
        cmocka_unit_test_setup(test_page_aligned_buffer, setup),
        cmocka_unit_test_setup(test_zero_length, setup),
        cmocka_unit_test_setup(test_too_long, setup),
        cmocka_unit_test_setup(test_write_from_inside_a_page, setup),
        cmocka_unit_test_setup(test_read_into_a_page, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
