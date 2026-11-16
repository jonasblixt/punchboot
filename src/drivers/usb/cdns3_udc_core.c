/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A minimalistic driver for the cadence USB3 OTG controller.
 *
 * There is not really any good documentation publicly available for this
 * controller and PHY. This drivers is pieced together / reverse engineered
 * by looking at the imx8qxp datasheet, uboot and linux implementations.
 *
 * TODO:
 *  - EP0 has an in-transfer alignment buffer, but out transfers require
 *  the buffer argument to be aligned
 *  - Transfers on other endpoints must supply DMA aligned buffers
 *  - TRB amount is hard coded: a transfer longer than the ring holds is
 *  refused (-PB_ERR_PARAM), and command mode's buffer must fit it.
 *  - EP burst size is hardcoded to 64byte (512b endpoint max pkt length)
 *  - Support for super speed
 *  - Occationally we get TRBERR irq's. It's not clear why, but re-starting
 *  the DMA transfer works and there seems to be no dropped packets.
 *
 */

#include <drivers/usb/cdns3_udc_core.h>
#include <inttypes.h>
#include <pb/delay.h>
#include <pb/dma.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <stdio.h>
#include <string.h>

/* Registers and bits */
#define CDNS_USB_CONF                      (0x20000)
#define CONF_USB3DIS                       BIT(3)
#define CONF_DMULT                         BIT(9)
#define CONF_DEV_EN                        BIT(14)
#define CONF_DEVDS                         BIT(15)
#define CONF_L1DS                          BIT(17)
#define CONF_CLK2OFFDS                     BIT(19)
#define CONF_CLK3OFFDS                     BIT(22)
#define CONF_U1DS                          BIT(25)
#define CONF_U2DS                          BIT(27)

#define CDNS_USB_STS                       (0x20004)
#define STS_USB2CONS                       BIT(20)
#define STS_INRST                          BIT(10)
#define STS_LST(x)                         ((x >> 26) & 0x0f)

#define CDNS_USB_CMD                       (0x20008)
#define USB_CMD_SET_ADDR                   BIT(0)
#define USB_CMD_ADDR(x)                    ((x & 0x7f) << 1)
#define USB_CMD_ADDR_MASK                  (USB_CMD_SET_ADDR | USB_CMD_ADDR(0x7f))

#define CDNS_USB_IEN                       (0x20014)
#define USB_IEN_ALL                        (0xffffffff)
#define CDNS_USB_ISTS                      (0x20018)
#define ISTS_CON2I                         BIT(16) // HS/FS mode connection detected
#define ISTS_DIS2I                         BIT(17) // HS/FS mode disconnection detected
#define ISTS_U2RESI                        BIT(18) // USB reset in HS/FS mode ends
#define ISTS_CFGRESI                       BIT(26) // USB configuration reset detected
/* A bus reset or an unplug at high speed, as Linux 6.1's
 * cdns3_check_usb_interrupt_proceed() (cdns3-gadget.c) takes them; DIS2I is
 * its cdns3-gadget.h's. Not SuperSpeed's (DISI, UWRESI, UHRESI): pb runs at
 * high speed (usb.md 4.1), and a SuperSpeed link's events beside it must
 * not end a session */
#define ISTS_LOST                          (ISTS_DIS2I | ISTS_U2RESI)

#define CDNS_USB_EP_SEL                    (0x2001C)
#define EP_SEL_DIR(x)                      ((x & 1) << 7)
#define EP_SEL(x)                          (x & 0x0f)

#define CDNS_USB_EP_TRADDR                 (0x20020)
#define CDNS_USB_EP_CFG                    (0x20024)
#define EP_CFG_MAX_PKT_SZ(x)               ((x & 0x7ff) << 16)
#define EP_CFG_TYPE(x)                     ((x & 0x03) << 1)
#define EP_CFG_STREAM_EN                   BIT(3)
#define EP_CFG_ENABLE                      BIT(0)

#define CDNS_USB_EP_CMD                    (0x20028)
#define EP_CMD_EPRST                       BIT(0)
#define EP_CMD_SSTALL                      BIT(1)
#define EP_CMD_ERDY                        BIT(3)
#define EP_CMD_REQ_CMPL                    BIT(5)
#define EP_CMD_DRDY                        BIT(6)
#define EP_CMD_DFLUSH                      BIT(7)

#define CDNS_USB_EP_STS_EN                 (0x20034)
#define EP_STS_EN_SETUP                    BIT(0)
#define EP_STS_EN_DESCMISEN                BIT(4)
#define EP_STS_EN_TRBERREN                 BIT(7)

#define CDNS_USB_EP_STS                    (0x2002C)
#define EP_STS_SETUP                       BIT(0)
#define EP_STS_IOC                         BIT(2)
#define EP_STS_ISP                         BIT(3) /* Short packet */
#define EP_STS_DESCMIS                     BIT(4)
#define EP_STS_TRBERR                      BIT(7)
#define EP_STS_DBUSY                       BIT(9)
#define EP_STS_BUFFEMPTY                   BIT(10)

