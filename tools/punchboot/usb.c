/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "usb.h"
#include "api.h"
#include <libusb-1.0/libusb.h>
#include <pb-tools/error.h>
#include <pb-tools/wire.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct pb_usb_private {
    libusb_device *dev;
    libusb_context *usb_ctx;
    libusb_device_handle *h;
    bool interface_claimed;
    const char *device_uuid;
};

#define PB_USB_PRIVATE(__ctx) ((struct pb_usb_private *)ctx->transport)
#define PB_USB_VID            0x1209
#define PB_USB_PID            0x2019

static void pb_usb_close_handle(struct pb_usb_private *p)
{
    if (p->h == NULL)
        return;

    libusb_close(p->h);
    p->h = NULL;
}

static int pb_usb_init(struct pb_context *ctx)
{
    int rc;
    struct pb_usb_private *priv = PB_USB_PRIVATE(ctx);

    rc = libusb_init(&priv->usb_ctx);

    if (rc < 0)
        return -PB_RESULT_ERROR;

    return PB_RESULT_OK;
}

static int pb_usb_connect(struct pb_context *ctx)
{
    int rc = PB_RESULT_OK;
    int i = 0;
    bool found_device = false;
    unsigned char device_serial[128];
    struct pb_usb_private *priv = PB_USB_PRIVATE(ctx);
    libusb_device *dev;
    libusb_device **devs;

    if (libusb_get_device_list(NULL, &devs) < 0)
        return -PB_RESULT_NOT_FOUND;

    while ((dev = devs[i++]) != NULL) {
        struct libusb_device_descriptor desc;

        rc = libusb_get_device_descriptor(dev, &desc);

        if (rc < 0)
            continue;

        if ((desc.idVendor == PB_USB_VID) && (desc.idProduct == PB_USB_PID)) {
            if (priv->device_uuid) {
                rc = libusb_open(dev, &priv->h);
                if (rc != 0)
                    continue;

                rc = libusb_get_string_descriptor_ascii(
                    priv->h, desc.iSerialNumber, device_serial, sizeof(device_serial));

                pb_usb_close_handle(priv);

                if (rc < 0)
                    continue;

                if (strcmp((char *)device_serial, priv->device_uuid) != 0)
                    continue;
            }
            priv->dev = dev;
            found_device = true;
            break;
        }
    }

    if (!found_device) {
        rc = -PB_RESULT_NOT_FOUND;
        goto err_free_devs_out;
    }

    rc = libusb_open(priv->dev, &priv->h);

    if (rc != 0) {
        rc = -PB_RESULT_NOT_FOUND;
        goto err_free_devs_out;
    }

    if (libusb_kernel_driver_active(priv->h, 0))
        libusb_detach_kernel_driver(priv->h, 0);

    rc = libusb_claim_interface(priv->h, 0);

    if (rc != 0) {
        rc = -PB_RESULT_ERROR;
        goto err_close_dev_out;
    }
    priv->interface_claimed = true;

    if (priv->h == NULL)
        rc = -PB_RESULT_ERROR;

    libusb_free_device_list(devs, 1);

    if (rc == PB_RESULT_OK)
        ctx->connected = true;

    return rc;

err_close_dev_out:
    pb_usb_close_handle(priv);
err_free_devs_out:
    libusb_free_device_list(devs, 1);
    return rc;
}

static int pb_usb_free(struct pb_context *ctx)
{
    struct pb_usb_private *priv = PB_USB_PRIVATE(ctx);

    if (priv->interface_claimed)
        libusb_release_interface(priv->h, 0);

    pb_usb_close_handle(priv);
    libusb_exit(priv->usb_ctx);
    free(ctx->transport);
    return PB_RESULT_OK;
}

static int pb_usb_read(struct pb_context *ctx, void *bfr, size_t sz)
{
    struct pb_usb_private *priv = PB_USB_PRIVATE(ctx);
    int err = 0;
    int rx_sz = 0;

    err = libusb_bulk_transfer(priv->h, (LIBUSB_ENDPOINT_IN | 1), bfr, sz, &rx_sz, 10000);

    if (err < 0)
        return -PB_RESULT_TRANSFER_ERROR;

    return PB_RESULT_OK;
}

static int pb_usb_write(struct pb_context *ctx, const void *bfr, size_t sz)
{
    struct pb_usb_private *priv = PB_USB_PRIVATE(ctx);
    int err = 0;
    int rx_sz = 0;

    err = libusb_bulk_transfer(priv->h, (LIBUSB_ENDPOINT_OUT | 2), (void *)bfr, sz, &rx_sz, 10000);

    if (err < 0)
        return -PB_RESULT_TRANSFER_ERROR;

    return PB_RESULT_OK;
}

static int pb_usb_list(struct pb_context *ctx,
                       void (*list_cb)(const char *uuid_str, void *priv),
                       void *list_cb_priv)
{
    int rc = PB_RESULT_OK;
    int i = 0;
    unsigned char device_serial[128];
    struct pb_usb_private *priv = PB_USB_PRIVATE(ctx);
    struct libusb_device_descriptor desc;
    libusb_device *dev;
    libusb_device **devs;

    if (libusb_get_device_list(NULL, &devs) < 0)
        return -PB_RESULT_ERROR;

    while ((dev = devs[i++]) != NULL) {
        rc = libusb_get_device_descriptor(dev, &desc);

        if (rc < 0)
            continue;

        if ((desc.idVendor == PB_USB_VID) && (desc.idProduct == PB_USB_PID)) {
            rc = libusb_open(dev, &priv->h);

            if (rc != 0)
                continue;

            rc = libusb_get_string_descriptor_ascii(
                priv->h, desc.iSerialNumber, device_serial, sizeof(device_serial));

            if (rc < 0)
                continue;

            pb_usb_close_handle(priv);

            list_cb((const char *)device_serial, list_cb_priv);
        }
    }

    libusb_free_device_list(devs, 1);

    return 0;
}

int pb_usb_transport_init(struct pb_context *ctx, const char *device_uuid)
{
    ctx->transport = malloc(sizeof(struct pb_usb_private));

    if (!ctx->transport)
        return -PB_RESULT_NO_MEMORY;

    memset(ctx->transport, 0, sizeof(struct pb_usb_private));

    struct pb_usb_private *priv = PB_USB_PRIVATE(ctx);
    priv->device_uuid = device_uuid;
    ctx->free = pb_usb_free;
    ctx->init = pb_usb_init;
    ctx->read = pb_usb_read;
    ctx->write = pb_usb_write;
    ctx->list = pb_usb_list;
    ctx->connect = pb_usb_connect;

    return ctx->init(ctx);
}
