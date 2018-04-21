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
#include <uuid/uuid.h>

#include "recovery.h"
#include "crc.h"
#include "pb_types.h"
#include "gpt.h"
#include "utils.h"

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


static int pb_write(libusb_device_handle *h, struct pb_cmd *cmd) {
    int err = 0;
    int tx_sz = 0;

    err = libusb_interrupt_transfer(h,
                0x02,
                (uint8_t *) cmd, sizeof(struct pb_cmd) , &tx_sz, 0);
    
    if (err < 0) {
        printf ("USB: cmd=0x%2.2x, transfer err = %i\n",cmd->cmd, err);
    }

    return err;
}

static int pb_read(libusb_device_handle *h, u8 *bfr, u32 sz) {
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


static int pb_reset(libusb_device_handle *h) {
    struct pb_cmd c;
    c.cmd = PB_CMD_DO_RESET;
    printf ("Sending RESET\n");
    return pb_write(h, &c);
}

static int pb_boot_part(libusb_device_handle *h, u8 part_no) {
    struct pb_cmd c;
    c.cmd = PB_CMD_BOOT_PART;
    printf ("Booting\n");
    return pb_write(h, &c);
}


static int pb_print_version(libusb_device_handle *h) {
    struct pb_cmd c;
    struct pb_cmd response;
    int err;

    c.cmd = PB_CMD_GET_VERSION;
    err = pb_write(h, &c);

    if (err) {
        return err;
    }

    pb_read(h, (u8*) &response, sizeof(struct pb_cmd));

    printf ("PB Version: %s\n",response.data);

    return 0;

}

static int pb_print_gpt_table(libusb_device_handle *h) {
    struct pb_cmd c;
    struct gpt_primary_tbl gpt;
    struct gpt_part_hdr *part;
    char str_type_uuid[37];
    int err;
    u8 tmp_string[64];

    c.cmd = PB_CMD_GET_GPT_TBL;
    err = pb_write(h, &c);

    if (err) {
        printf ("pb_print_gpt_table: %i\n",err);
        return err;
    }

    err = pb_read(h, (u8*) &gpt, sizeof(struct gpt_primary_tbl));

    if (err) {
        printf ("pb_print_gpt_table: %i\n",err);
        return err;
    }
    printf ("GPT Table:\n");
    for (int i = 0; i < gpt.hdr.no_of_parts; i++) {
        part = &gpt.part[i];

        if (!part->first_lba)
            break;
        
        uuid_unparse_upper(part->type_uuid, str_type_uuid);
        utils_gpt_part_name(part, tmp_string, 36);
        printf (" %i - [%16s] lba 0x%8.8X - 0x%8.8X, TYPE: %s\n", i,
                tmp_string,
                part->first_lba, part->last_lba,
                str_type_uuid);
                                
    }

    return 0;
}



static int pb_flash_part (libusb_device_handle *h, u8 part_no, const char *f_name) {
    int read_sz = 0;
    int sent_sz = 0;
    int buffer_id = 0;
    int err;
    FILE *fp = NULL; 
    unsigned char *bfr = NULL;
    struct pb_cmd_prep_buffer bfr_cmd;
    struct pb_cmd_write_part wr_cmd;

    fp = fopen (f_name,"rb");

    if (fp == NULL) {
        printf ("Could not open file: %s\n",f_name);
        return -1;
    }

    bfr =  malloc(1024*1024*8);
    
    bfr_cmd.cmd = PB_CMD_PREP_BULK_BUFFER;
    wr_cmd.cmd = PB_CMD_WRITE_PART;
    wr_cmd.lba_offset = 0;
    wr_cmd.part_no = part_no;
    printf ("Writing");
    fflush(stdout);
    while ((read_sz = fread(bfr, 1, 1024*1024*8, fp)) >0) {
       bfr_cmd.no_of_blocks = read_sz / 512;
        if (read_sz % 512)
            bfr_cmd.no_of_blocks++;
        
        bfr_cmd.buffer_id = buffer_id;
        pb_write(h, (u8*)&bfr_cmd);

        err = libusb_bulk_transfer(h,
                    1,
                    bfr,
                    read_sz,
                    &sent_sz,
                    1000);
 
        if (err != 0) {
            printf ("USB: Bulk xfer error, err=%i\n",err);
            goto err_xfer;
        }

        wr_cmd.no_of_blocks = bfr_cmd.no_of_blocks;
        wr_cmd.buffer_id = buffer_id;
        buffer_id = !buffer_id;
        //printf ("wr: %i kBytes read_sz = %i, send_sz = %i\n",bfr_cmd.no_of_blocks*512/1024, read_sz, sent_sz);
        printf (".");
        fflush(stdout);
        pb_write(h, (u8 *) &wr_cmd);
        wr_cmd.lba_offset += bfr_cmd.no_of_blocks;
 

    }
    printf ("Done\n");
err_xfer:
    free(bfr);
    fclose(fp);
    return err;
}




static int pb_program_bootloader (libusb_device_handle *h, const char *f_name) {
    int read_sz = 0;
    int sent_sz = 0;
    int err;
    FILE *fp = NULL; 
    unsigned char bfr[1024*1024*1];
    struct stat finfo;
    struct pb_cmd cmd;


    fp = fopen (f_name,"rb");

    if (fp == NULL) {
        printf ("Could not open file: %s\n",f_name);
        return -1;
    }

    stat(f_name, &finfo);
    cmd.cmd = PB_CMD_PREP_BULK_BUFFER;
    
    uint32_t *no_of_blocks = (uint32_t*) cmd.data;

    *no_of_blocks = finfo.st_size / 512;

    if (finfo.st_size % 512)
       *no_of_blocks = finfo.st_size/512+1;

    printf ("Installing bootloader, sz = %d blocks\n", *no_of_blocks);
    
    pb_write(h, &cmd);

    while ((read_sz = fread(bfr, 1, sizeof(bfr), fp)) >0) {
        err = libusb_bulk_transfer(h,
                    1,
                    bfr,
                    read_sz,
                    &sent_sz,
                    1000);
 
        if (err != 0) {
            printf ("USB: Bulk xfer error, err=%i\n",err);
            return -1;
        }
    }
    
    fclose(fp);

    cmd.cmd = PB_CMD_FLASH_BOOTLOADER;
    return pb_write(h, &cmd);
}

void print_help(void) {
    printf (" --- Punch BOOT ---\n\n");
    printf (" Bootloader:\n");
    printf ("  punchboot -b -f <file name> - Install bootloader\n");
    printf ("  punchboot -r                - Reset device\n");
    printf ("  punchboot -s                - BOOT System\n");
    printf ("\n");
    printf (" Partition Management:\n");
    printf ("  punchboot -l                - List partitions\n");
    printf ("  punchboot -w <n> -f <fn>    - Write 'fn' to partition 'n'\n");
}

int main(int argc, char **argv) {
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
        print_help();
        exit(0);
    }

	r = libusb_init(&ctx);
	if (r < 0)
		return r;


    int part_no = -1;
    bool flag_part_write = false;
    bool flag_bl_write = false;
    bool flag_reset = false;
    bool flag_read_pb_version = false;
    bool flag_list_part = false;
    bool flag_help = false;
    bool flag_boot_part = false;
    char *fn = NULL;

    while ((c = getopt (argc, argv, "hpslrbw:f:")) != -1) {
        switch (c) {
            case 'b':
                flag_bl_write = true;       
            break;
            case 'r':
                flag_reset = true;
            break;
            case 'l':
                flag_list_part = true;
            break;
            case 'w':
                part_no = atoi(optarg);
                flag_part_write = true;
            break;
            case 'f':
                fn = optarg;
            break;
            case 'p':
                flag_read_pb_version = true;
            break;
            case 's':
                flag_boot_part = true;
            break;
            case 'h':
                flag_help = true;
            break;
            default:
                abort ();
        }
    }
    if (flag_help) {
        print_help();
        exit(0);
    }

     
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

     if (flag_boot_part) {
        pb_boot_part(h,1);
    }

  
    if (flag_read_pb_version) {
        pb_print_version(h);
    }

    if (flag_list_part) {
        pb_print_gpt_table(h);
    }

    if (flag_bl_write && fn) {
        pb_program_bootloader(h, fn);
        if (flag_reset) 
            pb_reset(h);
    } else if (flag_part_write && fn) {
        printf ("Writing %s to part %i\n",fn, part_no);
        pb_flash_part(h, part_no, fn);
    } else if (flag_reset) {
        pb_reset(h);
    }
    

    libusb_exit(NULL);
	return 0;
}
