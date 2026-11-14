/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * LIMITATIONS:
 *
 * - Endpoint 0 has a transfer of its own, with its own descriptor and bounce
 *   buffer, and the bulk endpoints have one between them: the stack starts
 *   one bulk transfer at a time (usb.md 2.3). A request on endpoint 0 leaves
 *   the bulk transfer alone (usb.md 3.2, 4.3).
 *
 * - It only supports device mode.
 *
 * - A bulk transfer is at most IMX_CI_UDC_MAX_XFER_BYTES, the driver's own
 *   limit: what its transfer descriptors hold, five 4 KiB pages each. A
 *   longer one is refused (usb.md 2.3, 4.3).
 *
 */

#include <drivers/usb/imx_ci_udc.h>
#include <drivers/usb/usbd.h>
#include <inttypes.h>
#include <pb/delay.h>
#include <pb/dma.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <stdio.h>
#include <string.h>

#include "imx_ci_udc_private.h"

/* Misc defines */
#define IMX_CI_UDC_NO_OF_EPS      8
#define IMX_CI_UDC_SZ_64B         0x40

/*
 * The longest bulk transfer, the driver's own limit (usb.md 2.3, 4.3):
 * command mode's buffer and the boot's piece, 4 MiB on jiffy and the 8DXL
 * EVK, 32 and 4 KiB on the RT1060 EVK. From any start: a buffer that
 * doesn't start on a page has a first descriptor of at least four pages and
 * a byte, so N descriptors of five pages hold N * 20 KiB - 4 KiB.
 */
#define IMX_CI_UDC_MAX_XFER_BYTES (4 * 1024 * 1024)
#define IMX_CI_UDC_NO_OF_DESCRIPTORS \
    ((IMX_CI_UDC_MAX_XFER_BYTES + DTD_PAGE_SIZE + DTD_MAX_BYTES - 1) / DTD_MAX_BYTES)

#ifdef CONFIG_CM_BUF_SIZE_KiB
PB_STATIC_ASSERT((unsigned long long)CONFIG_CM_BUF_SIZE_KiB * 1024 <= IMX_CI_UDC_MAX_XFER_BYTES,
                 "CONFIG_CM_BUF_SIZE_KiB: above the ChipIdea's longest transfer");
#endif
#ifdef CONFIG_BOOT_LOAD_CHUNK_kB
PB_STATIC_ASSERT((unsigned long long)CONFIG_BOOT_LOAD_CHUNK_kB * 1024 <= IMX_CI_UDC_MAX_XFER_BYTES,
                 "CONFIG_BOOT_LOAD_CHUNK_kB: above the ChipIdea's longest transfer");
#endif

/* Endpoint 0's transfers: what the stack sends there is a descriptor or a
 * status, the longest a string descriptor, 255 bytes (usbd.c). Its bounce
 * buffer is aligned to its size, so it is inside one page */
#define IMX_CI_UDC_EP0_MAX_BYTES         512

/* A status stage the host hasn't taken in this time it has left: USB 2.0
 * 9.2.6.4 gives the device 500 ms for each packet of a data stage and 50 ms
 * for the status stage after it, and the host polls for that stage at once.
 * The DWC3's driver waits as long (DWC3_EP0_TIMEOUT_us) */
#define IMX_CI_UDC_EP0_TIMEOUT_us        500000

/*
 * A prime or a flush of an endpoint. The RM gives no time: a prime's
 * "varies based upon the current USB traffic" (i.MX RT1060 RM rev. 3
 * 42.7.37, the 6UL's 54.6.37), and a flush waits for the packet in progress
 * on the endpoint (42.5.6.6.5). A packet takes 50 us at most at full speed
 * (64 bytes) and 10 us at high speed (512 bytes); 10 ms is ten full speed
 * frames. Linux's chipidea waits for both without a bound (hw_ep_prime(),
 * hw_ep_flush(), drivers/usb/chipidea/udc.c); the DWC3's driver gives its
 * endpoint commands 10 ms (DWC3_CMD_TIMEOUT_us)
 */
#define IMX_CI_UDC_ENDPT_TIMEOUT_us      10000

/* The controller's reset, USBCMD.RST. The RM gives no time (42.7.18).
 * Linux's chipidea gives it 10 ms (hw_controller_reset(),
 * drivers/usb/chipidea/core.c), U-Boot's EHCI 250 ms (ehci_reset()); the
 * longer */
#define IMX_CI_UDC_RESET_TIMEOUT_us      250000

/* A bus reset the host drives, PORTSC1.PR: 50 ms at most from a root port
 * (USB 2.0 7.1.7.5, TDRSTR), and twice that */
#define IMX_CI_UDC_PORT_RESET_TIMEOUT_us 100000

#define EP_IS_IN(ep)                     ((ep) & 1)
#define EP_USB_NUMBER(ep)                ((ep) >> 1)

/* bEndpointAddress in an endpoint descriptor */
#define USB_EP_ADDR_NUMBER_MASK          0x0f
#define USB_EP_ADDR_DIR_IN               0x80

/*
 * What the controller reads and writes of the driver's own (drivers.md 4.2,
 * 5). No data structure seen by the controller should span a 4k page
 * boundary: a descriptor is half a cache line and a queue head is one, the
 * bulk endpoints' bounce buffer is a page and endpoint 0's is inside one.
 *
 * The controller writes a queue head whenever it likes while it runs: the
 * overlay during a transfer, the setup packet when one arrives. The queue
 * heads are therefore the controller's from init to stop ('queues_dma'), and
 * the driver takes one back for as long as it reads or changes it
 * (qh_take(), qh_give()).
 */
PB_DMA_ARRAY_ALIGNED(struct imx_ci_udc_transfer_head, dtds, IMX_CI_UDC_NO_OF_DESCRIPTORS, 4096);
PB_DMA_ARRAY_ALIGNED(struct imx_ci_udc_queue_head, dqhs, IMX_CI_UDC_NO_OF_EPS * 2, 4096);
PB_DMA_ARRAY_ALIGNED(uint8_t, align_buffer, DTD_PAGE_SIZE, 4096);
PB_DMA_ARRAY(struct imx_ci_udc_transfer_head, ep0_dtd, 1);
PB_DMA_ARRAY_ALIGNED(uint8_t, ep0_bounce, IMX_CI_UDC_EP0_MAX_BYTES, IMX_CI_UDC_EP0_MAX_BYTES);
static bool queues_dma;