#define CDNS_USB_EP_STS_SID                (0x20030)
#define CDNS_USB_DRBL                      (0x20038)
#define CDNS_USB_EP_IEN                    (0x2003C)
#define EP_IEN_EP_OUT(n)                   (1 << (n))
#define EP_IEN_EP_IN(n)                    (1 << (16 + (n)))
#define CDNS_USB_EP_ISTS                   (0x20040)
#define CDNS_USB_PWR                       (0x20044)
#define CDNS_USB_CONF2                     (0x20048)
#define CDNS_USB_CAP1                      (0x2004C)
#define CDNS_USB_CAP2                      (0x20050)
#define CDNS_USB_CAP3                      (0x20054)
#define CDNS_USB_CAP5                      (0x2005C)
#define CDNS_USB_CAP6                      (0x20060) /* Device controller version */
#define CDNS_USB_CPKT1                     (0x20064)
#define CDNS_USB_CPKT2                     (0x20068)
#define CDNS_USB_CPKT3                     (0x2006C)
#define CDNS_USB_DBG_LINK1                 (0x20104)
#define DBG_LINK1_LFPS_MIN_GEN_U1_EXIT_SET BIT(25)
#define DBG_LINK1_LFPS_MIN_GEN_U1_EXIT(x)  ((x & 0xff) << 8)

/* Transfer descriptor & bits*/
struct cdns_trb {
    uint32_t addr;
    uint32_t length;
    uint32_t flags;
} __packed;

#define TRB_MAX_LENGTH      (127 * 1024) // (130048)
#define TRB_LENGTH_MASK     0x1ffff
/* Bits 31:24 of a TRB's length word, above the length's 16:0 (Linux 6.18,
 * cdns3-gadget.h TRB_BURST_LEN). It was written to 23:16 (F-461) */
#define TRB_BURST_LENGTH(x) (((uint32_t)(x) & 0xFF) << 24)
/* The first version without the DMA bug below (Linux's DEV_VER_V2) */
#define CDNS_DEV_VER_V2     0x0002450C

#define TRB_FLAG_CYCLE      BIT(0)
#define TRB_FLAG_TOGGLE     BIT(1)
#define TRB_FLAG_ISP        BIT(2) /* Interrupt on a short packet */
#define TRB_FLAG_CHAIN      BIT(4)
#define TRB_FLAG_IOC        BIT(5)
#define TRB_TYPE(x)         ((x) << 10)
#define TRB_TYPE_NORMAL     (1)
#define TRB_TYPE_LINK       (6)

static uintptr_t base;

/*
 * What the controller reads and writes of the driver's own, each on cache
 * lines of its own (drivers.md 4.2, 3 and 5). They were aligned and not
 * padded: the setup packet is 8 bytes, and whatever the linker put behind it
 * shared the line that every SETUP invalidates (F-205). Not zeroed at boot:
 * a ring's terminating TRB is written with the ring.
 */
PB_DMA_BUFFER(struct usb_setup_packet, setup_pkt);
PB_DMA_ARRAY(struct cdns_trb, ep0_in_trb, 2);
PB_DMA_ARRAY(struct cdns_trb, ep0_out_trb, 2);
/* Endpoint 0's IN data stage goes through it: a longer reply is refused
 * (F-205). The longest the stack sends is a string descriptor, 255 bytes */
PB_DMA_ARRAY(uint8_t, align_buffer, 512);
#define CDNS_TRB_COUNT 128
/*
 * A ring for each direction of the bulk endpoints (TODO: hardcoded amount
 * of TRB's). An endpoint stays after its transfer's last TRB, and with
 * CONF_DMULT it takes a TRB written there later as its own: with one ring
 * for both, the OUT data's second TRB went out on IN as the result (F-434).
 */
PB_DMA_ARRAY(struct cdns_trb, trb_in, CDNS_TRB_COUNT);
PB_DMA_ARRAY(struct cdns_trb, trb_out, CDNS_TRB_COUNT);
/* The longest bulk transfer: the ring's TRBs but the last, which is the
 * stop gap behind the transfer's. A transfer of CDNS_TRB_COUNT TRBs wrote
 * its stop gap past the ring (F-205). 15.75 MiB, above command mode's
 * buffer and the boot's piece (usb.md 2.3, 4.3; 4 MiB each on the MEK) */
#define CDNS_XFER_MAX ((size_t)TRB_MAX_LENGTH * (CDNS_TRB_COUNT - 1))

#ifdef CONFIG_CM_BUF_SIZE_KiB
_Static_assert((size_t)CONFIG_CM_BUF_SIZE_KiB * 1024 <= CDNS_XFER_MAX,
               "CONFIG_CM_BUF_SIZE_KiB: above the cdns3's longest transfer");
#endif
#ifdef CONFIG_BOOT_LOAD_CHUNK_kB
_Static_assert((size_t)CONFIG_BOOT_LOAD_CHUNK_kB * 1024 <= CDNS_XFER_MAX,
               "CONFIG_BOOT_LOAD_CHUNK_kB: above the cdns3's longest transfer");
#endif

/* EP0's max packet size at high speed, the descriptor's bMaxPacketSize0. 512
 * is SuperSpeed's, which pb doesn't support: at high speed a longer reply,
 * the serial number string, went out as one packet and the host dropped it
 * (F-159). */
#define EP0_MAX_PKT_SZ 64

static void *current_xfer_buf;
static size_t current_xfer_length;
static usb_ep_t current_xfer_ep;
/* The bulk transfer's TRBs, and an OUT transfer's buffer, are the
 * controller's: begun, and not ended yet */
static bool current_xfer_dma;
static struct cdns_trb *current_xfer_trb;
static size_t current_xfer_trb_length;
/* The same for endpoint 0's two rings and what its OUT transfer writes,
 * which is the setup packet */
