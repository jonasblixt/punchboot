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
#include <string.h>
#include <stdbool.h>
#include <libusb-1.0/libusb.h>

#include "recovery.h"
#include "crc.h"


static libusb_context *ctx = NULL;

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



#define PB_USB_REQUEST_TYPE 0x20
#define PB_PREP_BUFFER 0x21
#define PB_PROG_BOOTLOADER 0x22
#define PB_GET_VERSION 0x23


static int pb_send_command(libusb_device_handle *h, uint8_t cmd, uint16_t param0, uint8_t *bfr,
                                    uint32_t sz) {
    int err = 0;

    err = libusb_control_transfer(h,
                CTRL_OUT,
                cmd,
                param0,
                0,
                bfr, sz, 1000);

    if (err < 0) {
        printf ("USB: cmd=0x%2.2x, transfer err = %i\n",cmd, err);
    }

    return err;
}

static int pb_read(libusb_device_handle *h,uint8_t *bfr,
                                    uint32_t sz) {
    int err = 0;

    err = libusb_control_transfer(h,
                CTRL_IN,
                0x23,
                0,
                0,
                bfr, sz, 1000);

    if (err < 0) {
        printf ("USB: read error \n");
    }

    return err;
}


static int pb_program_part (libusb_device_handle *h, uint8_t part_no, 
                                                const char *f_name) {


    unsigned char tmp[1024*1024*4];
    int err;
    struct pb_usb_command_hdr *hdr = (struct pb_usb_command_hdr *) tmp;

    struct pb_write_part_hdr write_cmd;

    write_cmd.part_no = part_no;
    write_cmd.lba_offset = 0;
    write_cmd.no_of_blocks = 0;

    FILE *fp = fopen(f_name, "rb");
    
    unsigned char * chunk_ptr = tmp + sizeof(struct pb_usb_command_hdr) +
                                    sizeof(struct pb_chunk_hdr);
    unsigned char * payload_ptr = tmp + sizeof(struct pb_usb_command_hdr);
    
    struct pb_chunk_hdr * chunk_hdr = (struct pb_chunk_hdr *) payload_ptr;
    unsigned int chunk_count = 0;
    unsigned int total_size = 0;


    while (1) {
        int read_sz = fread(chunk_ptr , 1, 2048, fp);
        


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

        if (chunk_count >= 2048) {
            
            write_cmd.no_of_blocks = chunk_count*4;
            pb_send_command (h, PB_CMD_WRITE_PART, 0, (unsigned char*) &write_cmd, 
                                    sizeof(struct pb_write_part_hdr));
        
            write_cmd.lba_offset += chunk_count*4;
            chunk_count = 0;

            
        }

    }

    if (chunk_count) {
        write_cmd.no_of_blocks = chunk_count*4;
        pb_send_command (h, PB_CMD_WRITE_PART, 0, (unsigned char*) &write_cmd, 
                                sizeof(struct pb_write_part_hdr));
    }


    fclose(fp);


    return err;
}

static int pb_program_bootloader (libusb_device_handle *h, const char *f_name) {
    int read_sz = 0;
    int sent_sz = 0;
    int err;
    FILE *fp = fopen (f_name,"rb");
    unsigned char bfr[1024*1024*4];
    struct stat finfo;

    stat(f_name, &finfo);

    printf ("no_of_blocks = %i\n", (uint32_t) finfo.st_blocks);
    //pb_send_command(h, PB_PREP_BUFFER,(uint32_t) finfo.st_blocks,  NULL,0);
    while ((read_sz = fread(bfr, 1, 512, fp)) >0) {

        printf ("read_sz = %ib\n",read_sz);
        err = libusb_bulk_transfer(h,
                    1,
                    bfr,
                    read_sz,
                    &sent_sz,
                    1000);
        printf ("TX: %ib\n",sent_sz);
 
        if (err != 0) {
            printf ("USB: Bulk xfer error, err=%i\n",err);
            return -1;
        }

   }
    
    fclose(fp);

    //pb_send_command(h, PB_PROG_BOOTLOADER, 0, NULL, 0);
    return 0;
}
    /*
 * 
 *
 *
 * */

int main(int argc, char **argv)
{
    extern char *optarg;
    extern int optind, opterr, optopt;
    libusb_device_handle *h;
    char c;
	libusb_device **devs;
    libusb_device *dev;
	int r;
	ssize_t cnt;
    int err;


    if (argc <= 1) {
        printf (" --- Punch BOOT ---\n\n");
        printf (" Bootloader:\n");
        printf ("  punchboot -b -f <file name> - Install bootloader\n");
        printf ("  punchboot -r                - Reset device\n");
        printf ("\n");
        printf (" Partition Management:\n");
        printf ("  punchboot -l                - List partitions\n");
        printf ("  punchboot -w <n> -f <fn>    - Write 'fn' to partition 'b'\n");
        exit(0);
    }

	r = libusb_init(&ctx);
	if (r < 0)
		return r;

libusb_set_debug(ctx, 10);

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


   uint32_t pb_version = 0;

    //pb_send_command(h, PB_GET_VERSION, 0, NULL, 0);
    pb_read(h, &pb_version, 4);

    printf ("PB: Version = %x\n",pb_version);

    int part_no = -1;
    bool flag_part_write = false;
    bool flag_bl_write = false;
    bool flag_reset = false;
    char *fn = NULL;

    while ((c = getopt (argc, argv, "rbw:f:")) != -1) {
        switch (c) {
            case 'b':
                flag_bl_write = true;       
            break;
            case 'r':
                flag_reset = true;
            break;
            case 'w':
                part_no = atoi(optarg);
                flag_part_write = true;
            break;
            case 'f':
                fn = optarg;
            break;
            default:
                abort ();
        }
    }
    
    if (flag_bl_write && fn) {
        pb_program_bootloader(h, fn);
        if (flag_reset)
            pb_send_command(h, PB_CMD_RESET, 0, NULL, 0);

    } else if (flag_part_write && fn) {
        printf ("Writing %s to part %i\n",fn, part_no);
        pb_program_part(h, part_no, fn);  
    
        if (flag_reset)
            pb_send_command(h, PB_CMD_RESET, 0,  NULL, 0);
    }
    

    libusb_exit(NULL);
	return 0;
}
