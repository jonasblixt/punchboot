/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <strings.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>

#include "pbstate.h"


static void print_help(void)
{
    printf(" --- pbstate " VERSION " ---\n\n");
    printf(" Optional parameters:\n");
    printf("  pbstate -d <device> -o primary offset -b backup offset");
    printf(" Usage:\n");
    printf("  pbstate -i                                                  - " \
                                                        "View configuration\n");
    printf("  pbstate -s <A, B or none>  [ -c <counter> ]                 - " \
                                                        "Switch system\n");
    printf("  pbstate -v <A or B>                                         - " \
                                                        "Set verified flag\n");
}

static void print_configuration(void)
{
    printf("Punchboot status:\n\n");
    printf("System A is %s and %s\n", pbstate_is_system_active(PBSTATE_SYSTEM_A)?
                                    "enabled":"disabled",
                                    pbstate_is_system_verified(PBSTATE_SYSTEM_A)?
                                    "verified":"not verified");

    printf("System B is %s and %s\n", pbstate_is_system_active(PBSTATE_SYSTEM_B)?
                                    "enabled":"disabled",
                                    pbstate_is_system_verified(PBSTATE_SYSTEM_B)?
                                    "verified":"not verified");


    printf("Errors : 0x%08x\n", pbstate_get_errors());
    printf("Remaining boot attempts: %u\n", pbstate_get_boot_attempts());
}

int main(int argc, char * const argv[])
{
    extern char *optarg;
    int err;
    char c;
    char *device_path = NULL;
    char *system = NULL;
    char *system_verified = NULL;
    bool flag_help = false;
    bool flag_switch = false;
    bool flag_show = false;
    bool flag_verify = false;
    bool flag_device = false;
    bool flag_primary_offset = false;
    bool flag_backup_offset = false;
    uint64_t offset_primary = 0;
    uint64_t offset_backup = 0;
    uint8_t counter = 0;

    if (argc < 2)
        flag_help = true;

    while (((c = getopt (argc, argv, "hd:u:s:ic:v:")) != -1) && (c != 255))
    {
        switch (c)
        {
            case 'h':
                flag_help = true;
            break;
            case 'd':
                flag_device = true;
                device_path = optarg;
            break;
            case 'c':
                counter = strtol(optarg, NULL, 0);
            break;
            case 'i':
                flag_show = true;
            break;
            case 's':
                system = optarg;
                flag_switch = true;
            break;
            case 'v':
                system_verified = optarg;
                flag_verify = true;
            break;
            default:
                printf("Unknown option\n");
                exit(-1);
            break;
        }
    }

    if (flag_help)
    {
        print_help();
        exit(0);
    }

    if (flag_device)
    {
        err = pbstate_load(device_path, printf);

        if (err != 0)
            return -1;
    }
    else
    {
        printf("Error: No device specified\n");
        return -1;
    }


    if (flag_show)
    {
        print_configuration();
    }
    else if (flag_switch)
    {
        if (strncasecmp(system, "a", 1) == 0)
            err = pbstate_switch_system(PBSTATE_SYSTEM_A, counter);
        else if (strncasecmp(system, "b", 1) == 0)
            err = pbstate_switch_system(PBSTATE_SYSTEM_B, counter);
        else
            err = pbstate_switch_system(PBSTATE_SYSTEM_NONE, 0);
    }
    else if (flag_verify)
    {
        if (strncasecmp(system_verified, "a", 1) == 0)
            err = pbstate_set_system_verified(PBSTATE_SYSTEM_A);
        else if (strncasecmp(system_verified, "b", 1) == 0)
            err = pbstate_set_system_verified(PBSTATE_SYSTEM_B);
        else
            err = -1;
    }

    if (err != 0)
        printf("Error: operation failed\n");

    return err;
}
