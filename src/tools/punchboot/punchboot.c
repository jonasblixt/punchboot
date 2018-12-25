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


#include "crc.h"
#include "utils.h"
#include "transport.h"
#include "recovery_protocol.h"


static void pb_display_device_info(void)
{
    printf ("Device info:\n");
    //printf (" Hardware: %s, Revision: %s\n");
    //printf (" UUID: %s\n");
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
    char c;

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

    char *fn = NULL;
    char *cmd = argv[1];
    uint32_t cmd_value = 0;

    transport_init();

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

    if (flag_help) 
    {
        print_help();
        exit(0);
    }

    if (strcmp(cmd, "dev") == 0) {
         if (flag_list) {
            pb_display_device_info();
        }
    }

    if (strcmp(cmd, "boot") == 0) {
         if (flag_boot) {
            pb_boot_part(1);
        }
      
        if (flag_list) {
            pb_print_version();
        }

        if (flag_write) {
            pb_program_bootloader(fn);
           
        }
        if (flag_execute)
        {
            pb_execute_image(fn);
        }

        if (flag_reset) 
            pb_reset();
    }

    if (strcmp(cmd, "part") == 0) {
        if (flag_list) 
            pb_print_gpt_table();
        else if (flag_write && flag_index && fn) {
            printf ("Writing %s to part %i\n",fn, cmd_index);
            pb_flash_part(cmd_index, fn);
        } else if (flag_install) {
            pb_install_default_gpt();
        } else {
            printf ("Nope, that did not work\n");
        }

        if (flag_reset) 
            pb_reset();
 
    }

    if (strcmp(cmd, "config") == 0) {
        if (flag_list) {
            pb_get_config_tbl();
        }
        if (flag_write && flag_value)
        {
            pb_set_config_value(cmd_index, cmd_value);
        }
    }

    if (strcmp(cmd, "fuse") == 0) {

        if (flag_write && cmd_index > 0) {
            switch (cmd_index) {
                case 1:
                    printf ("Installing boot fuses...\n");
                    pb_write_default_fuse();
                break;
                case 2:
                    printf ("Installing unique ID\n");
                    pb_write_uuid();
                break;
            }
        }
    }

    transport_exit();
	return 0;
}
