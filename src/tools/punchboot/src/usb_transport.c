
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <libusb.h>
#include <pb/pb.h>
#include <pb/recovery.h>
#include <pb/usb.h>
#include "transport.h"


static libusb_device **devs;
static libusb_device *dev;
static libusb_context *ctx = NULL;
static libusb_device_handle *h = NULL;

static libusb_device * find_device(libusb_device **devs,
                                   uint8_t *usb_path,
                                   uint8_t usb_path_count)
{
    libusb_device *dev;
    int i = 0;
    uint8_t device_path[16];
    libusb_device *result = NULL;

    while ((dev = devs[i++]) != NULL)
    {
        struct libusb_device_descriptor desc;

        int r = libusb_get_device_descriptor(dev, &desc);

        if (r < 0)
        {
            return NULL;
        }

        if ( (desc.idVendor == PB_USB_VID) && (desc.idProduct == PB_USB_PID))
        {
            int path_count = libusb_get_port_numbers(dev, device_path, 16);


            if (usb_path_count == path_count)
            {
                bool found_device = true;

                for (int n = 0; n < path_count; n++)
                {
                    if (usb_path[n] != device_path[n])
                    {
                        found_device = false;
                        break;
                    }
                }

                if (!found_device)
                    continue;

                result = dev;
                break;
            }
            else if (usb_path_count == 0)
            {
                result = dev;
                break;
            }
        }
    }

    return result;
}

#define CTRL_IN            LIBUSB_ENDPOINT_IN |LIBUSB_REQUEST_TYPE_CLASS| \
                                                LIBUSB_RECIPIENT_INTERFACE
#define CTRL_OUT        LIBUSB_ENDPOINT_OUT|LIBUSB_REQUEST_TYPE_CLASS| \
                                                LIBUSB_RECIPIENT_INTERFACE

int transport_init(uint8_t *usb_path, uint8_t usb_path_count)
{
    int r = 0;
    int err = 0;
    int i = 0;
    int pb_device_count = 0;

    r = libusb_init(&ctx);
    if (r < 0)
        return r;

    if (libusb_get_device_list(NULL, &devs) < 0)
        return PB_ERR;


    if (usb_path_count == 0)
    {
        while ((dev = devs[i++]) != NULL)
        {
            struct libusb_device_descriptor desc;

            int r = libusb_get_device_descriptor(dev, &desc);

            if (r < 0)
                continue;

            if ( (desc.idVendor == PB_USB_VID) &&
                            (desc.idProduct == PB_USB_PID))
                pb_device_count++;
        }
    }

    if ((usb_path_count == 0) && (pb_device_count > 1))
    {
        printf("Error: Multiple devices detected. Append -u \"<usbpath>\"\n");
        printf("Found the following devices:\n");
        i = 0;

        while ((dev = devs[i++]) != NULL)
        {
            struct libusb_device_descriptor desc;

            int r = libusb_get_device_descriptor(dev, &desc);
            uint8_t device_path[16];

            if (r < 0)
                continue;

            if ( (desc.idVendor == PB_USB_VID) &&
                            (desc.idProduct == PB_USB_PID))
            {
                int path_count = libusb_get_port_numbers(dev, device_path,
                                            16);

                for (uint32_t n = 0; n < path_count; n++)
                {
                    printf("%u", device_path[n]);
                    if (n < (path_count-1))
                        printf(":");
                }
                printf("\n");
            }
        }
        return PB_ERR;
    }

    dev = find_device(devs, usb_path, usb_path_count);

    libusb_free_device_list(devs, 1);

    if (dev == NULL)
    {
        printf("Could not find device\n\r");
        libusb_exit(NULL);
        return -1;
    }

    err = libusb_open(dev, &h);

    if (err != 0)
    {
        printf("Could not open device\n");
        libusb_exit(NULL);
        return -1;
    }

    if (libusb_kernel_driver_active(h, 0))
         libusb_detach_kernel_driver(h, 0);

    err = libusb_claim_interface(h, 0);

    if (err != 0)
    {
        printf("Claim interface failed (%i)\n", err);
        return -1;
    }

    if (h == NULL)
    {
        printf("Could not open device\n");
        return -1;
    }

    return err;
}

void transport_exit(void)
{
    libusb_exit(NULL);
}


uint32_t pb_read_result_code(void)
{
    uint32_t result_code = PB_ERR;

    if (pb_read((uint8_t *) &result_code, sizeof(uint32_t)) != PB_OK)
        result_code = PB_ERR;

    return result_code;
}


int pb_write(uint32_t cmd, uint32_t arg0,
                           uint32_t arg1,
                           uint32_t arg2,
                           uint32_t arg3,
                           uint8_t *bfr, int sz)
{
    struct pb_cmd_header hdr;
    int err = 0;
    int tx_sz = 0;


    hdr.cmd = cmd;
    hdr.arg0 = arg0;
    hdr.arg1 = arg1;
    hdr.arg2 = arg2;
    hdr.arg3 = arg3;
    hdr.size = sz;

    err = libusb_interrupt_transfer(h,
                0x02,
                (uint8_t *) &hdr, sizeof(struct pb_cmd_header) , &tx_sz, 0);

    if (err < 0) {
        printf("USB: cmd=0x%2.2x, transfer err = %i\n", cmd, err);
        return err;
    }

    if (sz)
    {
        err = libusb_interrupt_transfer(h, 0x02, bfr, sz, &tx_sz, 0);

        if (err < 0)
        {
            printf("USB: cmd=0x%2.2x, transfer err = %i\n", cmd, err);
            return err;
        }
    }

    return err;
}

int pb_read(uint8_t *bfr, int sz)
{
    int err = 0;
    int rx_sz = 0;

    err = libusb_interrupt_transfer(h,
                LIBUSB_ENDPOINT_IN|3,
                bfr, sz , &rx_sz, 10000);

    if (err < 0)
    {
        printf("pb_read: %i %i\n", err, rx_sz);
    }

    return err;
}


int pb_write_bulk(uint8_t *bfr, int sz, int *sz_tx)
{
    int err = 0;

    err = libusb_bulk_transfer(h,
                1,
                bfr,
                sz,
                sz_tx,
                1000);
    return err;
}