static bool ep0_in_dma;
static bool ep0_out_dma;
static void *ep0_out_buf;
static size_t ep0_out_length;
/* A flush that didn't end: the bulk transfer may still be the
 * controller's, and the next one is refused until init (drivers.md 3) */
static bool xfer_flush_failed;
/*
 * A bulk TRB's DMA burst, as Linux's cdns3_ep_config(): 64 for an endpoint
 * of 512 byte packets, and 16 on a controller before DEV_VER_V2, such as
 * the i.MX8QXP's (0x24502), whose DMA corrupts data when a longer burst to
 * an address not on 128 bytes crosses a 4 KiB border
 */
static uint8_t trb_burst = 16;
/* The host configured the device, and reset the bus or went away after
 * that: every bulk transfer fails until the device is configured or
 * started again (usb.md 2.4, 4.3) */
static bool configured;
static bool bus_lost;
/* From init to stop: the controller is set up, and the operations may use
 * it. Before init and after stop they are -PB_ERR_STATE (drivers.md 3) */
static bool running;
/* The endpoints pb enabled, a bit for each usb_ep_t: what stop resets */
static uint32_t eps_enabled;

/*
 * The bounds of the driver's waits (drivers.md 3). There is no manual for
 * the controller here: each is the reference that comes closest.
 *
 * A flush: Linux 6.1's cdns3-gadget.c waits up to 1 ms for EP_CMD_DFLUSH to
 * clear (readl_poll_timeout_atomic(..., 1, 1000) in cdns3_ep_stall_flush()
 * and cdns3_gadget_ep_dequeue()).
 */
#define CDNS_FLUSH_TIMEOUT_us        1000
/* The status stage of a request without a data stage: USB 2.0 9.2.6.4, the
 * request and its status stage are done within 50 ms of the SETUP */
#define CDNS_STATUS_STAGE_TIMEOUT_us 50000
/* The controller's internal reset (USB_STS's IN_RST, "controller is fully
 * operational" in Linux 6.1's cdns3-gadget.h, which doesn't wait for it): as
 * the waits on the same block's status bits in Linux 6.1's cdns3-imx.c,
 * readl_poll_timeout(..., 10, 100000), 100 ms */
#define CDNS_RESET_TIMEOUT_us        100000
/* An endpoint's reset in stop, as Linux 6.1's cdns3_gadget_ep_disable():
 * up to 10 us for the endpoint's DMA to go idle (EP_STS_DBUSY, "10us is
 * enough time for controller to stop transfer", and no error when it isn't),
 * then up to 1 ms for EP_CMD_EPRST to clear (cdns3_gadget_udc_stop() gives
 * it 100 us, the longer of the two is taken) */
#define CDNS_DBUSY_TIMEOUT_us        10
/* An IN endpoint's packets still in the on-chip buffer at stop: the host
 * reads them within a few microframes when it waits for them. A host that
 * doesn't is given up after this */
#define CDNS_BUFFEMPTY_TIMEOUT_us    50000
#define CDNS_EPRST_TIMEOUT_us        1000

/* Polls 'reg' until its bits in 'mask' are 'value', at most 'timeout_us' */
static int wait_for(uintptr_t reg, uint32_t mask, uint32_t value, uint32_t timeout_us)
{
    struct pb_timeout to;

    pb_timeout_init_us(&to, timeout_us);

    while ((mmio_read_32(base + reg) & mask) != value) {
        if (pb_timeout_has_expired(&to))
            return -PB_ERR_TIMEOUT;
    }

    return PB_OK;
}

static int select_ep(usb_ep_t ep)
{
    uint8_t dir = ep % 2; // 0 = Out, 1 = In
    uint8_t ep_val = ep / 2;

    if (ep >= USB_EP_END)
        return -PB_ERR_PARAM;

    mmio_write_32(base + CDNS_USB_EP_SEL, EP_SEL_DIR(dir) | EP_SEL(ep_val));
    return PB_OK;
}

static bool is_out_with_data(usb_ep_t ep, const void *buf, size_t length)
{
    return !(ep % 2) && buf != NULL && length > 0;
}

/* Endpoint 0's transfer in one direction is over, or is replaced: its ring
 * and what it wrote are the driver's again, and read from RAM */
static void ep0_dma_end(bool in_xfer)
{
    if (in_xfer && ep0_in_dma) {
        dma_in_end(ep0_in_trb, sizeof(ep0_in_trb));
        ep0_in_dma = false;
    }

    if (!in_xfer && ep0_out_dma) {
        dma_in_end(ep0_out_trb, sizeof(ep0_out_trb));

        if (ep0_out_buf != NULL)
            dma_in_end(ep0_out_buf, ep0_out_length);

        ep0_out_dma = false;
    }
}

/* The same for the bulk transfer */
static void xfer_dma_end(void)
{
    if (!current_xfer_dma)
        return;

    dma_in_end(current_xfer_trb, current_xfer_trb_length);

    if (is_out_with_data(current_xfer_ep, current_xfer_buf, current_xfer_length))
        dma_in_end(current_xfer_buf, current_xfer_length);

    current_xfer_dma = false;
}

