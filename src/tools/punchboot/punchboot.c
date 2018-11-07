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
#include <string.h>

#include <pb/recovery.h>
#include <pb/config.h>
#include <pb/gpt.h>
#include <pb/image.h>

#include "crc.h"
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


static int pb_write(libusb_device_handle *h, uint32_t cmd, uint8_t *bfr,
                                             uint32_t sz) 
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
        
        if (err < 0) {
            printf ("USB: cmd=0x%2.2x, transfer err = %i\n",cmd, err);
        }
    }

    return err;
}

static int pb_read(libusb_device_handle *h, uint8_t *bfr, uint32_t sz) {
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
    printf ("Installing default GPT table\n");
    return pb_write(h, PB_CMD_WRITE_DFLT_GPT, NULL, 0);
}

static int pb_write_default_fuse(libusb_device_handle *h) {
    return pb_write(h, PB_CMD_WRITE_DFLT_FUSE, NULL, 0);
}

static int pb_write_uuid(libusb_device_handle *h) {
    uuid_t uuid;
    uuid_generate_random(uuid);
    return pb_write(h, PB_CMD_WRITE_UUID, (uint8_t *) uuid, 16);
}

static int pb_reset(libusb_device_handle *h) {
    printf ("Sending RESET\n");
    return pb_write(h, PB_CMD_RESET, NULL, 0);
}

static int pb_boot_part(libusb_device_handle *h, uint8_t part_no) {
    printf ("Booting\n");
    return pb_write(h, PB_CMD_BOOT_PART, NULL, 0);
}

static int pb_print_version(libusb_device_handle *h) {
    uint32_t sz = 0;
    char version_string[255];
    int err;

    err = pb_write(h, PB_CMD_GET_VERSION, NULL, 0);

    if (err) {
        return err;
    }

    pb_read(h, (uint8_t*) &sz, 4);
    pb_read(h, (uint8_t*) &version_string, sz);

    printf ("PB Version: %s\n",version_string);
    return 0;
}

static int pb_print_gpt_table(libusb_device_handle *h) {
    struct gpt_primary_tbl gpt;
    struct gpt_part_hdr *part;
    char str_type_uuid[37];
    uint32_t tbl_sz = 0;
    int err;
    uint8_t tmp_string[64];

    err = pb_write(h, PB_CMD_GET_GPT_TBL, NULL, 0);

    if (err) {
        printf ("pb_print_gpt_table: %i\n",err);
        return err;
    }

    err = pb_read(h, (uint8_t*) &tbl_sz, 4);
    
    if (err)
    {
        return err;
    }

    err = pb_read(h, (uint8_t*) &gpt, tbl_sz);

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
        printf (" %i - [%16s] lba 0x%8.8lX - 0x%8.8lX, TYPE: %s\n", i,
                tmp_string,
                part->first_lba, part->last_lba,
                str_type_uuid);
                                
    }

    return 0;
}

static unsigned int pb_get_config_value(libusb_device_handle *h, uint32_t index) 
{
    int err;
    int value;
    uint32_t sz;

    err = pb_write(h, PB_CMD_GET_CONFIG_VAL, (uint8_t *) &index, 4);

    if (err) {
        printf ("Error sending cmd\n");
        return err;
    }

    pb_read(h, (uint8_t *) &sz, 4);
    pb_read(h, (uint8_t *) &value, sz);

    return value;
}

static unsigned int pb_set_config_value(libusb_device_handle *h, 
                            uint32_t index, uint32_t val) 
{
    int err;

    uint32_t data[2];
    data[0] = index;
    data[1] = val;

    printf ("Setting %i to %x\n", index, val);
    err = pb_write(h, PB_CMD_SET_CONFIG_VAL, (uint8_t *) data, 8);

    if (err) {
        printf ("Error sending cmd\n");
        return err;
    }


    return 0;
}

