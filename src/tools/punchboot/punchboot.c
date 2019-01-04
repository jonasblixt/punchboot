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
#include <string.h>
#include <pb/pb.h>
#include <pb/config.h>
#include <uuid/uuid.h>

#include "crc.h"
#include "utils.h"
#include "transport.h"
#include "recovery_protocol.h"

static int print_gpt_table(void)
{
    struct gpt_primary_tbl gpt;
    int err;
    char str_type_uuid[37];
    uint8_t tmp_string[64];
    struct gpt_part_hdr *part;

    err = pb_get_gpt_table(&gpt);

    if (err != 0)
        return err;

    if (gpt.hdr.no_of_parts == 0)
        return -1;

    printf ("GPT Table:\n");
    for (int i = 0; i < gpt.hdr.no_of_parts; i++) 
    {
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

static uint32_t pb_display_device_info(void)
{
    char str_device_uuid[37];
    uint8_t device_uuid_raw[16];
    uint32_t err = PB_ERR;
    char *version_string;
    err = pb_get_version(&version_string);

    if (err != PB_OK)
        return -1;

    err = pb_read_uuid(device_uuid_raw);
    
    if (err != PB_OK)
        return err;

    uuid_unparse_upper(device_uuid_raw, str_device_uuid);
    printf ("Device info:\n");
    //printf (" Hardware: %s, Revision: %s\n");
    printf (" UUID: %s\n",str_device_uuid);
    printf (" Security State:\n");
    printf (" Bootloader Version: %s\n",version_string);
    free(version_string);
    return PB_OK;
}

static void print_help_header(void)
{
    printf (" --- Punch BOOT " VERSION " ---\n\n");
}

static void print_boot_help(void)
{
    printf (" Bootloader:\n");
    printf ("  punchboot boot -w -f <fn>           - Install bootloader\n");
    printf ("  punchboot boot -r                   - Reset device\n");
    printf ("  punchboot boot -s -a or -b          - BOOT System A or B\n");
    printf ("  punchboot boot -l                   - Display version\n");
    printf ("  punchboot boot -x -f <fn>           - Load image to RAM and execute it\n");
    printf ("\n");
}


static void print_dev_help(void)
{
    printf (" Device:\n");
    printf ("  punchboot dev -l                    - Display device information\n");
    printf ("\n");
}

static void print_config_help(void)
{
    printf (" Configuration:\n");
    printf ("  punchboot config -l                 - Display configuration\n");
    printf ("  punchboot config -w -n <n> -v <val> - Write <val> to key <n>\n");
    printf ("\n");
}

static void print_part_help(void)
{

    printf (" Partition Management:\n");
    printf ("  punchboot part -l                   - List partitions\n");
    printf ("  punchboot part -w -n <n> -f <fn>    - Write 'fn' to partition 'n'\n");
    printf ("  punchboot part -i                   - Install default GPT table\n");
    printf ("\n");
}

static void print_fuse_help(void)
{
    printf (" Fuse Management (WARNING: these operations are OTP and can't be reverted):\n");
    printf ("  punchboot fuse -w -n <n>            - Install fuse set <n>\n");
    printf ("                        1             - Boot fuses\n\r");
    printf ("                        2             - Device Identity\n\r");
    printf ("\n");
}

static void print_help(void) {
    print_help_header();
    print_boot_help();
    print_dev_help();
    print_config_help();
    print_part_help();
    print_fuse_help();
}

int main(int argc, char **argv) 
{
    extern char *optarg;
    extern int optind, opterr, optopt;
    char c;
    int err;

    if (argc <= 1) {
        print_help();
        exit(0);
    }

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
    bool flag_a = false;
    bool flag_b = false;

    char *fn = NULL;
    char *cmd = argv[1];
    uint32_t cmd_value = 0;

    if (transport_init() != 0)
        exit(-1);

    while ((c = getopt (argc-1, &argv[1], "hiwrabxsln:f:v:")) != -1) {
        switch (c) {
            case 'w':
                flag_write = true;
            break;
            case 'x':
                flag_execute = true;
            break;
            case 'a':
                flag_a = true;
            break;
            case 'b':
                flag_b = true;
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

    if (flag_help) 
    {
        print_help();
        exit(0);
    }

    if (strcmp(cmd, "dev") == 0) 
    {
        if (flag_list) 
        {
            err = pb_display_device_info();

            if (err != PB_OK)
                return -1;
        } else {
            print_help_header();
            print_dev_help();
        }
    }

    if (strcmp(cmd, "boot") == 0) 
    {
        if ( !(flag_boot | flag_list | flag_write | flag_execute | flag_reset ))
        {
            print_help_header();
            print_boot_help();
            return 0;
        }

        if (flag_boot) 
        {
            if (flag_a)
                err = pb_boot_part(0xAA);
            else if (flag_b)
                err = pb_boot_part(0xBB);
            else
                err = PB_ERR;

            if (err != PB_OK)
                return -1;
        }
      
        if (flag_list) 
        {
            char *version_string;
            err = pb_get_version(&version_string);

            if (err != PB_OK)
                return -1;

            printf ("%s\n",version_string);
            free(version_string);
        }

        if (flag_write) 
        {
            err = pb_program_bootloader(fn);

            if (err != PB_OK)
                return -1;
        }

        if (flag_execute)
        {
            err = pb_execute_image(fn);

            if (err != PB_OK)
                return -1;
        }

        if (flag_reset) 
        {
            err = pb_reset();
            
            if (err != PB_OK)
                return -1;
        }
    }

    if (strcmp(cmd, "part") == 0) 
    {
        if (flag_list) 
        {
            err = print_gpt_table();

            if (err != PB_OK)
                return -1;

        } else if (flag_write && flag_index && fn) {
            printf ("Writing %s to part %i\n",fn, cmd_index);
            err = pb_flash_part(cmd_index, fn);

            if (err != PB_OK)
                return -1;

        } else if (flag_install) {
            err = pb_install_default_gpt();

            if (err != PB_OK)
                return -1;

        } else {
            print_help_header();
            print_part_help();
            return 0;
        }

        if (flag_reset) 
        {
            err = pb_reset();

            if (err != PB_OK)
                return -1;
        }
    }

    if (strcmp(cmd, "config") == 0) 
    {
        if (flag_list) 
        {
            const char *access_text[] = {"  ","RW","RO","OTP"};
            struct pb_config_item items[127];
            err = pb_get_config_tbl(items);

            if (err != PB_OK)
                return -1;

            int n = 0;
            printf (    " Index   Description        Access   Default      Value\n");
            printf (    " -----   -----------        ------  ----------  ----------\n\n");
            do {
                uint32_t value = 0;
                err = pb_get_config_value(n, &value);
                if (err != PB_OK)
                    return -1;

                printf (" %-3u     %-16s   %-3s     0x%8.8X  0x%8.8X\n",
                                             items[n].index,
                                             items[n].description,
                                             access_text[items[n].access],
                                             items[n].default_value,
                                             value);
                n++;
            } while (items[n].index != -1);
        } else if (flag_write && flag_value) {
            err = pb_set_config_value(cmd_index, cmd_value);
            if (err != 0)
                return -1;
        } else {
            print_help_header();
            print_config_help();
        }
    }

    if (strcmp(cmd, "fuse") == 0) 
    {

        if (flag_write && cmd_index > 0) 
        {
            switch (cmd_index) 
            {
                case 1:
                {
                    printf ("Installing boot fuses...\n");
                    err = pb_write_default_fuse();
                    
                    if (err != PB_OK)
                        printf ("Error: Could not install boot fuses\n");
                }
                break;
                case 2:
                {
                    printf ("Installing unique ID\n");
                    err = pb_write_uuid();

                    if (err != PB_OK)
                        printf ("Error: Could not install UUID fuses\n");
                }
                break;
                default:
                {
                    printf ("Error: Invalid fuse set\n");
                    err = PB_ERR;
                }
            }

        } else {
            print_help_header();
            print_fuse_help();
        }
    }

    transport_exit();
    if (err != PB_OK)
        return -1;

    return 0;
}
