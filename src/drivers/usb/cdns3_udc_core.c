/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
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
 *  - TRB amount is hard coded. This means that it's possbile to configure
 *  a too large xfer buffer for command mode, so that there is not enough
 *  trb's.
 *  - EP burst size is hardcoded to 64byte (512b endpoint max pkt length)
 *  - Support for super speed
 *  - Occationally we get TRBERR irq's. It's not clear why, but re-starting
 *  the DMA transfer works and there seems to be no dropped packets.
 *  - A few places that wait for a status bit change should have timeout's
 *
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <arch/arch.h>
#include <pb/pb.h>
#include <pb/mmio.h>
#include <pb/delay.h>
#include <drivers/usb/cdns3_udc_core.h>

/* Registers and bits */
#define CDNS_USB_CONF       (0x20000)
#define CONF_DMULT          BIT(9)
#define CONF_DEV_EN         BIT(14)
#define CONF_L1DS           BIT(17)
#define CONF_CLK2OFFDS      BIT(19)
#define CONF_CLK3OFFDS      BIT(22)
#define CONF_U1DS           BIT(25)
#define CONF_U2DS           BIT(27)

#define CDNS_USB_STS        (0x20004)
#define STS_USB2CONS        BIT(20)
#define STS_INRST           BIT(10)
#define STS_LST(x)          ((x >> 26) & 0x0f)

#define CDNS_USB_CMD        (0x20008)
#define USB_CMD_SET_ADDR    BIT(0)
#define USB_CMD_ADDR(x)     ((x & 0x7f) << 1)

#define CDNS_USB_IEN        (0x20014)
#define CDNS_USB_ISTS       (0x20018)
#define ISTS_CON2I          BIT(16) // HS/FS mode connection detected
#define ISTS_U2RESI         BIT(18) // USB reset in HS/FS mode ends
#define ISTS_CFGRESI        BIT(26) // USB configuration reset detected

#define CDNS_USB_EP_SEL     (0x2001C)
#define EP_SEL_DIR(x)       ((x & 1) << 7)
#define EP_SEL(x)           (x & 0x0f)

#define CDNS_USB_EP_TRADDR  (0x20020)
#define CDNS_USB_EP_CFG     (0x20024)
#define EP_CFG_MAX_PKT_SZ(x) ((x & 0x7ff) << 16)
#define EP_CFG_TYPE(x)      ((x & 0x03) << 1)
#define EP_CFG_STREAM_EN    BIT(3)
#define EP_CFG_ENABLE       BIT(0)

#define CDNS_USB_EP_CMD     (0x20028)
#define EP_CMD_SSTALL       BIT(1)
#define EP_CMD_ERDY         BIT(3)
#define EP_CMD_REQ_CMPL     BIT(5)
#define EP_CMD_DRDY         BIT(6)

#define CDNS_USB_EP_STS_EN  (0x20034)
#define EP_STS_EN_SETUP     BIT(0)
#define EP_STS_EN_DESCMISEN BIT(4)
#define EP_STS_EN_TRBERREN  BIT(7)

#define CDNS_USB_EP_STS     (0x2002C)
#define EP_STS_SETUP        BIT(0)
#define EP_STS_IOC          BIT(2)
#define EP_STS_DESCMIS      BIT(4)
#define EP_STS_TRBERR       BIT(7)

#define CDNS_USB_EP_STS_SID (0x20030)
#define CDNS_USB_DRBL       (0x20038)
#define CDNS_USB_EP_IEN     (0x2003C)
#define CDNS_USB_EP_ISTS    (0x20040)
#define CDNS_USB_PWR        (0x20044)
#define CDNS_USB_CONF2      (0x20048)
#define CDNS_USB_CAP1       (0x2004C)
#define CDNS_USB_CAP2       (0x20050)
#define CDNS_USB_CAP3       (0x20054)
#define CDNS_USB_CAP5       (0x2005C)
#define CDNS_USB_CAP6       (0x20060) /* Device controller version */
#define CDNS_USB_CPKT1      (0x20064)
#define CDNS_USB_CPKT2      (0x20068)
#define CDNS_USB_CPKT3      (0x2006C)
#define CDNS_USB_DBG_LINK1  (0x20104)
#define DBG_LINK1_LFPS_MIN_GEN_U1_EXIT_SET BIT(25)
#define DBG_LINK1_LFPS_MIN_GEN_U1_EXIT(x) ((x & 0xff) << 8)

/* Transfer descriptor & bits*/
struct cdns_trb
{
    uint32_t addr;
    uint32_t length;
    uint32_t flags;
} __packed;