static int ep0_xfer_start(void *buf, size_t length, bool in_xfer)
{
    void *buf_p = buf;
    struct cdns_trb *trb_p = in_xfer ? ep0_in_trb : ep0_out_trb;

    if (length > TRB_MAX_LENGTH)
        return -PB_ERR_PARAM;

    /* IN goes through the bounce buffer, which it must fit (F-205) */
    if (in_xfer && length > sizeof(align_buffer))
        return -PB_ERR_PARAM;

    /* The transfer before this one in the same direction */
    ep0_dma_end(in_xfer);

    if (in_xfer) {
        // TODO: Temparary alignment fix
        memcpy(align_buffer, buf, length);
        buf_p = align_buffer;
        dma_out(buf_p, length);
    }

    // LOG_DBG("%s: %p, len=%zu", in_xfer?"IN":"OUT", buf_p, length);

    trb_p->addr = (uint32_t)(uintptr_t)buf_p;
    trb_p->length = (uint32_t)length;
    trb_p->flags = TRB_FLAG_CYCLE | TRB_FLAG_IOC | TRB_TYPE(TRB_TYPE_NORMAL);
    /* The ring ends behind it: the controller stops at a TRB whose cycle
     * bit is clear. It was zero from .bss */
    trb_p[1].addr = 0;
    trb_p[1].length = 0;
    trb_p[1].flags = 0;

    /* The controller reads the ring and may write it, and it writes an OUT
     * transfer's buffer: the setup packet, which owns its lines */
    if (in_xfer) {
        dma_in_begin(ep0_in_trb, sizeof(ep0_in_trb));
        ep0_in_dma = true;
    } else {
        ep0_out_buf = (length > 0) ? buf : NULL;
        ep0_out_length = length;

        if (ep0_out_buf != NULL)
            dma_in_begin(ep0_out_buf, ep0_out_length);

        dma_in_begin(ep0_out_trb, sizeof(ep0_out_trb));
        ep0_out_dma = true;
    }

    select_ep(in_xfer ? USB_EP0_IN : USB_EP0_OUT);
    mmio_write_32(base + CDNS_USB_EP_STS, EP_STS_IOC | EP_STS_TRBERR);
    mmio_write_32(base + CDNS_USB_EP_TRADDR, (uint32_t)(uintptr_t)trb_p);
    mmio_write_32(base + CDNS_USB_EP_CMD, EP_CMD_DRDY);

    /* The data stage: out of flow control, as Linux's cdns3_ep0_run_transfer() */
    if (in_xfer)
        mmio_write_32(base + CDNS_USB_EP_CMD, EP_CMD_ERDY);

    return PB_OK;
}

int cdns3_udc_core_xfer_start(usb_ep_t ep, void *buf, size_t length)
{
    size_t chunk;
    size_t bytes_to_xfer = length;
    uintptr_t buf_p = (uintptr_t)buf;
    uint8_t ep_val = ep / 2;
    size_t desc_count = 0;
    bool in_xfer = !!(ep % 2);
    struct cdns_trb *trb = in_xfer ? trb_in : trb_out;
    struct cdns_trb *trb_p = &trb[0];

    if (ep >= USB_EP_END)
        return -PB_ERR_PARAM;

    /* After stop nothing is armed again but by init (drivers.md 2.3) */
    if (!running)
        return -PB_ERR_STATE;

    LOG_DBG("%s: buf=%p, len=%zu", ep_to_str(ep), buf, length);

    if (ep_val == 0) {
        return ep0_xfer_start(buf, length, in_xfer);
    }

    if (length > CDNS_XFER_MAX)
        return -PB_ERR_PARAM;

    /* A read of 0 bytes isn't a transfer (usb.md 2.3). One wasn't refused:
     * the endpoint was armed with no TRB, and the transfer never ended */
    if (length == 0 && !in_xfer)
        return -PB_ERR_PARAM;

    if (xfer_flush_failed)
        return -PB_ERR_TIMEOUT;

    /* A bus reset or an unplug while no transfer ran is the error of the
     * next one. Endpoint 0 goes on: the host enumerates */
    if (bus_lost)
        return -PB_ERR_IO;

    /* One bulk transfer at a time: one that wasn't completed or cancelled
     * ends here, and the ring is the driver's to write */
    xfer_dma_end();

    // TODO: xfer alignment, what is the minimum alignment?
    current_xfer_ep = ep;
    current_xfer_buf = buf;
    current_xfer_length = length;

    if (in_xfer)
        dma_out(buf, length);

    /* At least one TRB: a write of 0 bytes is one of length 0, which sends
     * one empty packet (usb.md 2.3, 4.3). It had none, and the endpoint was
     * armed on the stop gap: nothing was sent and the transfer never ended */
    do {
        chunk = bytes_to_xfer > TRB_MAX_LENGTH ? TRB_MAX_LENGTH : bytes_to_xfer;

        trb_p->addr = (uint32_t)buf_p;
        trb_p->length = (uint32_t)chunk | TRB_BURST_LENGTH(trb_burst);
        trb_p->flags = TRB_FLAG_CYCLE | TRB_TYPE(TRB_TYPE_NORMAL);

        /* A short packet ends the transfer with EP_STS_ISP, as Linux's
         * cdns3 driver asks for. Without it the controller went on to the
         * next TRB, the stop gap at the end, and the transfer never ended
         * (F-287) */
        if (!in_xfer)
            trb_p->flags |= TRB_FLAG_ISP;

        bytes_to_xfer -= chunk;
        buf_p += chunk;

        /* Last TRB should generate completion IRQ */
        if (bytes_to_xfer == 0) {
            trb_p->flags |= TRB_FLAG_IOC;
        }

        LOG_DBG(
            "desc[%lu]: addr=%x, len=%zu, flags=%x", desc_count, trb_p->addr, chunk, trb_p->flags);

        trb_p++;
        trb_p->flags = 0;
        desc_count++;
    } while (bytes_to_xfer > 0);

    /* Stop gap TRB */
    trb_p->addr = 0;
    trb_p->length = 0;
    trb_p->flags = 0;

    /* With the stop gap TRB, which the controller reads too. The ring and
     * an OUT transfer's buffer, which owns its lines (usb.md 4.3), are the
     * controller's until the transfer is done or given up */
    current_xfer_trb = trb;
    current_xfer_trb_length = sizeof(trb[0]) * (desc_count + 1);
    dma_in_begin(trb, current_xfer_trb_length);

    if (is_out_with_data(ep, buf, length))
        dma_in_begin(buf, length);

    current_xfer_dma = true;

    select_ep(ep);
    mmio_write_32(base + CDNS_USB_EP_STS, EP_STS_IOC | EP_STS_TRBERR);
    mmio_write_32(base + CDNS_USB_EP_TRADDR, (uint32_t)(uintptr_t)trb);
    mmio_write_32(base + CDNS_USB_EP_CMD, EP_CMD_DRDY);

    return PB_OK;
}

