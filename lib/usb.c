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
#include <pb/api.h>
#include <pb/protocol.h>
#include <pb/error.h>

struct pb_usb_private
{
    libusb_device *dev;
    libusb_context *usb_ctx;
    libusb_device_handle *h;
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
        return -PB_ERR;

    return PB_OK;
}

static int pb_usb_connect(struct pb_context *ctx)
{
    int rc = PB_OK;
    int i;
    bool found_device = false;
    struct pb_usb_private *priv = PB_USB_PRIVATE(ctx);
    libusb_device *dev;
    libusb_device **devs;

    if (libusb_get_device_list(NULL, &devs) < 0)
        return -PB_ERR;

    while ((dev = devs[i++]) != NULL)
    {
        struct libusb_device_descriptor desc;

        rc = libusb_get_device_descriptor(dev, &desc);

        if (rc < 0)
            continue;

        if ((desc.idVendor == PB_USB_VID) && (desc.idProduct == PB_USB_PID))
        {
            priv->dev = dev;
            found_device = true;
            break;
        }
    }

    if (!found_device)
    {
        rc = -PB_ERR;
        goto err_free_devs_out;
    }

    rc = libusb_open(priv->dev, &priv->h);

    if (rc != 0)
    {
        rc = -PB_ERR;
        goto err_free_devs_out;
    }

    if (libusb_kernel_driver_active(priv->h, 0))
         libusb_detach_kernel_driver(priv->h, 0);

    rc = libusb_claim_interface(priv->h, 0);

    if (rc != 0)
    {
        rc = -PB_ERR;
        goto err_close_dev_out;
    }

    if (priv->h == NULL)
        rc = -PB_ERR;

    libusb_free_device_list(devs, 1);

    if (rc == PB_OK)
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
    return PB_OK;
}

static int pb_usb_command(struct pb_context *ctx,
                            uint32_t cmd,
                            uint32_t arg0,
                            uint32_t arg1,
                            uint32_t arg2,
                            uint32_t arg3,
                            const void *bfr, size_t sz)
{
    struct pb_usb_private *priv = PB_USB_PRIVATE(ctx);
    struct pb_cmd_header hdr;
    int err = 0;
    int tx_sz = 0;

    hdr.magic = PB_PROTO_MAGIC;
    hdr.cmd = cmd;
    hdr.arg0 = arg0;
    hdr.arg1 = arg1;
    hdr.arg2 = arg2;
    hdr.arg3 = arg3;
    hdr.size = sz;

    err = libusb_interrupt_transfer(priv->h, 0x02, (uint8_t *) &hdr,
                                            sizeof(hdr), &tx_sz, 0);

    if (err < 0)
        return -PB_TRANSFER_ERROR;

    if (sz)
    {
        err = libusb_interrupt_transfer(priv->h, 0x02, (unsigned char *) bfr,
                                        sz, &tx_sz, 0);

        if (err < 0)
            return -PB_TRANSFER_ERROR;
    }

    return PB_OK;
}

static int pb_usb_read(struct pb_context *ctx, void *bfr, size_t sz)
{
    struct pb_usb_private *priv = PB_USB_PRIVATE(ctx);
    int err = 0;
    int rx_sz = 0;

    err = libusb_interrupt_transfer(priv->h, (LIBUSB_ENDPOINT_IN | 3),
                                                bfr, sz , &rx_sz, 1000);

    if (err < 0)
        return -PB_TRANSFER_ERROR;

    return PB_OK;
}

int pb_usb_transport_init(struct pb_context *ctx)
{
    ctx->transport = malloc(sizeof(struct pb_usb_private));

    if (!ctx->transport)
        return -PB_NO_MEMORY;

    memset(ctx->transport, 0, sizeof(struct pb_usb_private));

    ctx->free = pb_usb_free;
    ctx->init = pb_usb_init;
    ctx->read = pb_usb_read;
    ctx->command = pb_usb_command;
    ctx->connect = pb_usb_connect;

    return ctx->init(ctx);
}