/*
 * A transfer, and what it has of its own. Endpoint 0 has one, and the bulk
 * endpoints have one: a transfer on endpoint 0 writes nothing of the bulk
 * transfer's, its descriptors, its bounce buffer or its queue head, so the
 * bulk transfer goes on where it was (usb.md 3.2, 4.3; F-203).
 */
struct imx_ci_udc_xfer {
    struct imx_ci_udc_transfer_head *dtds;
    size_t no_of_dtds;
    uint8_t *bounce;
    size_t bounce_size;
    size_t max_length;
    uintptr_t bfr;
    size_t length;
    /* The last descriptor, which xfer_complete polls. NULL when no transfer
     * was started, or its result was given, or it was given up */
    struct imx_ci_udc_transfer_head *head;
    /* The first descriptor the driver hasn't seen retired: xfer_complete
     * reads the descriptors from here, and the ones before are done */
    struct imx_ci_udc_transfer_head *next;
    usb_ep_t ep;
    /* The transfer's descriptors from 'next' on, and an OUT transfer's
     * buffer and the bounce buffer, are the controller's: begun, and not
     * ended yet */
    bool dma;
};

static struct imx_ci_udc_xfer ep0_xfer = {
    .dtds = ep0_dtd,
    .no_of_dtds = 1,
    .bounce = ep0_bounce,
    .bounce_size = IMX_CI_UDC_EP0_MAX_BYTES,
    .max_length = IMX_CI_UDC_EP0_MAX_BYTES,
};

static struct imx_ci_udc_xfer bulk_xfer = {
    .dtds = dtds,
    .no_of_dtds = IMX_CI_UDC_NO_OF_DESCRIPTORS,
    .bounce = align_buffer,
    .bounce_size = DTD_PAGE_SIZE,
    .max_length = IMX_CI_UDC_MAX_XFER_BYTES,
};

static uintptr_t imx_ci_udc_base;
/* The host configured the device, and reset the bus or was unplugged after
 * that: every bulk transfer fails until the device is configured or started
 * again (usb.md 2.4, 4.3) */
static bool configured;
static bool lost;
/* The controller had VBUS when the host configured the device: its loss is
 * an unplug */
static bool vbus_seen;
/* The endpoints that were enabled, endpoint 0, the configured ones and any
 * the driver primed: a flush waits for their bits only. Another endpoint is
 * never primed, and the bits of one the controller doesn't have may never
 * clear */
static uint32_t endpoints_enabled;
/* A flush of an endpoint with a transfer failed: the controller may still
 * write the transfer's memory, which stays its own, and no transfer is
 * started until init or stop reset the controller (drivers.md 3, 4.2) */
static bool held;

/* The bit of an endpoint in ENDPTPRIME, ENDPTFLUSH, ENDPTSTAT and ENDPTCOMPLETE */
static uint32_t ep_bit(usb_ep_t ep)
{
    if (EP_IS_IN(ep))
        return ENDPT_TX(EP_USB_NUMBER(ep));
    else
        return ENDPT_RX(EP_USB_NUMBER(ep));
}

/* Waits until the register's 'mask' bits are clear, bounded (drivers.md 3) */
static int ci_wait_clear(uint32_t reg, uint32_t mask, uint32_t timeout_us)
{
    struct pb_timeout to;
    int rc;

    rc = pb_timeout_init_us(&to, timeout_us);

    if (rc != PB_OK)
        return rc;

    while (mmio_read_32(imx_ci_udc_base + reg) & mask) {
        if (pb_timeout_has_expired(&to)) {
            LOG_ERR("Timeout: 0x%03" PRIx32 " & 0x%08" PRIx32, reg, mask);
            return -PB_ERR_TIMEOUT;
        }
    }

    return PB_OK;
}

/*
 * The enabled endpoints of 'bits' are flushed (RT1060 RM 42.5.6.6.5, the
 * 6UL's 54.4.6.6.5): written to ENDPTFLUSH, waited for, and written again
 * while ENDPTSTAT shows one still primed: the controller refuses the flush
 * of an endpoint with a packet in progress. Linux's hw_ep_flush() is the same
 * loop. Done also needs ENDPTPRIME clear: the controller primes a transfer's
 * next descriptor by itself, and ENDPTSTAT is clear for that moment
 * (42.7.35, 42.7.37). The first write isn't held back by a prime that runs,
 * or by one that never ends. One bound for all of it.
 */
static int imx_ci_udc_flush(uint32_t bits)
{
    struct pb_timeout to;
    int rc;

    bits &= endpoints_enabled;

    rc = pb_timeout_init_us(&to, IMX_CI_UDC_ENDPT_TIMEOUT_us);

    if (rc != PB_OK)
        return rc;

    for (;;) {
        mmio_write_32(imx_ci_udc_base + CI_ENDPTFLUSH, bits);

        while (mmio_read_32(imx_ci_udc_base + CI_ENDPTFLUSH) & bits) {
            if (pb_timeout_has_expired(&to))
                goto timeout;
        }

        if (!(mmio_read_32(imx_ci_udc_base + CI_ENDPTSTAT) & bits) &&
            !(mmio_read_32(imx_ci_udc_base + CI_ENDPTPRIME) & bits))
            return PB_OK;

        if (pb_timeout_has_expired(&to))
            goto timeout;
    }

timeout:
    LOG_ERR("Flush of 0x%08" PRIx32 ": timeout", bits);
    return -PB_ERR_TIMEOUT;
}

/* Endpoint 0's transfer, or the bulk endpoints' */
static struct imx_ci_udc_xfer *xfer_of(usb_ep_t ep)
{
    return (EP_USB_NUMBER(ep) == 0) ? &ep0_xfer : &bulk_xfer;
}

/* The cache line of the transfer's last descriptor, and the descriptors the
 * controller may still write before that line: from the line of the first
 * one that isn't seen retired */
static void *xfer_head_line(const struct imx_ci_udc_xfer *x)
{
    return (void *)((uintptr_t)x->head & ~((uintptr_t)PB_DMA_ALIGN - 1));
}