static int pb_get_config_tbl (libusb_device_handle *h) {
    struct pb_config_item item [127];
    int err;
    uint32_t tbl_sz = 0;
    const char *access_text[] = {"  ","RW","RO","OTP"};

    err = pb_write(h, PB_CMD_GET_CONFIG_TBL, NULL, 0);

    if (err) {
        printf ("%s: Could not read config table\n",__func__);
        return err;
    }

    err = pb_read(h, (uint8_t *) &tbl_sz, 4);

    err = pb_read(h, (uint8_t *) &item, tbl_sz);

    int n = 0;
    printf (    " Index   Description        Access   Default      Value\n");
    printf (    " -----   -----------        ------  ----------  ----------\n\n");
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

static int pb_flash_part (libusb_device_handle *h, uint8_t part_no, const char *f_name) {
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

    wr_cmd.lba_offset = 0;
    wr_cmd.part_no = part_no;
    printf ("Writing");
    fflush(stdout);
    while ((read_sz = fread(bfr, 1, 1024*1024*8, fp)) >0) {
       bfr_cmd.no_of_blocks = read_sz / 512;
        if (read_sz % 512)
            bfr_cmd.no_of_blocks++;
        
        bfr_cmd.buffer_id = buffer_id;
        pb_write(h, PB_CMD_PREP_BULK_BUFFER, 
                (uint8_t *) &bfr_cmd, sizeof(struct pb_cmd_prep_buffer));

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
        //pb_write(h, (struct pb_cmd *) &wr_cmd);
        pb_write(h, PB_CMD_WRITE_PART, (uint8_t *) &wr_cmd,
                    sizeof(struct pb_cmd_write_part));

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
    uint32_t no_of_blocks = 0;
    struct stat finfo;
    struct pb_cmd_prep_buffer buffer_cmd;

    fp = fopen (f_name,"rb");

    if (fp == NULL) {
        printf ("Could not open file: %s\n",f_name);
        return -1;
    }

    stat(f_name, &finfo);

    no_of_blocks = finfo.st_size / 512;

    if (finfo.st_size % 512)
        no_of_blocks++;

    buffer_cmd.buffer_id = 0;
    buffer_cmd.no_of_blocks = no_of_blocks;


    printf ("Installing bootloader, sz = %d blocks\n", 
                    buffer_cmd.no_of_blocks);
    
    pb_write(h, PB_CMD_PREP_BULK_BUFFER, (uint8_t *) &buffer_cmd,
                                    sizeof(struct pb_cmd_prep_buffer));

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

    return pb_write(h, PB_CMD_FLASH_BOOTLOADER, (uint8_t *) &no_of_blocks, 4);
}




static int pb_execute_image (libusb_device_handle *h, const char *f_name) {
    int read_sz = 0;
    int sent_sz = 0;
    int err = 0;
    FILE *fp = NULL; 
    unsigned char *bfr = NULL;
    uint32_t data_remaining;
    uint32_t bytes_to_send;
    struct pb_pbi pbi;

    fp = fopen (f_name,"rb");

    if (fp == NULL) {
        printf ("Could not open file: %s\n",f_name);
        return -1;
    }

    bfr =  malloc(1024*64);
    
    if (bfr == NULL) {
        printf ("Could not allocate memory\n");
        return -1;
    }

    read_sz = fread(&pbi, 1, sizeof(struct pb_pbi), fp);

    pb_write(h, PB_CMD_BOOT_RAM, (uint8_t *) &pbi, sizeof(struct pb_pbi));
    
    printf ("Punchboot Image:\n");
    for (uint32_t i = 0; i < pbi.hdr.no_of_components; i++) {
        printf (" o %u - LA: 0x%8.8X OFF:0x%8.8X\n",i, 
                            pbi.comp[i].load_addr_low,
                            pbi.comp[i].component_offset);
    }

    for (uint32_t i = 0; i < pbi.hdr.no_of_components; i++) {
        printf ("Loading component %u, %u bytes...\n",i, 
                                pbi.comp[i].component_size);

        fseek (fp, pbi.comp[i].component_offset, SEEK_SET);
        data_remaining = pbi.comp[i].component_size;

        while ((read_sz = fread(bfr, 1, 1024*64, fp)) >0) {

            if (read_sz > data_remaining)
                bytes_to_send = data_remaining;
            else
                bytes_to_send = read_sz;

            err = libusb_bulk_transfer(h,
                        1,
                        bfr,
                        bytes_to_send,
                        &sent_sz,
                        1000);
     
            data_remaining = data_remaining - bytes_to_send;

            if (!data_remaining)
                break;

            if (err != 0) {
                printf ("USB: Bulk xfer error, err=%i\n",err);
                goto err_xfer;
            }
        }
    }

    printf ("Done\n");
err_xfer:
    free(bfr);
    fclose(fp);
    return err;
}

static void pb_display_device_info(libusb_device_handle *h)
{
    printf ("Device info:\n");
    printf (" Hardware: %s, Revision: %s\n");
    printf (" UUID: %s\n");
    printf (" Security State:\n");
}

static void print_help(void) {
    printf (" --- Punch BOOT " VERSION " ---\n\n");
    printf (" Bootloader:\n");
    printf ("  punchboot boot -w -f <fn>           - Install bootloader\n");
    printf ("  punchboot boot -r                   - Reset device\n");
    printf ("  punchboot boot -s                   - BOOT System\n");
    printf ("  punchboot boot -l                   - Display version\n");
    printf ("  punchboot boot -x -f <fn>           - Load image to RAM and execute it\n");
    printf ("\n");
    printf (" Device:\n");
    printf ("  punchboot dev -l                    - Display device information\n");
    printf ("\n");
    printf (" Partition Management:\n");
    printf ("  punchboot part -l                   - List partitions\n");
    printf ("  punchboot part -w -n <n> -f <fn>    - Write 'fn' to partition 'n'\n");
    printf ("  punchboot part -i                   - Install default GPT table\n");
    printf ("\n");
    printf (" Configuration:\n");
    printf ("  punchboot config -l                 - Display configuration\n");
    printf ("  punchboot config -w -n <n> -v <val> - Write <val> to key <n>\n");
    printf ("\n");
    printf (" Fuse Management (WARNING: these operations are OTP and can't be reverted):\n");
    printf ("  punchboot fuse -w -n <n>            - Install fuse set <n>\n");
    printf ("                        1             - Boot fuses\n\r");
    printf ("                        2             - Device Identity\n\r");
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
    bool flag_value = false;
    bool flag_execute = false;

    char *fn = NULL;
    char *cmd = argv[1];
    uint32_t cmd_value = 0;

    while ((c = getopt (argc-1, &argv[1], "hiwrxsln:f:v:")) != -1) {
        switch (c) {
            case 'w':
                flag_write = true;
            break;
            case 'x':
                flag_execute = true;
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
            case 'v':
                flag_value = true;
                cmd_value = atoi(optarg);
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

    if (strcmp(cmd, "dev") == 0) {
         if (flag_list) {
            pb_display_device_info(h);
        }
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
        if (flag_execute)
        {
            pb_execute_image(h, fn);
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
        if (flag_write && flag_value)
        {
            pb_set_config_value(h, cmd_index, cmd_value);
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