#define TRB_MAX_LENGTH (127 * 1024) // (130048)
#define TRB_BURST_LENGTH(x) ((x & 0xFF) << 16)

#define TRB_FLAG_CYCLE  BIT(0)
#define TRB_FLAG_TOGGLE BIT(1)
#define TRB_FLAG_CHAIN  BIT(4)
#define TRB_FLAG_IOC    BIT(5)
#define TRB_TYPE(x) ((x) << 10)
#define TRB_TYPE_NORMAL (1)
#define TRB_TYPE_LINK   (6)

static uintptr_t base;
static struct usb_setup_packet setup_pkt __aligned(64); // TODO: Define CASHE_WRITEBACK_GRANULE
static struct cdns_trb ep0_in_trb[2] __aligned(64);
static struct cdns_trb ep0_out_trb[2] __aligned(64);
static uint8_t align_buffer[512] __aligned(64);
#define CDNS_TRB_COUNT 128
static struct cdns_trb trb[CDNS_TRB_COUNT] __aligned(64); // TODO: Hardcoded amount of TRB's

static void *current_xfer_buf;
static size_t current_xfer_length;
static usb_ep_t current_xfer_ep;

static int select_ep(usb_ep_t ep)
{
    uint8_t dir = ep % 2; // 0 = Out, 1 = In
    uint8_t ep_val = ep / 2;

    if (ep >= USB_EP_END)
        return -PB_ERR_PARAM;

    mmio_write_32(base + CDNS_USB_EP_SEL, EP_SEL_DIR(dir) | EP_SEL(ep_val));
    return PB_OK;
}

static int ep0_xfer_start(void *buf, size_t length, bool in_xfer)
{
    void *buf_p = buf;
    struct cdns_trb *trb_p = in_xfer?ep0_in_trb:ep0_out_trb;

    if (length > TRB_MAX_LENGTH)
        return -PB_ERR_PARAM;

    if (in_xfer) {
        // TODO: Temparary alignment fix
        memcpy(align_buffer, buf, length);
        buf_p = align_buffer;
    }

    if (buf_p) {
        arch_clean_cache_range((uintptr_t) buf_p, length);
    }

    //LOG_DBG("%s: %p, len=%zu", in_xfer?"IN":"OUT", buf_p, length);

    trb_p->addr = (uint32_t)(uintptr_t) buf_p;
    trb_p->length = (uint32_t) length;
    trb_p->flags = TRB_FLAG_CYCLE | TRB_FLAG_IOC | TRB_TYPE(TRB_TYPE_NORMAL);
    arch_clean_cache_range((uintptr_t) trb_p, sizeof(*trb_p));

    select_ep(in_xfer?USB_EP0_IN:USB_EP0_OUT);
    mmio_write_32(base + CDNS_USB_EP_STS, EP_STS_IOC | EP_STS_TRBERR);
    mmio_write_32(base + CDNS_USB_EP_TRADDR, (uint32_t)(uintptr_t) trb_p);
    mmio_write_32(base + CDNS_USB_EP_CMD, EP_CMD_DRDY);

    return PB_OK;
}

int cdns3_udc_core_xfer_start(usb_ep_t ep, void *buf, size_t length)
{
    size_t chunk;
    size_t bytes_to_xfer = length;
    struct cdns_trb *trb_p = &trb[0];
    uintptr_t buf_p = (uintptr_t) buf;
    uint8_t ep_val = ep / 2;
    size_t desc_count = 0;
    bool in_xfer = !!(ep % 2);

    if (ep >= USB_EP_END)
        return -PB_ERR_PARAM;

    LOG_DBG("%s: buf=%p, len=%zu", ep_to_str(ep), buf, length);

    if (ep_val == 0) {
        return ep0_xfer_start(buf, length, in_xfer);
    }

    if (length > (TRB_MAX_LENGTH * CDNS_TRB_COUNT))
        return -PB_ERR_PARAM;

    // TODO: xfer alignment, what is the minimum alignment?
    current_xfer_ep = ep;
    current_xfer_buf = buf;
    current_xfer_length = length;

    arch_clean_cache_range((uintptr_t) buf, length);

    while (bytes_to_xfer > 0) {
        chunk = bytes_to_xfer > TRB_MAX_LENGTH?TRB_MAX_LENGTH:bytes_to_xfer;

        trb_p->addr = (uint32_t) buf_p;
        //TODO: Burst size, depends on EP size
        // EP sz   Burst sz
        // -----   --------
        // 1024    128
        // => 512  64
        // < 512   16
        trb_p->length = (uint32_t) chunk | TRB_BURST_LENGTH(64);
        trb_p->flags = TRB_FLAG_CYCLE | TRB_TYPE(TRB_TYPE_NORMAL);

        bytes_to_xfer -= chunk;
        buf_p += chunk;

        /* Last TRB should generate completion IRQ */
        if (bytes_to_xfer == 0) {
            trb_p->flags |= TRB_FLAG_IOC;
        }

        LOG_DBG("desc[%lu]: addr=%x, len=%zu, flags=%x",
                    desc_count, trb_p->addr, chunk, trb_p->flags);

        trb_p++;
        trb_p->flags = 0;
        desc_count++;
    }

    /* Stop gap TRB */
    trb_p->addr = 0;
    trb_p->length = 0;
    trb_p->flags = 0;

    arch_clean_cache_range((uintptr_t) trb, sizeof(trb[0]) * desc_count);

    select_ep(ep);
    mmio_write_32(base + CDNS_USB_EP_STS, EP_STS_IOC | EP_STS_TRBERR);
    mmio_write_32(base + CDNS_USB_EP_TRADDR, (uint32_t)(uintptr_t) trb);
    mmio_write_32(base + CDNS_USB_EP_CMD, EP_CMD_DRDY);

    return PB_OK;
}