static void *xfer_next_line(const struct imx_ci_udc_xfer *x)
{
    return (void *)((uintptr_t)x->next & ~((uintptr_t)PB_DMA_ALIGN - 1));
}

static size_t xfer_dtds_before_head(const struct imx_ci_udc_xfer *x)
{
    return (uintptr_t)xfer_head_line(x) - (uintptr_t)xfer_next_line(x);
}

static bool xfer_is_out(const struct imx_ci_udc_xfer *x)
{
    return !EP_IS_IN(x->ep) && x->bfr != 0 && x->length != 0;
}

/* The controller gets the transfer's descriptors, and what an OUT transfer
 * writes: the caller's buffer, which owns its lines (usb.md 4.3), and the
 * bounce buffer for a head that isn't on a page */
static void xfer_dtds_begin(struct imx_ci_udc_xfer *x)
{
    dma_in_begin(xfer_next_line(x), xfer_dtds_before_head(x));
    dma_in_begin(xfer_head_line(x), PB_DMA_ALIGN);
}

static void xfer_dtds_end(struct imx_ci_udc_xfer *x)
{
    dma_in_end(xfer_next_line(x), xfer_dtds_before_head(x));
    dma_in_end(xfer_head_line(x), PB_DMA_ALIGN);
}

static void xfer_dma_begin(struct imx_ci_udc_xfer *x)
{
    xfer_dtds_begin(x);

    if (xfer_is_out(x)) {
        dma_in_begin((void *)x->bfr, x->length);

        if (x->bfr & DTD_PAGE_OFFSET_MASK)
            dma_in_begin(x->bounce, x->bounce_size);
    }

    x->dma = true;
}

/* The transfer is done, given up, or the controller is stopped: its memory
 * is the driver's and the caller's again, and read from RAM (drivers.md
 * 4.2). 'descriptors' is false when they were ended already */
static void xfer_dma_end(struct imx_ci_udc_xfer *x, bool descriptors)
{
    if (!x->dma)
        return;

    if (descriptors)
        xfer_dtds_end(x);

    if (xfer_is_out(x)) {
        dma_in_end((void *)x->bfr, x->length);

        if (x->bfr & DTD_PAGE_OFFSET_MASK)
            dma_in_end(x->bounce, x->bounce_size);
    }

    x->dma = false;
}

/* The transfer is given up, its endpoint flushed: nothing of it completes
 * later, and a poll of it is -PB_ERR_STATE (usb.md 4.3) */
static void xfer_give_up(struct imx_ci_udc_xfer *x)
{
    xfer_dma_end(x, true);
    x->head = NULL;
}

/* The transfer is given up after its endpoint's flush, 'flush_rc'. A flush
 * that failed leaves the transfer's memory the controller's, as it was
 * begun: the session is lost, and every transfer fails until init or stop
 * take it back */
static void xfer_abandon(struct imx_ci_udc_xfer *x, int flush_rc)
{
    if (flush_rc == PB_OK) {
        xfer_give_up(x);
        return;
    }

    held = true;
    lost = true;
}

/* The driver reads or changes a queue head: taken back from the controller,
 * which may have written it, and given again */
static void qh_take(struct imx_ci_udc_queue_head *qh)
{
    if (queues_dma)
        dma_in_end(qh, sizeof(*qh));
}

static void qh_give(struct imx_ci_udc_queue_head *qh)
{
    if (queues_dma)
        dma_in_begin(qh, sizeof(*qh));
}

/* The controller is stopped or in reset: nothing of the driver's memory is
 * its own */
static void imx_ci_udc_dma_take_back(void)
{
    xfer_dma_end(&ep0_xfer, true);
    xfer_dma_end(&bulk_xfer, true);

    for (int i = 0; queues_dma && i < IMX_CI_UDC_NO_OF_EPS * 2; i++)
        dma_in_end(&dqhs[i], sizeof(dqhs[i]));

    queues_dma = false;
    held = false;
}

static void imx_ci_udc_reset_queues(void)
{
    for (int i = 0; i < IMX_CI_UDC_NO_OF_EPS * 2; i++) {
        dqhs[i].caps = 0;
        dqhs[i].next = DTD_NEXT_T;
        dqhs[i].token = 0;
        dqhs[i].current_dtd = 0;
    }

    ep0_xfer.head = NULL;
    bulk_xfer.head = NULL;
    configured = false;
    lost = false;
    vbus_seen = false;
    endpoints_enabled = ENDPT_TX(0) | ENDPT_RX(0);
}

/* Every status is cleared and endpoint 0 flushed, after the controller's
 * reset, and a reset of the port is waited for */
static int imx_ci_udc_reset(void)
{
    int rc;

    mmio_write_32(imx_ci_udc_base + CI_USBSTS, USBSTS_ALL);
    mmio_write_32(imx_ci_udc_base + CI_ENDPTSETUPSTAT, ENDPTSETUPSTAT_ALL);
    mmio_write_32(imx_ci_udc_base + CI_ENDPTCOMPLETE, ENDPT_ALL);

    rc = imx_ci_udc_flush(ENDPT_ALL);

    if (rc != PB_OK)
        return rc;

    return ci_wait_clear(CI_PORTSC1, PORTSC1_PR, IMX_CI_UDC_PORT_RESET_TIMEOUT_us);
}

static void imx_ci_udc_config_ep(usb_ep_t ep, uint32_t size, uint32_t flags)
{
    struct imx_ci_udc_queue_head *qh = &dqhs[ep];

    qh_take(qh);
    qh->caps = DQH_CAP_ZLT | DQH_CAP_MAX_PACKET_LEN(size) | flags;
    qh_give(qh);
}

/*
 * At a bus reset or an unplug every endpoint is flushed, and a transfer's
 * buffer is the caller's again (usb.md 4.3). The flush isn't waited for, as
 * Linux's hw_usb_reset() (drivers/usb/chipidea/udc.c): at a bus reset the
 * controller has cancelled every prime itself (RT1060 RM 42.5.6.2.1), and
 * after an unplug there is no host to move a packet. The RM's wait for
 * ENDPTPRIME to clear before it is kept, bounded, for the enabled
 * endpoints; at its end the flush is written anyway. Memory a failed flush
 * left the controller's stays so.
 */
