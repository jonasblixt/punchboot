/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8M SPL's fastboot on pb's USB stack (D-089, plat-imx.md 2).
 *
 * The device is pb's vendor id with a product id of its own, and the
 * interface Android's fastboot and uuu look for (class 0xff, subclass 0x42,
 * protocol 3), on the two bulk endpoints pb's own device has. A command is
 * one OUT transfer that a short packet ends (usb.md 4.5), its answer one IN
 * transfer; a download goes where the caller said, in pieces the controller
 * takes.
 *
 * What the host can do here is before anything checked it (security.md 1):
 * it gets a download into the room the caller gave, and the caller's step
 * decides what starts. Nothing of the device is told but that it is pb's
 * SPL (security.md 4.7): no serial number, no version.
 */

#include <drivers/usb/usbd.h>
#include <pb/delay.h>
#include <pb/dma.h>
#include <pb/pb.h>
#include <string.h>

#include "fb_proto.h"
#include "fb_usb.h"

#ifndef PB_USB_VID
#define PB_USB_VID 0x1209
#endif

/* pid.codes' test product id, which the SPL reuses (D-089, Jonas) */
#ifndef PB_SPL_USB_PID
#define PB_SPL_USB_PID 0x0001
#endif

#define FB_USB_SUBCLASS 0x42
#define FB_USB_PROTOCOL 0x03

#define FB_USB_EP_IN    USB_EP1_IN
#define FB_USB_EP_OUT   USB_EP2_OUT

/* A download is read in pieces of whole packets that every controller's
 * longest transfer holds */
#ifndef FB_USB_CHUNK
#define FB_USB_CHUNK (4u * 1024 * 1024)
#endif

/* The time a host has to configure the device, and to move a transfer the
 * device waits for inside a command: the download's piece, an answer. No
 * time runs while the device waits for a command */
#define FB_USB_READY_TIMEOUT_us (10u * 1000 * 1000)
#define FB_USB_XFER_TIMEOUT_us  (10u * 1000 * 1000)

PB_STATIC_ASSERT(FB_USB_CHUNK % FB_USB_PACKET == 0, "FB_USB_CHUNK: whole packets");
PB_STATIC_ASSERT(FB_USB_CHUNK % PB_DMA_ALIGN == 0, "FB_USB_CHUNK: whole cache lines");
PB_STATIC_ASSERT(FB_CMD_MAX < FB_USB_PACKET, "A command is less than a packet");

static const struct usbd_descriptors descriptors = {
    .language = {
        .bLength = 0x04,
        .bDescriptorType = USB_DESC_TYPE_STRING,
        .wLang = USB_LANG_EN_US,
    },
    .qualifier = {
        .bLength = 0x0A,
        .bDescriptorType = USB_DESC_TYPE_QUALIFIER,
        .bcdUSB = USB_BCD_USB_2_0,
        .bMaxPacketSize0 = 0x40,
        .bNumConfigurations = 0x01,
    },
    .device = {
        .bLength = 0x12,
        .bDescriptorType = USB_DESC_TYPE_DEVICE,
        .bcdUSB = USB_BCD_USB_2_0,
        .bMaxPacketSize = 0x40,
        .idVendor = PB_USB_VID,
        .idProduct = PB_SPL_USB_PID,
        .iManufacturer = 0x01,
        .iProduct = 0x01,
        .iSerialNumber = 0x00, /* None */
        .bNumConfigurations = 0x01,
    },
    .config = {
        .bLength = 0x09,
        .bDescriptorType = USB_DESC_TYPE_CONFIG,
        .wTotalLength = 0x20, /* The configuration, the interface, two endpoints */
        .bNumInterfaces = 0x01,
        .bConfigurationValue = 0x01,
        .bmAttributes = USB_CONFIG_ATTR_ONE,
        .MaxPower = USB_MAX_POWER_500MA,
    },
    .interface = {
        .bLength = 0x09,
        .bDescriptorType = USB_DESC_TYPE_INTERFACE,
        .bNumEndpoints = 0x02,
        .bInterfaceClass = USB_CLASS_VENDOR,
        .bInterfaceSubClass = FB_USB_SUBCLASS,
        .bInterfaceProtocol = FB_USB_PROTOCOL,
    },
    .eps = {
        {
            .bLength = 0x07,
            .bDescriptorType = USB_DESC_TYPE_ENDPOINT,
            .bEndpointAddress = USB_EP_ADDR_IN | 1,
            .bmAttributes = USB_EP_ATTR_BULK,
            .wMaxPacketSize = FB_USB_PACKET,
        },
        {
            .bLength = 0x07,
            .bDescriptorType = USB_DESC_TYPE_ENDPOINT,
            .bEndpointAddress = 2,
            .bmAttributes = USB_EP_ATTR_BULK,
            .wMaxPacketSize = FB_USB_PACKET,
        },
    },
};

