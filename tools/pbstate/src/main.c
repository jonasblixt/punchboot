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

static void print_version(void)
{
    printf("pbstate v%s\n", PROJECT_VERSION);
}

static void print_help(void)
{
    print_version();
    printf("\n");
    printf("Usage: pbstate [-p <primary device> -b <backup device>] Command\n");
    printf("\n");
    printf("Optional parameters:\n");
    printf("    -p, --primary <Path>       Primary state partition\n");
    printf("    -b, --backup  <Path>       Backup state partition\n");
    printf("\n");
    printf("Commands:\n");
    printf("    -s, --switch <System ID>   Switch active system\n");
    printf("    -v, --verified <System ID> Set system to verified state\n");
    printf("    -i, --info                 Print current state\n");
    printf("    -h, --help                 Display this help\n");
    printf("    -V, --version              Display version\n");
    printf("\n");
    printf("Optional parameter for switch command:\n");
    printf("    -c, --count <n>            Set the system to not verified and boot retry counter to <n>\n");
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
    int opt;
    int long_index = 0;
    int err;
    const char *primary_device_path = NULL;
    const char *backup_device_path = NULL;
    const char *system = NULL;
    const char *system_verified = NULL;
    bool flag_switch = false;
    bool flag_show = false;
    bool flag_verify = false;
    uint8_t counter = 0;

    struct option long_options[] =
    {
        {"primary",   required_argument,   0,  'p' },
        {"backup",    required_argument,   0,  'b' },
        {"switch",    required_argument,   0,  's' },
        {"count",     required_argument,   0,  'c' },
        {"verified",  required_argument,   0,  'v' },
        {"info",      no_argument,         0,  'i' },
        {"help",      no_argument,         0,  'h' },
        {"version",   no_argument,         0,  'V' },
        {0,           0,                   0,   0  }
    };

    if (argc < 2) {
        print_help();
        return 0;
    }

#if defined(PRIMARY_PART) && defined(BACKUP_PART)
    primary_device_path = PRIMARY_PART;
    backup_device_path = BACKUP_PART;
#endif

    while ((opt = getopt_long(argc, argv, "p:b:s:c:v:ihVw:",
                   long_options, &long_index )) != -1) {
        switch (opt) {
            case 'h':
                print_help();
                return 0;
            break;
            case 'V':
                print_version();
                return 0;
            break;
            case 'p':
                primary_device_path = optarg;
            break;
            case 'b':
                backup_device_path = optarg;
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
            case ':':
                fprintf(stderr, "Missing arg for %c\n", optopt);
                return -1;
            break;
             default:
                print_help();
                exit(EXIT_FAILURE);
        }
    }

#if !defined(PRIMARY_PART) || !defined(BACKUP_PART)
    if ((primary_device_path == NULL) || (backup_device_path == NULL)) {
        fprintf(stderr, "Error: Missing required -p and -b parameters\n");
        exit(EXIT_FAILURE);
    }
#endif

    err = pbstate_load(primary_device_path, backup_device_path, printf);

    if (err != 0)
        return -1;

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