static void imx_ci_udc_flush_all(void)
{
    (void)ci_wait_clear(CI_ENDPTPRIME, endpoints_enabled, IMX_CI_UDC_ENDPT_TIMEOUT_us);
    mmio_write_32(imx_ci_udc_base + CI_ENDPTFLUSH, ENDPT_ALL);

    if (!held) {
        xfer_dma_end(&ep0_xfer, true);
        xfer_dma_end(&bulk_xfer, true);
    }
}

static int imx_ci_udc_irq_process(void)
{
    uint32_t tmp;
    uint32_t sts = mmio_read_32(imx_ci_udc_base + CI_USBSTS);
    uint32_t epc = mmio_read_32(imx_ci_udc_base + CI_ENDPTCOMPLETE);

    /* Write one to clear: what was read, and nothing that came after the
     * read (a bus reset there is seen at the next poll) */
    if (sts != 0)
        mmio_write_32(imx_ci_udc_base + CI_USBSTS, sts);

    if (epc != 0)
        mmio_write_32(imx_ci_udc_base + CI_ENDPTCOMPLETE, epc);

    /*
     * A bus reset, as the RM's sequence (RT1060 RM 42.5.6.2.1, the 6UL's
     * 54.4.6.2.1): the setup and the complete status cleared with what is
     * read, the primes waited for and every endpoint flushed, the flush
     * not waited for (imx_ci_udc_flush_all()). Not done:
     * the RM's check that PORTSC1.PR is still set, with a reset of the
     * controller when it isn't, which needs the driver at the reset within
     * its 3 ms. A polling driver often isn't, and a reset of the controller
     * detaches the device. Linux's chipidea doesn't check it either
     * (hw_usb_reset(), drivers/usb/chipidea/udc.c).
     */
    if (sts & USBSTS_URI) {
        tmp = mmio_read_32(imx_ci_udc_base + CI_ENDPTSETUPSTAT);
        mmio_write_32(imx_ci_udc_base + CI_ENDPTSETUPSTAT, tmp);

        tmp = mmio_read_32(imx_ci_udc_base + CI_ENDPTCOMPLETE);
        mmio_write_32(imx_ci_udc_base + CI_ENDPTCOMPLETE, tmp);

        imx_ci_udc_flush_all();

        mmio_write_32(imx_ci_udc_base + CI_DEVICEADDR, 0);

        LOG_DBG("Bus reset");

        /* Before the device is configured a reset is the host's
         * enumeration and no error (usb.md 2.2) */
        if (configured)
            lost = true;
    }

    /*
     * An unplug of the configured device: VBUS, which the controller had
     * when the host configured it, is gone (usb.md 2.4, 4.3). What the
     * controller has for it in device mode is the OTG status's B session
     * valid, a comparator of the PHY's; Linux's chipidea driver reads the
     * same bit for it (drivers/usb/chipidea/otg.c, ci_handle_vbus_change()).
     * Linux's device side doesn't use PORTSC's connect status for it, and
     * the suspend that follows an unplug is a host's suspend too. Read only
     * while it can change something. A board whose VBUS doesn't reach the
     * controller never has the bit, and nothing is reported, as before: an
     * unplug there shows as the next host's bus reset.
     */
    if (vbus_seen && !(mmio_read_32(imx_ci_udc_base + CI_OTGSC) & OTGSC_BSV)) {
        imx_ci_udc_flush_all();
        LOG_DBG("Unplugged");
        vbus_seen = false;
        lost = true;
    }

    if (sts & USBSTS_UEI)
        LOG_ERR("IMX_CI_UDC: Error %" PRIx32, sts);

    return PB_OK;
}

/*
 * The transfer's endpoint is primed (RT1060 RM 42.5.6.6.3, case 1, and
 * 42.5.6.4.2.2; the 6UL's 54.4.6.6.3, 54.4.6.4.2.2). The prime is done when
 * ENDPTPRIME's bit is clear and ENDPTSTAT's is set, or when the controller
 * has retired the first descriptor already: a short IN transfer or a status
 * stage the host took at once, or the controller priming the next
 * descriptor itself, when ENDPTSTAT is clear for a moment (42.7.37's note).
 * Linux's hw_ep_prime() doesn't look at ENDPTSTAT, its comment says because
 * it "doesn't work": so it is polled, and a prime fails only when none of
 * that came within the bound. The RM's causes of a failed prime: the queue
 * head or a descriptor set up wrong (F-130 was), or a SETUP during the
 * prime; and a bus reset cancels every prime (42.5.6.2.1). A SETUP or a bus
 * reset that is waiting ends the poll at once. A failed prime's descriptor
 * stays active for ever, so the transfer is given up, its endpoint flushed.
 *
 * On endpoint 0 a failed prime with a SETUP or a bus reset waiting is the
 * host that left the control transfer (42.5.6.4.2.2: "the transfer
 * descriptor can be freed and the DCD must reinterpret the setup packet"):
 * -PB_ERR_TIMEOUT, what the stage's bound would have returned, and which
 * the stack takes as a host that started over (usbd_connect(), usb.md 2.2).
 * A prime that never ends is -PB_ERR_TIMEOUT too, any other failure
 * -PB_ERR_IO.
 */