enum fb_usb_state {
    FB_USB_IDLE, /* Not started, or the session is over */
    FB_USB_CONNECT, /* Waiting for a host to configure the device */
    FB_USB_COMMAND, /* A command's transfer runs */
    FB_USB_DATA, /* A piece of a download runs */
    FB_USB_REPLY, /* An answer's transfer runs */
};

static struct {
    struct fb_usb_config cfg;
    struct fb_state proto;
    enum fb_usb_state state;
    enum fb_next after_reply;
    size_t offset; /* Of the download's piece that runs */
    size_t chunk; /* Its size */
    size_t received; /* What the controller got of a command or a piece */
    struct pb_timeout timeout;
} fb;

/* What the controller writes and reads of the SPL's (drivers.md 4): a
 * command's packet and an answer, each on lines of its own */
PB_DMA_ARRAY(uint8_t, fb_cmd, FB_USB_PACKET);
PB_DMA_ARRAY(char, fb_reply, FB_REPLY_MAX + 1);

static struct usb_string_descriptor *get_string_descriptor(uint8_t index)
{
    static struct usb_string_descriptor desc;
    static const char product[] = "Punchboot SPL";
    size_t n = sizeof(product) - 1;

    /* An index the class doesn't have: stalled (D-073) */
    if (index != 1)
        return NULL;

    memset(&desc, 0, sizeof(desc));
    desc.bLength = (uint8_t)(n * 2 + 2);
    desc.bDescriptorType = USB_DESC_TYPE_STRING;

    for (size_t i = 0; i < n; i++)
        desc.unicode[i] = (uint16_t)product[i];

    return &desc;
}

static void set_reply(const char *text)
{
    memcpy(fb_reply, text, strlen(text) + 1);
}

static void arm(uint32_t timeout_us)
{
    (void)pb_timeout_init_us(&fb.timeout, timeout_us);
}

/* The host is gone or started over (usb.md 2.4), or didn't move a transfer
 * in its time: the device leaves the bus and comes back (usb.md 2.1), and
 * what was downloaded is the last session's */
static int restart(void)
{
    int rc;

    LOG_INFO("fastboot: the session is lost, starting over");

    fb.state = FB_USB_IDLE;
    fb.proto.expect = 0;
    fb.proto.downloaded = 0;

    rc = usbd_disconnect();

    if (rc == PB_OK)
        rc = usbd_init();

    if (rc != PB_OK)
        return rc;

    fb.state = FB_USB_CONNECT;
    arm(FB_USB_READY_TIMEOUT_us);
    return -PB_ERR_AGAIN;
}

static int read_command(void)
{
    if (usbd_xfer_start_short(FB_USB_EP_OUT, fb_cmd, FB_USB_PACKET, &fb.received) != PB_OK)
        return restart();

    fb.state = FB_USB_COMMAND;
    return -PB_ERR_AGAIN;
}

static int send_reply(enum fb_next after)
{
    if (usbd_xfer_start(FB_USB_EP_IN, fb_reply, strlen(fb_reply)) != PB_OK)
        return restart();

    fb.after_reply = after;
    fb.state = FB_USB_REPLY;
    arm(FB_USB_XFER_TIMEOUT_us);
    return -PB_ERR_AGAIN;
}

