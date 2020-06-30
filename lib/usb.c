/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <libusb.h>
#include <pb-tools/api.h>
#include <pb-tools/wire.h>
#include <pb-tools/error.h>
#include <pb-tools/usb.h>

struct pb_usb_private
{
    libusb_device *dev;
    libusb_context *usb_ctx;
    libusb_device_handle *h;
    const char *device_uuid;
};

#define PB_USB_PRIVATE(__ctx) ((struct pb_usb_private *) ctx->transport)
#define PB_USB_VID 0x1209
#define PB_USB_PID 0x2019

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
    char device_serial[128];
    struct pb_usb_private *priv = PB_USB_PRIVATE(ctx);
    libusb_device *dev;
    libusb_device **devs;

    if (libusb_get_device_list(NULL, &devs) < 0)
        return -PB_RESULT_ERROR;

    while ((dev = devs[i++]) != NULL)
    {
        struct libusb_device_descriptor desc;

        rc = libusb_get_device_descriptor(dev, &desc);

        if (rc < 0)
            continue;

        if ((desc.idVendor == PB_USB_VID) && (desc.idProduct == PB_USB_PID))
        {
            if (priv->device_uuid)
            {
                libusb_open(dev, &priv->h);
                libusb_get_string_descriptor_ascii(priv->h, desc.iSerialNumber,
                         device_serial, sizeof(device_serial));

                libusb_close(priv->h);

                if (strcmp(device_serial, priv->device_uuid) != 0)
                    continue;
            }
            priv->dev = dev;
            found_device = true;
            break;
        }
    }

    if (!found_device)
    {
        rc = -PB_RESULT_ERROR;
        goto err_free_devs_out;
    }

    rc = libusb_open(priv->dev, &priv->h);

    if (rc != 0)
    {
        rc = -PB_RESULT_ERROR;
        goto err_free_devs_out;
    }

    if (libusb_kernel_driver_active(priv->h, 0))
         libusb_detach_kernel_driver(priv->h, 0);

    rc = libusb_claim_interface(priv->h, 0);

    if (rc != 0)
    {
        rc = -PB_RESULT_ERROR;
        goto err_close_dev_out;
    }

    if (priv->h == NULL)
        rc = -PB_RESULT_ERROR;

    libusb_free_device_list(devs, 1);

    if (rc == PB_RESULT_OK)
        ctx->connected = true;

    return rc;

err_close_dev_out:
    libusb_close(priv->h);
err_free_devs_out:
    libusb_free_device_list(devs, 1);
    return rc;
}

static int pb_usb_free(struct pb_context *ctx)
{
    libusb_exit(NULL);
    free(ctx->transport);
    return PB_RESULT_OK;
}


static int pb_usb_read(struct pb_context *ctx, void *bfr, size_t sz)
{
    struct pb_usb_private *priv = PB_USB_PRIVATE(ctx);
    int err = 0;
    int rx_sz = 0;

    err = libusb_bulk_transfer(priv->h, (LIBUSB_ENDPOINT_IN | 1),
                                                bfr, sz , &rx_sz, 10000);

    if (err < 0)
        return -PB_RESULT_TRANSFER_ERROR;

    return PB_RESULT_OK;
}

static int pb_usb_write(struct pb_context *ctx, const void *bfr, size_t sz)
{
    struct pb_usb_private *priv = PB_USB_PRIVATE(ctx);
    int err = 0;
    int rx_sz = 0;

    err = libusb_bulk_transfer(priv->h, (LIBUSB_ENDPOINT_OUT | 2),
                                             (void *) bfr, sz , &rx_sz, 10000);

    if (err < 0)
        return -PB_RESULT_TRANSFER_ERROR;

    return PB_RESULT_OK;
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
    ctx->connect = pb_usb_connect;

    return ctx->init(ctx);
}