static int imx_ci_udc_prime(struct imx_ci_udc_xfer *x)
{
    uint32_t bit = ep_bit(x->ep);
    bool ep0 = EP_USB_NUMBER(x->ep) == 0;
    struct pb_timeout to;
    int rc;

    rc = pb_timeout_init_us(&to, IMX_CI_UDC_ENDPT_TIMEOUT_us);

    if (rc != PB_OK)
        return rc;

    endpoints_enabled |= bit;
    mmio_write_32(imx_ci_udc_base + CI_ENDPTPRIME, bit);

    for (;;) {
        if (!(mmio_read_32(imx_ci_udc_base + CI_ENDPTPRIME) & bit)) {
            uint32_t token;

            if (mmio_read_32(imx_ci_udc_base + CI_ENDPTSTAT) & bit)
                return PB_OK;

            xfer_dtds_end(x);
            token = x->dtds[0].token;
            xfer_dtds_begin(x);

            if (!(token & DTD_TOKEN_STATUS_ACTIVE))
                return PB_OK;

            if (mmio_read_32(imx_ci_udc_base + CI_USBSTS) & USBSTS_URI) {
                rc = ep0 ? -PB_ERR_TIMEOUT : -PB_ERR_IO;
                break;
            }

            if (ep0 && (mmio_read_32(imx_ci_udc_base + CI_ENDPTSETUPSTAT) & ENDPTSETUPSTAT_EP(0))) {
                rc = -PB_ERR_TIMEOUT;
                break;
            }
        }

        if (pb_timeout_has_expired(&to)) {
            rc = (mmio_read_32(imx_ci_udc_base + CI_ENDPTPRIME) & bit) ? -PB_ERR_TIMEOUT
                                                                       : -PB_ERR_IO;
            break;
        }
    }

    LOG_ERR("EP%u %s: the prime failed (%i)",
            (unsigned int)EP_USB_NUMBER(x->ep),
            EP_IS_IN(x->ep) ? "IN" : "OUT",
            rc);

    /* Nothing of it completes later (drivers.md 3) */
    xfer_abandon(x, imx_ci_udc_flush(bit));
    return rc;
}

/**
 * Note: This function will accept buffers of any alignment but for optimal
 * performace all buffers should be 4k aligned.
 */
static int imx_ci_udc_xfer_start(usb_ep_t ep, void *bfr_, size_t length)
{
    struct imx_ci_udc_xfer *x;
    struct imx_ci_udc_queue_head *qh;
    struct imx_ci_udc_transfer_head *dtd;
    size_t bytes_to_tx = length;
    size_t align_length = 0;
    size_t align_offset = 0;
    uintptr_t bfr = (uintptr_t)bfr_;
    uintptr_t buf_ptr = bfr;

    if (ep >= USB_EP_END)
        return -PB_ERR_PARAM;

    x = xfer_of(ep);
    qh = &dqhs[ep];
    dtd = x->dtds;

    /* A flush failed: the memory is the controller's until init or stop */
    if (held)
        return -PB_ERR_IO;

    /* A bus reset or an unplug while no transfer ran is the error of the
     * next one. Endpoint 0 goes on: the host enumerates */
    if (lost && ep >= USB_EP1_OUT)
        return -PB_ERR_IO;

    /* Longer than the transfer's descriptors or endpoint 0's bounce buffer
     * hold: refused, never shortened (usb.md 4.3) */
    if (length > x->max_length)
        return -PB_ERR_PARAM;

    /* The endpoint's transfer that wasn't completed or cancelled ends here,
     * and its descriptors are the driver's to write. Its endpoint is flushed
     * first: the queue head is changed only on an endpoint that isn't primed
     * (RT1060 RM 42.5.6.5.1). The other transfer isn't touched */
    if (x->head != NULL) {
        int rc = imx_ci_udc_flush(ep_bit(x->ep));

        if (rc != PB_OK) {
            xfer_abandon(x, rc);
            return rc;
        }
    }

    xfer_dma_end(x, true);
    x->head = NULL;

    if (bytes_to_tx == 0) {
        dtd->next = DTD_NEXT_T;
        dtd->token = DTD_TOKEN_STATUS_ACTIVE | DTD_TOKEN_IOC;
    } else if (bfr && EP_IS_IN(ep)) {
        dma_out(bfr_, length);
    }

    /* Check and align input buffer if needed */
    if ((length > 0) && (bfr & DTD_PAGE_OFFSET_MASK)) {
        align_length = (DTD_PAGE_SIZE - (bfr & DTD_PAGE_OFFSET_MASK)) & DTD_PAGE_OFFSET_MASK;
        align_length = (length > DTD_PAGE_SIZE) ? align_length : length;
        /* The controller fills page 0 from its offset up to the page's end
         * before it goes on to page 1 (RM 54.4.5, Table 54-63). A head that
         * more pages follow therefore ends at the bounce buffer's end */
        align_offset = (length > DTD_PAGE_SIZE) ? DTD_PAGE_SIZE - align_length : 0;
        /* LOG_DBG("Aligning buffer <%p> (%zu bytes)", (void *) bfr, align_length); */
        /* If this is an IN transfer we need to fill the align buffer */
        if (EP_IS_IN(ep)) {
            memcpy(x->bounce + align_offset, (void *)bfr, align_length);
            dma_out(x->bounce + align_offset, align_length);
        }
    }

    while (bytes_to_tx) {
        /* The descriptor that starts with the bounced head holds what is
         * left of page 0 and four pages */
        size_t dtd_max_bytes = DTD_MAX_BYTES - (align_length ? align_offset : 0);

        dtd->next = DTD_NEXT_T;
        dtd->token = DTD_TOKEN_STATUS_ACTIVE;

        if (bytes_to_tx > dtd_max_bytes) {
            dtd->token |= DTD_TOKEN_TOTAL_BYTES(dtd_max_bytes);
        } else {
            dtd->token |= DTD_TOKEN_TOTAL_BYTES(bytes_to_tx);
        }

        for (int n = 0; n < DTD_NO_OF_PAGES; n++) {
            /* Only the bounced head starts inside its page */
            uint32_t page_offset = 0;

            dtd->page[n] = (uint32_t)buf_ptr;

            if (align_length) {
                /* If we need to align the input buffer, it will be at most
                 * one 4k page, and the first one only */
                dtd->page[n] = (uint32_t)(uintptr_t)(x->bounce + align_offset);
                page_offset = dtd->page[n] & DTD_PAGE_OFFSET_MASK;
                bytes_to_tx -= align_length;
                buf_ptr += align_length;
                align_length = 0;
            } else if (bytes_to_tx >= DTD_PAGE_SIZE) {
                buf_ptr += DTD_PAGE_SIZE;
                bytes_to_tx -= DTD_PAGE_SIZE;
            } else {
                buf_ptr += bytes_to_tx;
                bytes_to_tx = 0;
            }

            if ((dtd->page[n] & DTD_PAGE_OFFSET_MASK) != page_offset)
                return -PB_ERR_ALIGN;

            if (bytes_to_tx == 0) {
                buf_ptr = 0;
                dtd->token |= DTD_TOKEN_IOC;
                break;
            }
        }

        if (bytes_to_tx) {
            struct imx_ci_udc_transfer_head *dtd_prev = dtd;
            dtd++;
            /* Not reached: max_length fits the descriptors */
            if (dtd == &x->dtds[x->no_of_dtds])
                return -PB_ERR_PARAM;
            dtd_prev->next = (uint32_t)(uintptr_t)dtd;
        }
    }

    /* The controller writes the dQH overlay, so a cached copy of it can be
     * stale and the clean below would write an old, active overlay back over
     * the controller's, which it then resumes instead of fetching 'next'.
     * Drop the cached copy first, and clear the overlay's active and halted
     * bits before priming (RM 54.4.6.6.3, an empty list). */
    qh_take(qh);
    qh->next = (uint32_t)(uintptr_t)x->dtds;
    qh->token = 0;
    qh_give(qh);

    x->bfr = (uintptr_t)bfr;
    x->length = length;
    x->head = dtd;
    x->next = x->dtds;
    x->ep = ep;

    /* The transfer's descriptors are cleaned, and are the controller's with
     * what it writes */
    dma_out(x->dtds, (size_t)(dtd + 1 - x->dtds) * sizeof(*dtd));
    xfer_dma_begin(x);

    return imx_ci_udc_prime(x);
}