int cdns3_udc_core_xfer_complete(usb_ep_t ep)
{
    bool in_xfer = !!(ep % 2);
    uint32_t ep_sts;

    if (!running)
        return -PB_ERR_STATE;

    /* A bus reset or an unplug ends a bulk transfer: lost. It is seen in
     * poll_setup_pkt(), which the stack calls before each poll of it */
    if (bus_lost && ep / 2 != 0)
        return -PB_ERR_IO;

    select_ep(ep);
    ep_sts = mmio_read_32(base + CDNS_USB_EP_STS);

    if (ep_sts & EP_STS_TRBERR) {
        mmio_write_32(base + CDNS_USB_EP_STS, EP_STS_TRBERR);

        LOG_DBG("%s: TRBERR (0x%x)", ep_to_str(ep), ep_sts);
        struct cdns_trb *trb_p =
            (struct cdns_trb *)(uintptr_t)mmio_read_32(base + CDNS_USB_EP_TRADDR);

        /* For the log, the bulk ring as the controller has it */
        (void)trb_p;

        if (current_xfer_dma)
            dma_in_end(current_xfer_trb, current_xfer_trb_length);

        LOG_DBG("  addr=%x, len=%u, flags=%x", trb_p->addr, trb_p->length & 0xffff, trb_p->flags);
        LOG_DBG("  trb_base=%p, trb_p=0x%p", (void *)current_xfer_trb, trb_p);

        if (current_xfer_dma)
            dma_in_begin(current_xfer_trb, current_xfer_trb_length);

        // Re-start DMA
        mmio_write_32(base + CDNS_USB_EP_CMD, EP_CMD_DRDY);
    }

    /* A last packet may be short: done on a short packet too, as Linux.
     * An OUT transfer of whole TRBs then has bytes missing, below */
    if ((ep_sts & EP_STS_IOC) || (!in_xfer && (ep_sts & EP_STS_ISP)) ||
        (ep / 2 == 0 && (ep_sts & EP_STS_ISP))) {
        LOG_DBG("%s: Xfer done %x", ep_to_str(ep), ep_sts);

        mmio_write_32(base + CDNS_USB_EP_STS, EP_STS_IOC | EP_STS_ISP);

        if (ep / 2 == 0) {
            /* The setup packet's transfer is polled in poll_setup_pkt() */
            if (in_xfer)
                ep0_dma_end(true);
        } else if (ep == current_xfer_ep) {
            size_t expected = current_xfer_length;
            size_t received = expected;

            /* The controller writes back what each TRB moved, as Linux's
             * cdns3 driver reads it (the request's actual length) */
            if (!in_xfer && current_xfer_dma) {
                size_t trbs = current_xfer_trb_length / sizeof(current_xfer_trb[0]) - 1;

                dma_in_end(current_xfer_trb, current_xfer_trb_length);
                received = 0;
                for (size_t n = 0; n < trbs; n++)
                    received += current_xfer_trb[n].length & TRB_LENGTH_MASK;
                dma_in_begin(current_xfer_trb, current_xfer_trb_length);
            }

            /* A short packet before 'length': the two sides no longer agree
             * (usb.md 2.3, F-287). The TRBs after it are still armed: the
             * endpoint is flushed before its buffer is the caller's again */
            if (received != expected) {
                LOG_ERR("%s: %zu of %zu bytes", ep_to_str(ep), received, expected);
                mmio_write_32(base + CDNS_USB_EP_CMD, EP_CMD_DFLUSH);

                if (wait_for(CDNS_USB_EP_CMD, EP_CMD_DFLUSH, 0, CDNS_FLUSH_TIMEOUT_us) != PB_OK) {
                    /* As xfer_cancel: the transfer stays the controller's */
                    LOG_ERR("%s: flush timeout", ep_to_str(ep));
                    xfer_flush_failed = true;
                    return -PB_ERR_IO;
                }
            }

            /* The ring, and what an OUT transfer wrote */
            xfer_dma_end();

            current_xfer_ep = -1;
            current_xfer_buf = NULL;
            current_xfer_length = 0;

            if (received != expected)
                return -PB_ERR_IO;
        }

        return PB_OK;
    } else {
        return -PB_ERR_AGAIN;
    }
}