static int read_chunk(void)
{
    fb.chunk = MIN(fb.proto.expect - fb.offset, (size_t)FB_USB_CHUNK);

    /* The controller writes whole packets: at most to the end of the room,
     * which is whole packets (fb_usb_start()) */
    if (usbd_xfer_start_short(FB_USB_EP_OUT, fb.cfg.dest + fb.offset, fb.chunk, &fb.received) !=
        PB_OK)
        return restart();

    fb.state = FB_USB_DATA;
    arm(FB_USB_XFER_TIMEOUT_us);
    return -PB_ERR_AGAIN;
}

static int command_done(void)
{
    enum fb_next next = fb_command(&fb.proto, (const char *)fb_cmd, fb.received, fb_reply);

    LOG_DBG("fastboot: a command of %u bytes (%i)", (unsigned int)fb.received, (int)next);

    if (next == FB_NEXT_BOOT) {
        if (fb.cfg.accept(fb.proto.downloaded) == PB_OK) {
            set_reply("OKAY");
        } else {
            /* What was refused isn't tried again */
            fb.proto.downloaded = 0;
            set_reply("FAILnot accepted");
            next = FB_NEXT_REPLY;
        }
    }

    return send_reply(next);
}

static int data_done(void)
{
    /* A piece that a short packet ended early: the host sent less than it
     * announced */
    if (fb.received != fb.chunk) {
        LOG_ERR("fastboot: %u bytes of a piece of %u",
                (unsigned int)fb.received,
                (unsigned int)fb.chunk);
        fb_data_done(&fb.proto, 0, fb_reply);
        return send_reply(FB_NEXT_REPLY);
    }

    fb.offset += fb.chunk;

    if (fb.offset < fb.proto.expect)
        return read_chunk();

    fb_data_done(&fb.proto, 1, fb_reply);
    return send_reply(FB_NEXT_REPLY);
}

static int reply_done(void)
{
    switch (fb.after_reply) {
    case FB_NEXT_DATA:
        fb.offset = 0;
        return read_chunk();
    case FB_NEXT_BOOT:
        /* The host has its answer: the device leaves the bus, and nothing of
         * the controller runs in what starts (drivers.md 2.3) */
        fb.state = FB_USB_IDLE;
        return usbd_disconnect();
    default:
        return read_command();
    }
}

int fb_usb_start(const struct fb_usb_config *cfg)
{
    static const struct usbd_cls_config cls_config = {
        .desc = &descriptors,
        .get_string_descriptor = get_string_descriptor,
    };
    int rc;

    fb.state = FB_USB_IDLE;

    if (cfg == NULL || cfg->accept == NULL || cfg->dest == NULL || cfg->room == 0 ||
        cfg->room % FB_USB_PACKET != 0 || !dma_owns_lines(cfg->dest, 0))
        return -PB_ERR_PARAM;

    fb.cfg = *cfg;
    memset(&fb.proto, 0, sizeof(fb.proto));
    fb.proto.room = cfg->room;

    rc = usbd_init_cls(&cls_config);

    if (rc == PB_OK)
        rc = usbd_init();

    if (rc != PB_OK)
        return rc;

    fb.state = FB_USB_CONNECT;
    arm(FB_USB_READY_TIMEOUT_us);
    return PB_OK;
}

int fb_usb_poll(void)
{
    int rc;

    switch (fb.state) {
    case FB_USB_CONNECT:
        rc = usbd_connect();

        if (rc == PB_OK)
            return read_command();

        if (rc == -PB_ERR_AGAIN && pb_timeout_has_expired(&fb.timeout)) {
            LOG_ERR("fastboot: no host");
            fb.state = FB_USB_IDLE;
            (void)usbd_disconnect();
            return -PB_ERR_TIMEOUT;
        }

        return rc;
    case FB_USB_COMMAND:
    case FB_USB_DATA:
    case FB_USB_REPLY:
        rc = usbd_xfer_complete();

        if (rc == -PB_ERR_AGAIN) {
            if (fb.state == FB_USB_COMMAND || !pb_timeout_has_expired(&fb.timeout))
                return rc;

            LOG_ERR("fastboot: the host didn't move a transfer");
            (void)usbd_xfer_cancel();
            return restart();
        }

        if (rc != PB_OK)
            return restart();

        if (fb.state == FB_USB_COMMAND)
            return command_done();

        if (fb.state == FB_USB_DATA)
            return data_done();

        return reply_done();
    default:
        return -PB_ERR_STATE;
    }
}