/*
 * The transfer's result, from its descriptors, which the driver has ended
 * (RT1060 RM 42.5.6.6.4 and 42.5.6.4.1; the 6UL's 54.4.6.6.4, 54.4.6.4.1).
 * Read from the first one not seen retired, in order, as the controller
 * retires them:
 *
 *  - Halted, Data Buffer Error or Transaction Error is the transfer's
 *    error, also on a descriptor still active: an OUT packet longer than
 *    what is left (an overflow) sets Data Buffer Error and the controller
 *    flushes the endpoint (42.5.6.4.1). Linux's chipidea reads the same
 *    bits (_hardware_dequeue()), its Freescale driver before Active
 *    (process_ep_req(), drivers/usb/gadget/udc/fsl_udc_core.c).
 *  - Active: not done.
 *  - Retired with bytes left in Total Bytes: the controller retires an OUT
 *    descriptor at a short packet, "a successful transfer completion"
 *    (42.5.6.4.1), and goes on to the next one, which stays active. A read
 *    is done at exactly its length and fewer bytes is an error (usb.md
 *    2.3, 4.3); an IN descriptor with bytes left is one too.
 *
 * A read of whole packets that ends at its length retires every descriptor
 * with nothing left, as before: what the host sends after it waits for the
 * next read (usb.md 2.3).
 */
static int xfer_result(struct imx_ci_udc_xfer *x)
{
    for (;;) {
        uint32_t token = x->next->token;
        uint32_t left = (token & DTD_TOKEN_TOTAL_BYTES_MASK) >> DTD_TOKEN_TOTAL_BYTES_SHIFT;
        unsigned int n = (unsigned int)(x->next - x->dtds);

        if (token & DTD_TOKEN_STATUS_ERRORS) {
            LOG_ERR("EP%u %s: descriptor %u, status 0x%02" PRIx32,
                    (unsigned int)EP_USB_NUMBER(x->ep),
                    EP_IS_IN(x->ep) ? "IN" : "OUT",
                    n,
                    token & DTD_TOKEN_STATUS_MASK);
            return -PB_ERR_IO;
        }

        if (token & DTD_TOKEN_STATUS_ACTIVE) {
            struct imx_ci_udc_queue_head *qh = &dqhs[x->ep];
            uint32_t overlay;

            /* The controller works in the queue head's overlay, and an
             * overflow sets the Halt bit there (RT1060 RM Table 42-74),
             * whether it wrote the descriptor back or not. xfer_start
             * cleared the overlay's status */
            qh_take(qh);
            overlay = qh->token;
            qh_give(qh);

            if (overlay & DTD_TOKEN_STATUS_ERRORS) {
                LOG_ERR("EP%u %s: descriptor %u, queue head status 0x%02" PRIx32,
                        (unsigned int)EP_USB_NUMBER(x->ep),
                        EP_IS_IN(x->ep) ? "IN" : "OUT",
                        n,
                        overlay & DTD_TOKEN_STATUS_MASK);
                return -PB_ERR_IO;
            }

            return -PB_ERR_AGAIN;
        }

        if (left != 0) {
            LOG_ERR("EP%u %s: a short packet, %" PRIu32 " bytes of descriptor %u not moved",
                    (unsigned int)EP_USB_NUMBER(x->ep),
                    EP_IS_IN(x->ep) ? "IN" : "OUT",
                    left,
                    n);
            return -PB_ERR_IO;
        }

        if (x->next == x->head)
            return PB_OK;

        x->next++;
    }
}

static int imx_ci_udc_xfer_complete(usb_ep_t ep)
{
    struct imx_ci_udc_xfer *x;
    int rc;

    if (ep >= USB_EP_END)
        return -PB_ERR_PARAM;

    rc = imx_ci_udc_irq_process();

    if (rc != PB_OK)
        return rc;

    /* A bus reset or an unplug ends a bulk transfer: lost. A flush that
     * failed ends every transfer until init or stop */
    if ((lost && ep >= USB_EP1_OUT) || held)
        return -PB_ERR_IO;

    x = xfer_of(ep);

    /* Nothing was started on the endpoint, or its result was given: it is
     * given once (usb.md 2.4, 4.3) */
    if (x->head == NULL || x->ep != ep)
        return -PB_ERR_STATE;

    /* The descriptors are read, and are the controller's again while the
     * transfer runs */
    if (x->dma)
        xfer_dtds_end(x);

    rc = xfer_result(x);

    if (rc != PB_OK) {
        if (x->dma)
            xfer_dtds_begin(x);

        /* A transfer that failed is given up, its endpoint flushed:
         * nothing of it completes later (usb.md 4.3, drivers.md 3) */
        if (rc != -PB_ERR_AGAIN) {
            xfer_abandon(x, imx_ci_udc_flush(ep_bit(ep)));
        }

        return rc;
    }

    /* Output from host, invalidate cache: the caller's buffer and the
     * bounce buffer. Before the copy below, which is in the cache only: an
     * invalidate after it would drop it (drivers.md 4.2) */
    xfer_dma_end(x, false);
    x->head = NULL;

    /* Check for un-aligned buffers */
    if (x->length && (x->bfr & DTD_PAGE_OFFSET_MASK)) {
        size_t align_length = (DTD_PAGE_SIZE - (x->bfr & DTD_PAGE_OFFSET_MASK)) &
                              DTD_PAGE_OFFSET_MASK;
        align_length = (x->length > DTD_PAGE_SIZE) ? align_length : x->length;
        /* As in imx_ci_udc_xfer_start() */
        size_t align_offset = (x->length > DTD_PAGE_SIZE) ? DTD_PAGE_SIZE - align_length : 0;

        /* If this was an out transfer we should copy data from the
         * alignment buffer */
        if (!EP_IS_IN(ep)) {
            /* LOG_DBG("Align: copying %zu bytes to %p", align_length,
                    (void *) x->bfr); */
            memcpy((void *)x->bfr, x->bounce + align_offset, align_length);
        }
    }

    return PB_OK;
}

