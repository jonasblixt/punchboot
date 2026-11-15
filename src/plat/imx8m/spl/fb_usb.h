/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8M SPL's fastboot (D-089, plat-imx.md 2) on pb's USB stack: its
 * device, and a session of commands (fb_proto.h) that ends when a download
 * was accepted. The controller is registered before (usbd_init_hal_ops()).
 */

#ifndef PLAT_IMX8M_SPL_FB_USB_H
#define PLAT_IMX8M_SPL_FB_USB_H

#include <stddef.h>
#include <stdint.h>

/* The bulk endpoints' packet at high speed */
#define FB_USB_PACKET 512

struct fb_usb_config {
    /* Where a download goes: on a cache line, memory the controller writes */
    uint8_t *dest;
    /* Its room: whole packets, so that a host that sends more than it
     * announced writes nothing past it */
    size_t room;
    /* The step that decides (D-089): whether the 'downloaded' bytes at
     * 'dest' may start. PB_OK: they may, and the session ends */
    int (*accept)(size_t downloaded);
};

/**
 * Starts the device: it attaches to the bus
 *
 * @param[in] cfg The download's place and the step that decides
 *
 * @return PB_OK, -PB_ERR_PARAM for a place that can't be used, or the
 *         controller's error
 */
int fb_usb_start(const struct fb_usb_config *cfg);

/**
 * One round of the session. It returns at once
 *
 * @return -PB_ERR_AGAIN while the session goes on; PB_OK when a download was
 *         accepted, the host has its answer and the device has left the bus;
 *         -PB_ERR_TIMEOUT when no host came; any other error is the
 *         controller's
 */
int fb_usb_poll(void);

#endif // PLAT_IMX8M_SPL_FB_USB_H