void cdns3_udc_core_xfer_cancel(usb_ep_t ep)
{
    /* Stopped: the endpoint was reset, and its transfer is the caller's */
    if (!running)
        return;

    select_ep(ep);

    mmio_write_32(base + CDNS_USB_EP_CMD, EP_CMD_DFLUSH);

    if (wait_for(CDNS_USB_EP_CMD, EP_CMD_DFLUSH, 0, CDNS_FLUSH_TIMEOUT_us) != PB_OK) {
        /* The endpoint may still run: its transfer stays the controller's,
         * and the next is refused until init resets the controller */
        LOG_ERR("%s: flush timeout", ep_to_str(ep));
        xfer_flush_failed = true;
        return;
    }

    /* The endpoint is idle: its transfer's buffer is the caller's again
     * (usb.md 4.3) */
    if (ep == current_xfer_ep)
        xfer_dma_end();
}

static void cdns_process_irq(void)
{
    uint32_t ists = mmio_read_32(base + CDNS_USB_ISTS);

    /* Before the device is configured a reset is the host's enumeration and
     * no error (usb.md 2.2) */
    if ((ists & ISTS_LOST) && configured && !bus_lost) {
        LOG_ERR("Bus reset or unplug (0x%x)", ists);
        bus_lost = true;
    }

    if (ists & ISTS_CFGRESI) {
        ep0_xfer_start(&setup_pkt, sizeof(setup_pkt), false);
    }

    mmio_write_32(base + CDNS_USB_ISTS, ists);
}

int cdns3_udc_core_poll_setup_pkt(struct usb_setup_packet *pkt)
{
    bool got_setup_pkt = false;
    uint32_t ep_ists;
    uint32_t ep_sts;

    /* It arms endpoint 0 again, which stop left unarmed */
    if (!running)
        return -PB_ERR_STATE;

    ep_ists = mmio_read_32(base + CDNS_USB_EP_ISTS);
    mmio_write_32(base + CDNS_USB_EP_ISTS, ep_ists);
    if (ep_ists & 1) { /* EP0 Out */
        select_ep(USB_EP0_OUT);
        ep_sts = mmio_read_32(base + CDNS_USB_EP_STS);

        if (ep_sts & EP_STS_SETUP) {
            /* Read from RAM, and the controller's again: the next SETUP
             * comes when the host sends it */
            if (ep0_out_dma)
                dma_in_end(&setup_pkt, sizeof(setup_pkt));

            memcpy(pkt, &setup_pkt, sizeof(setup_pkt));

            if (ep0_out_dma)
                dma_in_begin(&setup_pkt, sizeof(setup_pkt));

            got_setup_pkt = true;
        }

        if (ep_sts & EP_STS_DESCMIS) {
            LOG_ERR("Desc missing!");
        }

        if (ep_sts & EP_STS_TRBERR) {
            LOG_ERR("TRB Error");
        }

        if (ep_sts & EP_STS_IOC) {
            /* Queue up a new out setup xfer */
            ep0_xfer_start(&setup_pkt, sizeof(setup_pkt), false);
            /* Ack the setup stage */
            mmio_write_32(base + CDNS_USB_EP_CMD, EP_CMD_REQ_CMPL);
        }

        mmio_write_32(base + CDNS_USB_EP_STS, ep_sts);
    }

    cdns_process_irq();

    if (got_setup_pkt)
        return PB_OK;
    else
        return -PB_ERR_AGAIN;
}

int cdns3_udc_set_configuration(const struct usb_endpoint_descriptor *eps, size_t no_of_eps)
{
    usb_ep_t ep;
    uint16_t max_pkt_sz;
    enum usb_ep_type ep_type;
    uint32_t ep_num;

    // TODO's
    //  o Hard coded bulk EP type
    //  o Enable both IN/OUT INT

    if (!running)
        return -PB_ERR_STATE;

    for (size_t n = 0; n < no_of_eps; n++) {
        ep_num = (eps[n].bEndpointAddress & USB_EP_ADDR_NUM_MASK);

        ep = (eps[n].bEndpointAddress & USB_EP_ADDR_NUM_MASK) * 2;
        if (eps[n].bEndpointAddress & USB_EP_ADDR_IN)
            ep++;

        ep_type = eps[n].bmAttributes;
        max_pkt_sz = eps[n].wMaxPacketSize;

        LOG_INFO("ep=%u, type=%i, pkt_sz=%u", ep, ep_type, max_pkt_sz);

        select_ep(ep);
        mmio_write_32(base + CDNS_USB_EP_CFG,
                      EP_CFG_ENABLE | EP_CFG_TYPE(2) | EP_CFG_MAX_PKT_SZ(max_pkt_sz));

        mmio_write_32(base + CDNS_USB_EP_STS_EN, EP_STS_EN_DESCMISEN | EP_STS_EN_TRBERREN);

        /* Enable interrupt for EP0 in/out */
        mmio_write_32(base + CDNS_USB_EP_IEN, EP_IEN_EP_IN(ep_num) | EP_IEN_EP_OUT(ep_num));

        eps_enabled |= BIT(ep);
    }

    configured = true;
    bus_lost = false;

    return PB_OK;
}

