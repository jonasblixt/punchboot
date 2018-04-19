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


#define CTRL_IN			LIBUSB_ENDPOINT_IN |LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE
#define CTRL_OUT		LIBUSB_ENDPOINT_OUT|LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE




static int pb_send_command(libusb_device_handle *h, struct pb_cmd *cmd) {
    int err = 0;
    int tx_sz = 0;

    err = libusb_interrupt_transfer(h,
                0x02,
                (uint8_t *) cmd, 0x40 /*sizeof(struct pb_cmd)*/ , &tx_sz, 0);

    printf ("send_command tx_sz=%i\n\r",tx_sz);

    if (err < 0) {
        printf ("USB: cmd=0x%2.2x, transfer err = %i\n",cmd->cmd, err);
    }

    return err;
}



static int pb_write(libusb_device_handle *h,u_int8_t cmd, uint16_t param0, uint8_t *bfr,
                                    uint32_t sz) {
    int err = 0;

    err = libusb_control_transfer(h,
                CTRL_OUT,
                cmd,
                param0,
                0,
                bfr, sz, 2000);

    if (err < 0) {
        printf ("USB: read error \n");
    }

    return err;
}


static int pb_read(libusb_device_handle *h,u_int8_t cmd, uint16_t param0, uint8_t *bfr,
                                    uint32_t sz) {
    int err = 0;

    err = libusb_control_transfer(h,
                CTRL_IN,
                cmd,
                param0,
                0,
                bfr, sz, 2000);

    if (err < 0) {
        printf ("USB: read error \n");
    }

    return err;
}

static int pb_program_bootloader (libusb_device_handle *h, const char *f_name) {
    int read_sz = 0;
    int sent_sz = 0;
    int err;
    uint16_t sts;
    FILE *fp = fopen (f_name,"rb");
    unsigned char bfr[1024*1024*1];
    struct stat finfo;
    uint8_t z_padding[511];

    stat(f_name, &finfo);
    uint16_t no_of_blocks = finfo.st_size / 512;
    if (finfo.st_size % 512)
        no_of_blocks++;

    printf ("no_of_blocks = %i\n", no_of_blocks);
    pb_read(h, PB_PREP_BUFFER, no_of_blocks, (uint8_t *) &sts ,2);

    printf ("sts = %i\n",sts);
    while ((read_sz = fread(bfr, 1, sizeof(bfr), fp)) >0) {

        //printf ("read_sz = %ib\n",read_sz);

        err = libusb_bulk_transfer(h,
                    1,
                    bfr,
                    read_sz,
                    &sent_sz,
                    1000);
        //printf ("TX: %ib\n",sent_sz);
 
        if (err != 0) {
            printf ("USB: Bulk xfer error, err=%i\n",err);
            return -1;
        }

   }
    
    int remainder = read_sz %512;
    //printf ("Remainder: %i\n", remainder);
    
    if (remainder) {
       printf ("Padding %i bytes...\n", remainder);
       err = libusb_bulk_transfer(h,
                    1,
                    z_padding,
                    remainder,
                    &sent_sz,
                    1000);
    }
    fclose(fp);

    pb_read(h, PB_PROG_BOOTLOADER, no_of_blocks, (uint8_t *) &sts, 2);

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

    //libusb_set_debug(ctx, 10);

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
    uint16_t status = 0;
    struct pb_cmd a;
    a.cmd = 0x00;

    pb_read(h,PB_GET_VERSION,0, &pb_version, 4);
    printf ("PB: Version = %x\n",pb_version);


    if (argc == 2)
    while (1) {
        printf ("Sending cmd...\n");
        a.cmd++;
        pb_send_command(h, &a);
        printf ("Done...\n");
    }
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
        if (flag_reset) {
            pb_read(h, PB_DO_RESET, 0, &status, 2);
        }
    } else if (flag_part_write && fn) {
        printf ("Writing %s to part %i\n",fn, part_no);
        //pb_program_part(h, part_no, fn);  
        
        status = 0;
        pb_write(h, PB_PROG_PART, 0, &status, 2);
        if (flag_reset)
            pb_read(h, PB_DO_RESET, 0,  &status, 2);
    }
    

    libusb_exit(NULL);
	return 0;
}
