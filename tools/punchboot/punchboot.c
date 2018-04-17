/**
 *
 * Partition types:
 *   - Configuration (conf)
 *   - System A (sysa)
 *   - System B (sysb)
 *
 * Commands:
 * 
 *   - Display system info
 *
 * Partition management:
 *   - Reset FS
 *   - Create part type=<UUID> nblks=<BLKS>
 *   - Delete part guid=<UUID>
 *   - List
 * 
 * Boot loader:
 *   - Reset bootloader
 *   - Boot A/B
 *   - Install bootloader
 *
 * Flash:
 *   - flash image=<IMAGE> part=<UUID>
 *   - verify part=<UUID>
 *
 * Device management:
 *   - List fuse names
 *   - Burn fuse=<NAME> value=<VAL>
 *    
 *
 *
 */

#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <libusb-1.0/libusb.h>

#include "recovery.h"
#include "crc.h"

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

        if ( (desc.idVendor == 0xFFFF) && (desc.idProduct == 0x0001)) {
            return dev;
        }
		
	}

    return NULL;
}


// HID Class-Specific Requests values. See section 7.2 of the HID specifications
#define HID_GET_REPORT			0x01
#define HID_GET_IDLE			0x02
#define HID_GET_PROTOCOL		0x03
#define HID_SET_REPORT			0x09
#define HID_SET_IDLE			0x0A
#define HID_SET_PROTOCOL		0x0B
#define HID_REPORT_TYPE_INPUT		0x01
#define HID_REPORT_TYPE_OUTPUT		0x02
#define HID_REPORT_TYPE_FEATURE		0x03
#define CTRL_IN			LIBUSB_ENDPOINT_IN |LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE
#define CTRL_OUT		LIBUSB_ENDPOINT_OUT|LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE


static int pb_send_command(libusb_device_handle *h, uint8_t cmd, uint8_t *bfr,
                                    uint32_t sz) {
    unsigned char tmp[4096];
    int err;
    struct pb_usb_command_hdr *hdr = tmp;
    unsigned char * payload_ptr = tmp + sizeof(struct pb_usb_command_hdr);
    
    
    memcpy (payload_ptr, bfr, sz);
    
    hdr->cmd = cmd;
    hdr->payload_crc = crc32(0, payload_ptr, sz);
    hdr->payload_sz = sz;
    hdr->header_crc = crc32(0, tmp, sizeof(struct pb_usb_command_hdr) - 4);

    uint32_t total_size = sz + sizeof(struct pb_usb_command_hdr);

    err = libusb_control_transfer(h,
                CTRL_OUT,
                HID_SET_REPORT,
                (HID_REPORT_TYPE_OUTPUT << 8) | 1,
                0,
                tmp, total_size, 1000);

    if (!err) {
        printf ("transfer err = %i\n",err);
    }

    return err;
}

static int pb_program_bootloader(libusb_device_handle *h, const char *f_name) {
    unsigned char tmp[4096];
    int err;
    struct pb_usb_command_hdr *hdr = tmp;

    FILE *fp = fopen(f_name, "rb");
    
    unsigned char * chunk_ptr = tmp + sizeof(struct pb_usb_command_hdr) +
                                    sizeof(struct pb_chunk_hdr);
    unsigned char * payload_ptr = tmp + sizeof(struct pb_usb_command_hdr);
    
    struct pb_chunk_hdr * chunk_hdr = payload_ptr;
    unsigned int chunk_count = 0;
    unsigned int total_size = 0;

    while (1) {
        int read_sz = fread(chunk_ptr , 1, 1024, fp);
        


        if (read_sz <= 0)
            break;

        chunk_hdr->chunk_no = chunk_count;
        chunk_hdr->chunk_sz = read_sz;
        chunk_count++;

        hdr->cmd = PB_CMD_TRANSFER_DATA;
        hdr->payload_crc = crc32(0, payload_ptr, read_sz + sizeof(struct pb_chunk_hdr));
        hdr->payload_sz = read_sz + sizeof(struct pb_chunk_hdr);
        hdr->header_crc = crc32(0, tmp, sizeof(struct pb_usb_command_hdr) - 4);


        total_size = read_sz + sizeof(struct pb_chunk_hdr) +
                            sizeof(struct pb_usb_command_hdr);

        err = libusb_control_transfer(h,
                    CTRL_OUT,
                    HID_SET_REPORT,
                    (HID_REPORT_TYPE_OUTPUT << 8) | 1,
                    0,
                    tmp, total_size, 1000);

        if (!err) {
            printf ("transfer err = %i\n",err);
            break;
        }
    }

    hdr->cmd = PB_CMD_FLASH_BOOT;
    hdr->payload_crc = 0;
    hdr->payload_sz = 0;
    hdr->header_crc = crc32(0, tmp, sizeof(struct pb_usb_command_hdr) - 4);



    err = libusb_control_transfer(h,
                CTRL_OUT,
                HID_SET_REPORT,
                (HID_REPORT_TYPE_OUTPUT << 8) | 1,
                0,
                tmp, sizeof(struct pb_usb_command_hdr), 1000);

    if (!err) 
        printf ("transfer err = %i\n",err);

    return err;
}

/*
 * 
 *
 *
 * */

int main(int argc, char **argv)
{
    libusb_device_handle *h;
    char c;
	libusb_device **devs;
    libusb_device *dev;
	int r;
	ssize_t cnt;
    int err;


    if (argc <= 1) {
        printf ("--- Punch BOOT ---\n");
        printf (" punchboot -b <file name>    - Install bootloader\n");
        printf (" punchboot -r                - Reset device\n");

        exit(0);
    }

	r = libusb_init(NULL);
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


    h = libusb_open_device_with_vid_pid(NULL, 0xffff, 0x0001);

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


    while ((c = getopt (argc, argv, "rb:")) != -1) {
        switch (c) {
            case 'b':
                pb_program_bootloader(h, optarg);
            break;
            case 'r':
                printf ("Resetting...\n");
                pb_send_command(h, PB_CMD_RESET, NULL, 0);
            break;

            default:
                abort ();
        }
    }


    libusb_exit(NULL);
	return 0;
}