int cdns3_udc_core_set_address(uint16_t addr)
{
    if (!running)
        return -PB_ERR_STATE;

    mmio_clrsetbits_32(
        base + CDNS_USB_CMD, USB_CMD_ADDR_MASK, USB_CMD_SET_ADDR | USB_CMD_ADDR(addr));

    return PB_OK;
}

int cdns3_udc_core_xfer_zlp(usb_ep_t ep)
{
    if (!running)
        return -PB_ERR_STATE;

    select_ep(ep);
    /**
     * We set ERDY here because there is no data stage in the set address req.
     * This means that we exit the setup stage and skip the data stage,
     * since there is no data to write to the host.
     *
     * Setting REQ_CMPL means that we send the ACK when we enter the status stage.
     */

    if (ep == USB_EP0_IN) {
        mmio_write_32(base + CDNS_USB_EP_CMD, EP_CMD_ERDY | EP_CMD_REQ_CMPL);

        if (wait_for(CDNS_USB_EP_CMD, EP_CMD_REQ_CMPL, 0, CDNS_STATUS_STAGE_TIMEOUT_us) != PB_OK) {
            LOG_ERR("Status stage timeout");
            return -PB_ERR_TIMEOUT;
        }
    } else if (ep == USB_EP0_OUT) {
        mmio_write_32(base + CDNS_USB_EP_CMD, EP_CMD_ERDY);
    }

    return PB_OK;
}

/*
 * A protocol stall of endpoint 0 (usb.md 4.4, F-138): SSTALL on endpoint 0,
 * then the setup stage ended with ERDY and REQ_CMPL, so the host's data or
 * status stage gets the STALL. As Linux 6.1's cdns3-ep0.c does for a request
 * it refuses (no manual). The controller ends the stall at the next SETUP,
 * whose transfer is armed already (poll_setup_pkt)
 */
int cdns3_udc_core_ep0_stall(void)
{
    if (!running)
        return -PB_ERR_STATE;

    select_ep(USB_EP0_OUT);
    mmio_write_32(base + CDNS_USB_EP_CMD, EP_CMD_SSTALL);
    mmio_write_32(base + CDNS_USB_EP_CMD, EP_CMD_ERDY | EP_CMD_REQ_CMPL);
    return PB_OK;
}

int cdns3_udc_core_init(void)
{
    uint32_t dev_ctrl_version = 0;

    LOG_INFO("Probing @ 0x%" PRIxPTR "", base);

    dev_ctrl_version = mmio_read_32(base + CDNS_USB_CAP6);

    LOG_INFO("Device controller version: 0x%x", dev_ctrl_version);
    trb_burst = (dev_ctrl_version < CDNS_DEV_VER_V2) ? 16 : 64;

    /* A second init: the glue reset the controller, and what it had of the
     * driver's memory is the driver's again */
    xfer_dma_end();
    ep0_dma_end(true);
    ep0_dma_end(false);
    xfer_flush_failed = false;
    configured = false;
    bus_lost = false;
    running = false;
    eps_enabled = 0;

    /* Wait for controller to come out of reset */
    if (wait_for(CDNS_USB_STS, STS_INRST, STS_INRST, CDNS_RESET_TIMEOUT_us) != PB_OK) {
        LOG_ERR("Controller reset timeout");
        return -PB_ERR_TIMEOUT;
    }

    running = true;

    /* Configure and enable EP0 */
    // TODO: Hardcoded values for SS

    /* Enable all irq's */
    mmio_write_32(base + CDNS_USB_IEN, USB_IEN_ALL);

    select_ep(USB_EP0_OUT);
    // STREAM_EN should probably be set before we can get SS working
    mmio_write_32(base + CDNS_USB_EP_CFG, EP_CFG_ENABLE | EP_CFG_MAX_PKT_SZ(EP0_MAX_PKT_SZ));

    mmio_write_32(base + CDNS_USB_EP_STS_EN,
                  EP_STS_EN_SETUP | EP_STS_EN_DESCMISEN | EP_STS_EN_TRBERREN);

    select_ep(USB_EP0_IN);
    mmio_write_32(base + CDNS_USB_EP_CFG, EP_CFG_ENABLE | EP_CFG_MAX_PKT_SZ(EP0_MAX_PKT_SZ));

    mmio_write_32(base + CDNS_USB_EP_STS_EN, EP_STS_EN_SETUP | EP_STS_EN_TRBERREN);

    /* Enable interrupt for EP0 in/out */
    mmio_write_32(base + CDNS_USB_EP_IEN, EP_IEN_EP_IN(0) | EP_IEN_EP_OUT(0));
    eps_enabled = BIT(USB_EP0_OUT) | BIT(USB_EP0_IN);

    mmio_write_32(base + CDNS_USB_CONF, CONF_CLK2OFFDS | CONF_CLK3OFFDS | CONF_L1DS);

    mmio_write_32(base + CDNS_USB_CONF, CONF_U1DS | CONF_U2DS);

    /* High speed only (pb's descriptors are USB 2.0): the SuperSpeed side is
     * disabled before the device is enabled, as Linux 6.18's
     * cdns3_gadget_udc_start() does for a gadget whose max_speed is high
     * speed. With it enabled the controller tries for a SuperSpeed link, and
     * on the MEK the host saw it at high speed after 1 to 150 s, or not
     * before pb's 10 s ready timeout (F-172) */
    mmio_write_32(base + CDNS_USB_CONF, CONF_USB3DIS);

    /* Configure DMA for multiple transfers. When the DMA has completed the
     * current trb or trb chain it will look at the next trb index for a valid
     * descriptor and continue transfering data */
    mmio_write_32(base + CDNS_USB_CONF, CONF_DMULT);
    mmio_write_32(base + CDNS_USB_CONF, CONF_DEV_EN);

    mmio_write_32(base + CDNS_USB_DBG_LINK1,
                  DBG_LINK1_LFPS_MIN_GEN_U1_EXIT_SET | DBG_LINK1_LFPS_MIN_GEN_U1_EXIT(0x3c));

    // Maybe we don't need to do this here
    ep0_xfer_start(&setup_pkt, sizeof(setup_pkt), false);

    LOG_INFO("usb_sts = 0x%x", mmio_read_32(base + CDNS_USB_STS));

    return PB_OK;
}