int cdns3_udc_core_xfer_complete(usb_ep_t ep)
{
    bool in_xfer = !!(ep % 2);
    uint32_t ep_sts;

    select_ep(ep);
    ep_sts = mmio_read_32(base + CDNS_USB_EP_STS);

    if (ep_sts & EP_STS_TRBERR) {
        mmio_write_32(base + CDNS_USB_EP_STS, EP_STS_TRBERR);

        LOG_DBG("%s: TRBERR (0x%x)", ep_to_str(ep), ep_sts);
        struct cdns_trb *trb_p = (struct cdns_trb *)(uintptr_t) mmio_read_32(base + CDNS_USB_EP_TRADDR);
        arch_invalidate_cache_range((uintptr_t) trb_p, sizeof(struct cdns_trb));
        LOG_DBG("  addr=%x, len=%zu, flags=%x", trb_p->addr, trb_p->length & 0xffff, trb_p->flags);
        LOG_DBG("  trb_base=%p, trb_p=0x%x", (void *) trb, trb_p);

        // Re-start DMA
        mmio_write_32(base + CDNS_USB_EP_CMD, EP_CMD_DRDY);
    }

    if ((ep_sts & EP_STS_IOC)) {
        LOG_DBG("%s: Xfer done %x", ep_to_str(ep), ep_sts);

        mmio_write_32(base + CDNS_USB_EP_STS, EP_STS_IOC);

        if (ep == current_xfer_ep) {
            if (!in_xfer && current_xfer_buf && current_xfer_length > 0) {
                LOG_DBG("Invalidate %p, len=%zu", current_xfer_buf, current_xfer_length);
                arch_invalidate_cache_range((uintptr_t) current_xfer_buf,
                                                        current_xfer_length);
            }

            current_xfer_ep = -1;
            current_xfer_buf = NULL;
            current_xfer_length = 0;
        }

        return PB_OK;
    } else {
        return -PB_ERR_AGAIN;
    }
}

void cdns3_udc_core_xfer_cancel(usb_ep_t ep)
{
    (void) ep;

    // Not needed in this driver since we have dedicated descriptors
    // for ep0
}