static void imx_ci_udc_flush_ep(usb_ep_t ep)
{
    struct imx_ci_udc_xfer *x;
    int rc;

    if (ep >= USB_EP_END)
        return;

    rc = imx_ci_udc_flush(ENDPT_TX(EP_USB_NUMBER(ep)) | ENDPT_RX(EP_USB_NUMBER(ep)));

    /* The endpoint is idle: its transfer is given up and its buffer is the
     * caller's again (usb.md 4.3). A flush the controller didn't do within
     * the bound: the controller failed, the memory stays its own and the
     * transfers fail until init, which resets it */
    x = xfer_of(ep);

    if (x->head != NULL && EP_USB_NUMBER(x->ep) == EP_USB_NUMBER(ep)) {
        xfer_abandon(x, rc);
    } else if (rc != PB_OK) {
        held = true;
        lost = true;
    }
}

static int imx_ci_udc_set_configuration(const struct usb_endpoint_descriptor *eps, size_t no_of_eps)
{
    enum usb_ep_type ep_type;
    usb_ep_t ep;
    uint16_t max_pkt_sz;
    uintptr_t epctrl;
    uint32_t ep_type_val;
    uint32_t ep_reg;

    for (size_t n = 0; n < no_of_eps; n++) {
        ep = (eps[n].bEndpointAddress & USB_EP_ADDR_NUMBER_MASK) * 2;
        if (eps[n].bEndpointAddress & USB_EP_ADDR_DIR_IN)
            ep++;

        ep_type = eps[n].bmAttributes + 1;
        max_pkt_sz = eps[n].wMaxPacketSize;
        switch (ep_type) {
        case USB_EP_TYPE_CONTROL:
            ep_type_val = ENDPTCTRL_TYPE_CONTROL;
            break;
        case USB_EP_TYPE_ISO:
            ep_type_val = ENDPTCTRL_TYPE_ISO;
            break;
        case USB_EP_TYPE_BULK:
            ep_type_val = ENDPTCTRL_TYPE_BULK;
            break;
        case USB_EP_TYPE_INTR:
            ep_type_val = ENDPTCTRL_TYPE_INTR;
            break;
        default:
            return -PB_ERR_PARAM;
        };

        /* Endpoint 0 is always the control endpoint, it has no configuration */
        if (ep < USB_EP1_OUT || ep >= USB_EP_END)
            return -PB_ERR_PARAM;

        epctrl = CI_ENDPTCTRL(EP_USB_NUMBER(ep));

        /* Keep the other direction, and make it bulk if it's unused: a
         * disabled direction left as control type makes the data PID
         * tracking of the enabled one undefined. */
        ep_reg = mmio_read_32(imx_ci_udc_base + epctrl);

        if (EP_IS_IN(ep)) {
            ep_reg &= ENDPTCTRL_RX_MASK;
            ep_reg |= ENDPTCTRL_TXE | ENDPTCTRL_TXT(ep_type_val) | ENDPTCTRL_TXR;

            if (!(ep_reg & ENDPTCTRL_RXE))
                ep_reg = (ep_reg & ~ENDPTCTRL_RXT_MASK) | ENDPTCTRL_RXT(ENDPTCTRL_TYPE_BULK);
        } else {
            ep_reg &= ENDPTCTRL_TX_MASK;
            ep_reg |= ENDPTCTRL_RXE | ENDPTCTRL_RXT(ep_type_val) | ENDPTCTRL_RXR;

            if (!(ep_reg & ENDPTCTRL_TXE))
                ep_reg = (ep_reg & ~ENDPTCTRL_TXT_MASK) | ENDPTCTRL_TXT(ENDPTCTRL_TYPE_BULK);
        }

        LOG_DBG("EP config: reg=0x%lx, val=0x%x", epctrl, ep_reg);
        mmio_write_32(imx_ci_udc_base + epctrl, ep_reg);
        imx_ci_udc_config_ep(ep, max_pkt_sz, 0);
        endpoints_enabled |= ep_bit(ep);
    }

    configured = true;
    lost = false;
    vbus_seen = (mmio_read_32(imx_ci_udc_base + CI_OTGSC) & OTGSC_BSV) != 0;

    return PB_OK;
}

