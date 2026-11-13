/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The Cadence USB3 device controller's driver (cdns3_udc_core.c), built for
 * the host on a register file: what it queues for a bulk transfer and which
 * of it it cleans from the cache before the controller is started
 * (drivers.md 4.2). The controller has no model; how it behaves on the bus
 * is the bench's (usb.md 8).
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <arch/arch.h>
#include <drivers/usb/cdns3_udc_core.h>
#include <pb/pb.h>
#include <string.h>

#include "fakes/regs.h"

/* The MEK's controller */
#define CDNS3_BASE     0x5b110000u
#define EP_TRADDR      (CDNS3_BASE + 0x20020)
#define EP_CMD         (CDNS3_BASE + 0x20028)
#define EP_CMD_DRDY    BIT(6)

/* A TRB, and what the driver puts in one and in its ring (cdns3_udc_core.c) */
#define TRB_SIZE       12
#define TRB_MAX_LENGTH (127 * 1024)
#define TRB_COUNT      128
#define TRB_FLAG_CYCLE BIT(0)

/* The cache line the cleans act on (drivers.md 4.1) */
#define LINE           64
#define MAX_CLEANS     16

static struct {
    uintptr_t start;
    uintptr_t end;
} cleans[MAX_CLEANS];

static size_t n_cleans;

/* The transfer's buffer: an address in the MEK's RAM. The driver queues and
 * cleans it and never reads it, and here it is far from the driver's ring,
 * so that its clean says nothing of the ring's */
#define BUF ((void *)(uintptr_t)0x90000000u)

void arch_clean_cache_range(uintptr_t start, size_t len)
{
    if (len == 0)
        return;

    assert_true(n_cleans < MAX_CLEANS);
    cleans[n_cleans].start = start & ~(uintptr_t)(LINE - 1);
    cleans[n_cleans].end = (start + len + LINE - 1) & ~(uintptr_t)(LINE - 1);
    n_cleans++;
}

void arch_invalidate_cache_range(uintptr_t start, size_t len)
{
    (void)start;
    (void)len;
}

const char *ep_to_str(usb_ep_t ep)
{
    (void)ep;
    return "EP";
}

static bool cleaned(uintptr_t addr)
{
    for (size_t i = 0; i < n_cleans; i++) {
        if (addr >= cleans[i].start && addr < cleans[i].end)
            return true;
    }

    return false;
}

static int setup(void **state)
{
    (void)state;
    fake_regs_reset();
    cdns3_udc_core_set_base(CDNS3_BASE);
    n_cleans = 0;
    return 0;
}

/* drivers.md 4.2, item 1: everything the device follows is cleaned before it
 * is started, a descriptor list with its terminating entry. The controller
 * stops at the first TRB whose cycle bit is clear, which the driver writes
 * after the transfer's last one (F-184: the clean left it out, and for some
 * counts its flags are in a line of their own, where memory still had the
 * TRB of a longer transfer before) */
static void test_the_terminating_trb_is_cleaned(void **state)
{
    (void)state;

    for (size_t count = 1; count < TRB_COUNT; count++) {
        const uint32_t *trb;

        fake_regs_reset();
        n_cleans = 0;

        assert_int_equal(cdns3_udc_core_xfer_start(USB_EP2_OUT, BUF, count * TRB_MAX_LENGTH),
                         PB_OK);
        assert_int_equal(fake_regs_get(EP_CMD), EP_CMD_DRDY);

        /* The ring: 'count' TRBs with the cycle bit, then one without */
        trb = (const uint32_t *)(uintptr_t)fake_regs_get(EP_TRADDR);
        assert_non_null(trb);

        for (size_t i = 0; i < count; i++)
            assert_true(trb[3 * i + 2] & TRB_FLAG_CYCLE);

        assert_int_equal(trb[3 * count + 2], 0);

        for (size_t off = 0; off < (count + 1) * TRB_SIZE; off++) {
            if (!cleaned((uintptr_t)trb + off))
                fail_msg("%zu TRBs: byte %zu of the ring isn't cleaned", count, off);
        }
    }
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_the_terminating_trb_is_cleaned, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
