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
    uint32_t err = PB_ERR;
    char *version_string;

    err = pb_get_version(&version_string);

    if (err != PB_OK)
        return -1;
    printf ("Device info:\n");
    //printf (" Hardware: %s, Revision: %s\n");
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
    printf ("  punchboot boot -s A or B            - BOOT System A or B\n");
    printf ("  punchboot boot -x -f <fn>           - Load image to RAM and execute it\n");
    printf ("  punchboot boot -a -n <bootconf>     - Activate boot partition\n");
    printf ("\n");
}


static void print_dev_help(void)
{
    printf (" Device:\n");
    printf ("  punchboot dev -l                    - Display device information\n");
    printf ("  punchboot dev -s \"<rev> <var>\" [-y] - Perform device setup\n");
    printf ("  punchboot dev -w [-y]               - Lock device setup\n");
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

static void print_help(void) {
    print_help_header();
    print_boot_help();
    print_dev_help();
    print_part_help();
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
    bool flag_s = false;
    bool flag_index = false;
    bool flag_install = false;
    bool flag_execute = false;
    bool flag_a = false;
    bool flag_y = false;

    char *fn = NULL;
    char *cmd = argv[1];
    char *s_arg = NULL;

    if (transport_init() != 0)
        exit(-1);

    while ((c = getopt (argc-1, &argv[1], "hiwraybxs:ln:f:v:")) != -1) 
    {
        switch (c) 
        {
            case 'w':
                flag_write = true;
            break;
            case 'x':
                flag_execute = true;
            break;
            case 'a':
                flag_a = true;
            break;
            case 'y':
                flag_y = true;
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
                flag_s = true;
                s_arg = optarg;
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
        } else if (flag_s) {
            unsigned int device_version;
            unsigned int device_variant;
            char *setup_report = NULL;
            bool dry_run = !flag_y;
            int result = sscanf(s_arg, "%x %x", 
                        &device_version, &device_variant);

            if (result != 2)
                return -1;

            printf ("Performing device setup: Version=0x%2.2X, Variant=0x%2.2X\n",
                            device_version, device_variant);

            if (dry_run)
            {
                char confirm_input[5];

                if (!flag_y)
                {
                    printf ("\n\nWARNING: This is a permanent change, writing fuses " \
                            "can not be reverted. This could brick your device.\n"
                            "\n\nType 'yes' + <Enter> to proceed: ");
                    fgets(confirm_input, 5, stdin);

                    if (strncmp(confirm_input, "yes", 3)  != 0)
                    {
                        printf ("Aborted\n");
                        return -1;
                    }
                }

                err = pb_recovery_setup(device_version,
                                     device_variant,
                                     &setup_report,
                                     false);

                if (err != PB_OK)
                {
                    printf ("ERROR: Something went wrong\n");
                    return -1;
                }

                printf ("Success\n");
            }


        } else if (flag_write) {

            char confirm_input[5];
            printf ("\n\nWARNING: This is a permanent change, writing fuses " \
                    "can not be reverted. This could brick your device.\n"
                    "\n\nType 'yes' + <Enter> to proceed: ");
            fgets(confirm_input, 5, stdin);

            if (strncmp(confirm_input, "yes", 3)  != 0)
            {
                printf ("Aborted\n");
                return -1;
            }

            err = pb_recovery_setup_lock();

            if (err != PB_OK)
            {
                printf("ERROR: Something went wrong\n");
                return -1;
            }
            printf ("Success");
        } else {
            print_help_header();
            print_dev_help();
        }
    }

    if (strcmp(cmd, "boot") == 0) 
    {
        if ( !(flag_s | flag_a | flag_write | flag_execute | flag_reset ))
        {
            print_help_header();
            print_boot_help();
            return 0;
        }

        if (flag_a)
        {
            err = pb_set_bootpart(cmd_index);
        }

        if (flag_s) 
        {
            char *part = s_arg;

            if ((part[0] == 'A') || (part[0] == 'a') )
                err = pb_boot_part(1);
            else if ((part[0] == 'B') || (part[0] == 'b') )
                err = pb_boot_part(2);
            else
                err = PB_ERR;

            if (err != PB_OK)
                return -1;
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
            printf("Listing partitions:\n");
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

    transport_exit();
    if (err != PB_OK)
        return -1;

    return 0;
}