static int imx_ci_udc_poll_setup_pkt(struct usb_setup_packet *pkt)
{
    int rc;

    /* EP0 Process setup packets */
    if (mmio_read_32(imx_ci_udc_base + CI_ENDPTSETUPSTAT) & ENDPTSETUPSTAT_EP(0)) {
        struct imx_ci_udc_queue_head *qh = &dqhs[USB_EP0_OUT];
        struct pb_timeout to;

        /* The tripwire trips when a SETUP arrives during the copy, which
         * is made again: bounded, for a controller that keeps it tripped */
        rc = pb_timeout_init_us(&to, IMX_CI_UDC_ENDPT_TIMEOUT_us);

        if (rc != PB_OK)
            return rc;

        do {
            if (pb_timeout_has_expired(&to)) {
                LOG_ERR("Setup tripwire: timeout");
                return -PB_ERR_TIMEOUT;
            }

            mmio_clrsetbits_32(imx_ci_udc_base + CI_USBCMD, 0, USBCMD_SUTW);
            qh_take(qh);
            memcpy(pkt, qh->setup, sizeof(struct usb_setup_packet));
            qh_give(qh);
        } while (!(mmio_read_32(imx_ci_udc_base + CI_USBCMD) & USBCMD_SUTW));

        mmio_write_32(imx_ci_udc_base + CI_ENDPTSETUPSTAT, ENDPTSETUPSTAT_EP(0));
        mmio_clrsetbits_32(imx_ci_udc_base + CI_USBCMD, USBCMD_SUTW, 0);

        /*
         * A data or status stage of the control transfer before is flushed
         * before endpoint 0 is primed for this one (RT1060 RM 42.5.6.5.2,
         * step 3, with the flush of 42.5.6.6.5). Its setup status isn't
         * waited for: the write clears it, and a bit set again is the next
         * SETUP, which the next poll reads (42.5.6.4.2.1)
         */
        rc = imx_ci_udc_flush(ENDPT_TX(0) | ENDPT_RX(0));

        /* A control transfer the SETUP ended is given up: endpoint 0's
         * alone, a bulk transfer goes on */
        xfer_abandon(&ep0_xfer, rc);

        if (rc != PB_OK)
            return rc;

        return PB_OK;
    }

    rc = imx_ci_udc_irq_process();

    if (rc != PB_OK)
        return rc;

    return -PB_ERR_AGAIN;
}

static int imx_ci_udc_init_internal(void)
{
    int rc;

    LOG_DBG("Init base: 0x%" PRIxPTR, imx_ci_udc_base);

    mmio_clrsetbits_32(imx_ci_udc_base + CI_USBCMD, 0, USBCMD_RST);

    rc = ci_wait_clear(CI_USBCMD, USBCMD_RST, IMX_CI_UDC_RESET_TIMEOUT_us);

    if (rc != PB_OK)
        return rc;

    /* In reset the controller writes nothing: what an init before this one
     * gave it is taken back, the queue heads are written, and they are the
     * controller's from here on */
    imx_ci_udc_dma_take_back();
    imx_ci_udc_reset_queues();

    for (int i = 0; i < IMX_CI_UDC_NO_OF_EPS * 2; i++)
        dma_in_begin(&dqhs[i], sizeof(dqhs[i]));

    queues_dma = true;

    mmio_write_32(imx_ci_udc_base + CI_DEVICEADDR, 0);

    imx_ci_udc_config_ep(USB_EP0_IN, IMX_CI_UDC_SZ_64B, 0);
    imx_ci_udc_config_ep(USB_EP0_OUT, IMX_CI_UDC_SZ_64B, DQH_CAP_IOS);

    /* Program QH top */
    mmio_write_32(imx_ci_udc_base + CI_ENDPTLISTADDR, (uint32_t)(uintptr_t)dqhs);

    /* Enable USB */
    mmio_write_32(imx_ci_udc_base + CI_USBMODE, USBMODE_SDIS | USBMODE_SLOM | USBMODE_CM_DEVICE);

    /* Interrupt threshold 0 (immediate), the driver polls */
    mmio_write_32(imx_ci_udc_base + CI_USBCMD, USBCMD_ITC(0) | USBCMD_RS);

    return imx_ci_udc_reset();
}

static int imx_ci_udc_set_address(uint16_t addr)
{
    mmio_write_32(imx_ci_udc_base + CI_DEVICEADDR, DEVICEADDR_USBADR(addr) | DEVICEADDR_USBADRA);
    return PB_OK;
}

/*
 * Off the bus (Run/Stop clear), every endpoint flushed and the controller
 * reset, which ends whatever it did: the flush isn't waited for, and the
 * reset is the only wait (a failed stop fails the boot, drivers.md 2.3).
 * Reset, it writes pb's memory no more, which is the driver's again.
 */
static int imx_ci_udc_stop(void)
{
    int rc;

    mmio_clrsetbits_32(imx_ci_udc_base + CI_USBCMD, USBCMD_RS, 0);
    mmio_write_32(imx_ci_udc_base + CI_ENDPTFLUSH, ENDPT_ALL);
    mmio_clrsetbits_32(imx_ci_udc_base + CI_USBCMD, 0, USBCMD_RST);

    rc = ci_wait_clear(CI_USBCMD, USBCMD_RST, IMX_CI_UDC_RESET_TIMEOUT_us);

    imx_ci_udc_dma_take_back();
    imx_ci_udc_reset_queues();

    return rc;
}

/* The status stage of a control transfer, bounded (usb.md 4.4, drivers.md 3) */
static int imx_ci_udc_xfer_zlp(usb_ep_t ep)
{
    struct pb_timeout to;
    int rc;

    rc = imx_ci_udc_xfer_start(ep, 0, 0);

    if (rc != PB_OK)
        return rc;

    pb_timeout_init_us(&to, IMX_CI_UDC_EP0_TIMEOUT_us);

    for (;;) {
        rc = imx_ci_udc_xfer_complete(ep);

        if (rc != -PB_ERR_AGAIN)
            return rc;

        if (pb_timeout_has_expired(&to)) {
            LOG_ERR("EP0 status stage timeout");
            /* Given up: nothing of it completes later (drivers.md 3) */
            imx_ci_udc_flush_ep(ep);
            return -PB_ERR_TIMEOUT;
        }
    }
}

int imx_ci_udc_init(uintptr_t base)
{
    imx_ci_udc_base = base;

    static const struct usbd_hal_ops ops = {
        .init = imx_ci_udc_init_internal,
        .stop = imx_ci_udc_stop,
        .xfer_start = imx_ci_udc_xfer_start,
        .xfer_complete = imx_ci_udc_xfer_complete,
        .xfer_cancel = imx_ci_udc_flush_ep,
        .poll_setup_pkt = imx_ci_udc_poll_setup_pkt,
        .set_configuration = imx_ci_udc_set_configuration,
        .set_address = imx_ci_udc_set_address,
        .ep0_xfer_zlp = imx_ci_udc_xfer_zlp,
    };

    return usbd_init_hal_ops(&ops);
}
