/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_DRIVERS_USB_PB_DEV_CLS_H
#define INCLUDE_DRIVERS_USB_PB_DEV_CLS_H

#include <stddef.h>

/*
 * pb's USB device class, on the usbd stack. It is command mode's transport
 * on USB: pb_dev_cls.c has the cm_transport_*() functions of pb/cm.h, which
 * a build links (usb.md 2).
 */
int pb_dev_cls_init(void);
int pb_dev_cls_write(const void *buf, size_t length);
int pb_dev_cls_read(void *buf, size_t length);
int pb_dev_cls_xfer_complete(void);

#endif // INCLUDE_DRIVERS_USB_PB_DEV_CLS_H