/* An endpoint is disabled and reset: it is armed at no TRB, and moves no
 * data. As Linux 6.1's cdns3_gadget_ep_disable() */
static int ep_reset(usb_ep_t ep)
{
    select_ep(ep);
    mmio_clrsetbits_32(base + CDNS_USB_EP_CFG, EP_CFG_ENABLE, 0);

    /* Linux goes on to the reset when the endpoint stays busy: so does pb */
    (void)wait_for(CDNS_USB_EP_STS, EP_STS_DBUSY, 0, CDNS_DBUSY_TIMEOUT_us);

    mmio_write_32(base + CDNS_USB_EP_CMD, EP_CMD_EPRST);

    if (wait_for(CDNS_USB_EP_CMD, EP_CMD_EPRST, 0, CDNS_EPRST_TIMEOUT_us) != PB_OK) {
        LOG_ERR("%s: reset timeout", ep_to_str(ep));
        return -PB_ERR_TIMEOUT;
    }

    return PB_OK;
}

/*
 * drivers.md 2.3, usb.md 4.2: the device leaves the bus, so the host sees it
 * go; every endpoint pb enabled is disabled and reset, so none is armed at
 * pb's memory; the controller's interrupts are off. It was empty, and the
 * device stayed enumerated with endpoint 0 armed at setup_pkt: a SETUP
 * after the handover was written into the next stage's memory (F-204).
 *
 * The order is Linux 6.1's when a gadget is unbound
 * (usb_gadget_remove_driver()) on cdns3-gadget.c: cdns3_gadget_pullup(0),
 * which clears the pending events and writes USB_CONF_DEVDS; each endpoint
 * as cdns3_gadget_ep_disable(); then cdns3_gadget_udc_stop(), which resets
 * each endpoint again and writes USB_IEN 0. Endpoint 0 is reset too, which
 * Linux leaves: it is the one armed between two requests. EP_IEN, which pb
 * sets and Linux leaves, is cleared as well. USB_CONF_SWRST isn't used: no
 * driver here does, and what it resets isn't known. The i.MX glue holds the
 * block in reset after it (imx_cdns3_udc.c).
 */
int cdns3_udc_core_stop(void)
{
    int rc = PB_OK;
    int empty;

    /* Before init, or a second time */
    if (!running)
        return PB_OK;

    LOG_INFO("Stop");

    /*
     * An IN transfer completes when its DMA is done (IOC, QXP RM 14.12.6.178,
     * EP_STS): its packets may still be in the on-chip buffer, the host
     * not having read them. The result of the command before a reset or a
     * jump is one, and leaving the bus at once lost it. Each enabled IN
     * endpoint's buffer is waited for (BUFFEMPTY) before the device
     * leaves; one the host doesn't read is given up, and is no error.
     */
    for (usb_ep_t ep = USB_EP0_IN; ep < USB_EP_END; ep += 2) {
        if (!(eps_enabled & BIT(ep)))
            continue;

        select_ep(ep);
        empty = wait_for(
            CDNS_USB_EP_STS, EP_STS_BUFFEMPTY, EP_STS_BUFFEMPTY, CDNS_BUFFEMPTY_TIMEOUT_us);

        if (empty != PB_OK)
            LOG_INFO("%s: the host didn't read the last packets", ep_to_str(ep));
    }

    mmio_write_32(base + CDNS_USB_EP_ISTS, 0xffffffff);
    mmio_write_32(base + CDNS_USB_ISTS, 0xffffffff);
    mmio_write_32(base + CDNS_USB_CONF, CONF_DEVDS);

    for (usb_ep_t ep = USB_EP0_OUT; ep < USB_EP_END; ep++) {
        if (!(eps_enabled & BIT(ep)))
            continue;

        if (ep_reset(ep) == PB_OK)
            eps_enabled &= ~BIT(ep);
        else
            rc = -PB_ERR_TIMEOUT;
    }

    mmio_write_32(base + CDNS_USB_EP_IEN, 0);
    mmio_write_32(base + CDNS_USB_IEN, 0);

    configured = false;
    bus_lost = false;

    if (rc != PB_OK) {
        /* An endpoint may still move data: what it has stays the
         * controller's, every bulk transfer is refused, and a second stop
         * tries the endpoints that failed again */
        xfer_flush_failed = true;
        return rc;
    }

    /* Nothing is armed: the driver's memory is its own again */
    xfer_dma_end();
    ep0_dma_end(true);
    ep0_dma_end(false);
    running = false;

    return PB_OK;
}

void cdns3_udc_core_set_base(uintptr_t base_)
{
    base = base_;
}
