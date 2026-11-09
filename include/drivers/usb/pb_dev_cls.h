/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_DRIVERS_USB_PB_DEV_CLS_H
#define INCLUDE_DRIVERS_USB_PB_DEV_CLS_H

#include <pb/cm.h>
#include <stddef.h>

int pb_dev_cls_init(void);
int pb_dev_cls_write(const void *buf, size_t length);
int pb_dev_cls_read(void *buf, size_t length);
int pb_dev_cls_xfer_complete(void);

/** Command mode over USB with the pb device class, on the usbd stack */
extern const struct cm_transport_ops pb_dev_cls_transport;

#endif // INCLUDE_DRIVERS_USB_PB_DEV_CLS_H
