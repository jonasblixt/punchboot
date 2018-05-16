/**
 * Punch BOOT bootloader cli
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
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

static int pb_install_default_gpt(libusb_device_handle *h) {
    struct pb_cmd c;
    c.cmd = PB_CMD_WRITE_DFLT_GPT;
    printf ("Installing default GPT table\n");
    return pb_write(h, &c);
}

static int pb_write_default_fuse(libusb_device_handle *h) {
    struct pb_cmd c;
    c.cmd = PB_CMD_WRITE_DFLT_FUSE;
    return pb_write(h, &c);
}


static int pb_write_uuid(libusb_device_handle *h) {
    struct pb_cmd c;
    c.cmd = PB_CMD_WRITE_UUID;
    uuid_t uuid;

    uuid_generate_random(uuid);
    memcpy(c.data, uuid, 16);

    return pb_write(h, &c);
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

static unsigned int pb_get_config_value(libusb_device_handle *h, int index) {
    struct pb_cmd c;
    int err;
    int value;

    c.cmd = PB_CMD_GET_CONFIG_VAL;
    c.data[0] = (u8) index;

    err = pb_write(h, &c);

    if (err) {
        return err;
    }

    pb_read(h, (u8 *) &value, 4);


    return value;
}

static int pb_get_config_tbl (libusb_device_handle *h) {
    struct pb_cmd c;
    struct pb_config_item item [127];
    int err;
    const char *access_text[] = {"  ","RW","RO","OTP"};

    c.cmd = PB_CMD_GET_CONFIG_TBL;
    err = pb_write(h,&c);

    if (err) {
        printf ("%s: Could not read config table\n",__func__);
        return err;
    }

    err = pb_read(h, (u8 *) item, sizeof(item));
    int n = 0;
    printf (    " Index   Description        Access  Default     Value\n");
    printf (    " -----   -----------        ------  -------     -----\n\n");
    do {
        printf (" %-3u     %-16s   %-3s     0x%8.8X  0x%8.8X\n",item[n].index,
                                     item[n].description,
                                     access_text[item[n].access],
                                     item[n].default_value,
                                     pb_get_config_value(h,n));
        n++;
    } while (item[n].index != -1);
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
    
    if (bfr == NULL) {
        printf ("Could not allocate memory\n");
        return -1;
    }

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
        pb_write(h, (struct pb_cmd *)&bfr_cmd);

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
        pb_write(h, (struct pb_cmd *) &wr_cmd);
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
    printf (" --- Punch BOOT " VERSION " ---\n\n");
    printf (" Bootloader:\n");
    printf ("  punchboot boot -w -f <fn>        - Install bootloader\n");
    printf ("  punchboot boot -r                - Reset device\n");
    printf ("  punchboot boot -s                - BOOT System\n");
    printf ("  punchboot boot -l                - Display version\n");
    printf ("\n");
    printf (" Partition Management:\n");
    printf ("  punchboot part -l                - List partitions\n");
    printf ("  punchboot part -w -n <n> -f <fn> - Write 'fn' to partition 'n'\n");
    printf ("  punchboot part -i                - Install default GPT table\n");
    printf ("\n");
    printf (" Configuration:\n");
    printf ("  punchboot config -l              - Display configuration\n");
    printf ("\n");
    printf (" Fuse Management (WARNING: these operations are OTP and can't be reverted):\n");
    printf ("  punchboot fuse -w -n <n>         - Install fuse set <n>\n");
    printf ("                        1          - Boot fuses\n\r");
    printf ("                        2          - Device Identity\n\r");
    printf ("\n");
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


    int cmd_index = -1;
    bool flag_write = false;
    bool flag_list = false;
    bool flag_reset = false;
    bool flag_help = false;
    bool flag_boot = false;
    bool flag_index = false;
    bool flag_install = false;

    char *fn = NULL;
    char *cmd = argv[1];

    while ((c = getopt (argc-1, &argv[1], "hiwrsln:f:")) != -1) {
        switch (c) {
            case 'w':
                flag_write = true;
            break;
            case 'r':
                flag_reset = true;
            break;
            case 'l':
                flag_list = true;
            break;
            case 'i':
                flag_install = true;
            break;
            case 'f':
                fn = optarg;
            break;
            case 'n':
                flag_index = true;
                cmd_index = atoi(optarg);
            break;
            case 's':
                flag_boot = true;
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

    if (strcmp(cmd, "boot") == 0) {
         if (flag_boot) {
            pb_boot_part(h,1);
        }
      
        if (flag_list) {
            pb_print_version(h);
        }

        if (flag_write) {
            pb_program_bootloader(h, fn);
           
        }
    
        if (flag_reset) 
            pb_reset(h);
    }

    if (strcmp(cmd, "part") == 0) {
        if (flag_list) 
            pb_print_gpt_table(h);
        else if (flag_write && flag_index && fn) {
            printf ("Writing %s to part %i\n",fn, cmd_index);
            pb_flash_part(h, cmd_index, fn);
        } else if (flag_install) {
            pb_install_default_gpt(h);
        } else {
            printf ("Nope, that did not work\n");
        }

        if (flag_reset) 
            pb_reset(h);
 
    }

    if (strcmp(cmd, "config") == 0) {
        if (flag_list) {
            pb_get_config_tbl(h);
        }
    }

    if (strcmp(cmd, "fuse") == 0) {

        if (flag_write && cmd_index > 0) {
            switch (cmd_index) {
                case 1:
                    printf ("Installing boot fuses...\n");
                    pb_write_default_fuse(h);
                break;
                case 2:
                    printf ("Installing unique ID\n");
                    pb_write_uuid(h);
                break;
            }
        }
    }

    libusb_exit(NULL);
	return 0;
}