static void cdns_process_irq(void)
{
    uint32_t ists = mmio_read_32(base + CDNS_USB_ISTS);

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

    ep_ists = mmio_read_32(base + CDNS_USB_EP_ISTS);
    mmio_write_32(base + CDNS_USB_EP_ISTS, ep_ists);
    if (ep_ists & 1) { /* EP0 Out */
        select_ep(USB_EP0_OUT);
        ep_sts = mmio_read_32(base + CDNS_USB_EP_STS);

        if (ep_sts & EP_STS_SETUP) {
            arch_invalidate_cache_range((uintptr_t) &setup_pkt, sizeof(setup_pkt));
            memcpy(pkt, &setup_pkt, sizeof(setup_pkt));
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

int cdns3_udc_core_configure_ep(usb_ep_t ep, enum usb_ep_type ep_type, size_t pkt_sz)
{
    uint32_t ep_num = ep / 2;
    LOG_INFO("ep=%u, type=%i, pkt_sz=%zu", ep, ep_type, pkt_sz);

    // TODO's
    //  o Hard coded bulk EP type
    //  o Enable both IN/OUT INT
    //
    select_ep(ep);
    mmio_write_32(base + CDNS_USB_EP_CFG, EP_CFG_ENABLE |
                                               EP_CFG_TYPE(2) |
                                               EP_CFG_MAX_PKT_SZ(pkt_sz));

    mmio_write_32(base + CDNS_USB_EP_STS_EN, EP_STS_EN_DESCMISEN |
                                                  EP_STS_EN_TRBERREN);

    /* Enable interrupt for EP0 in/out */
    mmio_write_32(base + CDNS_USB_EP_IEN, (1 << (16 + ep_num)) | (1 << ep_num));
    return PB_OK;
}

int cdns3_udc_core_set_address(uint16_t addr)
{
    mmio_clrsetbits_32(base + CDNS_USB_CMD, 0xff,
                        USB_CMD_SET_ADDR | USB_CMD_ADDR(addr));

    return PB_OK;
}

int cdns3_udc_core_xfer_zlp(usb_ep_t ep)
{
    select_ep(ep);
    /**
     * We set ERDY here because there is no data stage in the set address req.
     * This means that we exit the setup stage and skip the data stage,
     * since there is no data to write to the host.
     *
     * Setting REQ_CMPL means that we send the ACK when we enter the status stage.
     */

    if (ep == USB_EP0_IN) {
        mmio_write_32(base + CDNS_USB_EP_CMD, EP_CMD_ERDY |
                                                   EP_CMD_REQ_CMPL);

        // TODO: Timeout?
        while(mmio_read_32(base + CDNS_USB_EP_CMD) & EP_CMD_REQ_CMPL);
    } else if (ep == USB_EP0_OUT) {
        mmio_write_32(base + CDNS_USB_EP_CMD, EP_CMD_ERDY);
    }

    return PB_OK;
}

int cdns3_udc_core_init(void)
{
    uint32_t dev_ctrl_version = 0;

    LOG_INFO("Probing @ 0x%"PRIxPTR"", base);

    dev_ctrl_version = mmio_read_32(base + CDNS_USB_CAP6);

    LOG_INFO("Device controller version: 0x%x", dev_ctrl_version);


    /* Wait for controller to come out of reset */
    while (!(mmio_read_32(base + CDNS_USB_STS) & BIT(10)))
        ;

    /* Configure and enable EP0 */
    // TODO: Hardcoded values for SS

    /* Enable all irq's */
    mmio_write_32(base + CDNS_USB_IEN, 0xffffffff);

    select_ep(USB_EP0_OUT);
    // STREAM_EN should probably be set before we can get SS working
    mmio_write_32(base + CDNS_USB_EP_CFG, EP_CFG_ENABLE |
                                               EP_CFG_MAX_PKT_SZ(512));

    mmio_write_32(base + CDNS_USB_EP_STS_EN, EP_STS_EN_SETUP |
                                                  EP_STS_EN_DESCMISEN |
                                                  EP_STS_EN_TRBERREN);

    select_ep(USB_EP0_IN);
    mmio_write_32(base + CDNS_USB_EP_CFG, EP_CFG_ENABLE |
                                               EP_CFG_MAX_PKT_SZ(512));

    mmio_write_32(base + CDNS_USB_EP_STS_EN, EP_STS_EN_SETUP |
                                                  EP_STS_EN_TRBERREN);

    /* Enable interrupt for EP0 in/out */
    mmio_write_32(base + CDNS_USB_EP_IEN, BIT(16) | BIT(0));


    mmio_write_32(base + CDNS_USB_CONF, CONF_CLK2OFFDS |
                                             CONF_CLK3OFFDS |
                                             CONF_L1DS);

    mmio_write_32(base + CDNS_USB_CONF, CONF_U1DS | CONF_U2DS);

    /* Configure DMA for multiple transfers. When the DMA has completed the
     * current trb or trb chain it will look at the next trb index for a valid
     * descriptor and continue transfering data */
    mmio_write_32(base + CDNS_USB_CONF, CONF_DMULT);
    mmio_write_32(base + CDNS_USB_CONF, CONF_DEV_EN);

    mmio_write_32(base + CDNS_USB_DBG_LINK1,
                        DBG_LINK1_LFPS_MIN_GEN_U1_EXIT_SET |
                        DBG_LINK1_LFPS_MIN_GEN_U1_EXIT(0x3c));

    // Maybe we don't need to do this here
    ep0_xfer_start(&setup_pkt, sizeof(setup_pkt), false);

    LOG_INFO("usb_sts = 0x%x", mmio_read_32(base + CDNS_USB_STS));

    return PB_OK;
}

int cdns3_udc_core_stop(void)
{
    LOG_INFO("Stop");
    return PB_OK;
}

void cdns3_udc_core_set_base(uintptr_t base_)
{
    base = base_;
}
