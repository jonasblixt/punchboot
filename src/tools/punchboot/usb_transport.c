#include <stdio.h>
#include <libusb-1.0/libusb.h>
#include <pb/pb.h>
#include <pb/recovery.h>
#include "transport.h"


static libusb_device **devs;
static libusb_device *dev;
static libusb_context *ctx = NULL;
static libusb_device_handle *h = NULL;


static libusb_device * find_device(libusb_device **devs)
{
	libusb_device *dev;
	int i = 0;

	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0) {
			return NULL;
		}

        if ( (desc.idVendor == 0xffff) && (desc.idProduct == 0x0001)) {
            return dev;
        }
		
	}

    return NULL;
}

#define CTRL_IN			LIBUSB_ENDPOINT_IN |LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE
#define CTRL_OUT		LIBUSB_ENDPOINT_OUT|LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE

int transport_init(void)
{
    int r = 0;
	ssize_t cnt;
    int err = 0;

	r = libusb_init(&ctx);
	if (r < 0)
		return r;


	cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0)
		return (int) cnt;

	dev = find_device(devs);
	libusb_free_device_list(devs, 1);

    if (dev == NULL) {
        printf ("Could not find device\n\r");
        libusb_exit (NULL);
        return -1;
    }

    h = libusb_open_device_with_vid_pid(ctx, 0xffff, 0x0001);

    if (libusb_kernel_driver_active(h, 0))
		 libusb_detach_kernel_driver(h, 0);

    err = libusb_claim_interface(h, 0);
    
    if (err != 0) {
        printf ("Claim interface failed (%i)\n", err);
        return -1;
    }

    if (h == NULL) {
        printf ("Could not open device\n");
        return -1;
    }

    return err;
}

void transport_exit(void)
{
    libusb_exit(NULL);
}

int pb_write(uint32_t cmd, uint8_t *bfr, int sz) 
{
    struct pb_cmd_header hdr;
    int err = 0;
    int tx_sz = 0;

    hdr.cmd = cmd;
    hdr.size = sz;

    err = libusb_interrupt_transfer(h,
                0x02,
                (uint8_t *) &hdr, sizeof(struct pb_cmd_header) , &tx_sz, 0);
    
    if (err < 0) {
        printf ("USB: cmd=0x%2.2x, transfer err = %i\n",cmd, err);
        return err;
    }

    if (sz)
    {
        err = libusb_interrupt_transfer(h, 0x02, bfr, sz, &tx_sz, 0);
        
        if (err < 0) 
        {
            printf ("USB: cmd=0x%2.2x, transfer err = %i\n",cmd, err);
            return err;
        }
    }

    return err;
}

uint32_t pb_read_result_code(void)
{
    uint32_t result_code = PB_ERR;

    if (pb_read((uint8_t *) &result_code, sizeof(uint32_t)) != PB_OK)
        result_code = PB_ERR;

    return result_code;
}

int pb_read(uint8_t *bfr, int sz) 
{
    int err = 0;
    int rx_sz = 0;

    err = libusb_interrupt_transfer(h,
                LIBUSB_ENDPOINT_IN|3,
                bfr, sz , &rx_sz, 10000);
    
    if (err < 0) {
        printf ("pb_read: %i %i\n", err,rx_sz);
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
