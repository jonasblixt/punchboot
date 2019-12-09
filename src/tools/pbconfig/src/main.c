/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <uuid.h>
#include <strings.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>
#include <blkid.h>

#include "pbconfig.h"


static void print_help(void)
{
    printf(" --- pbconfig " VERSION " ---\n\n");
    printf(" Optional parameters:\n");
    printf("  pbconfig -d <device> -o primary offset -b backup offset");
    printf(" Usage:\n");
    printf("  pbconfig -i                                                  - " \
                                                        "View configuration\n");
    printf("  pbconfig -s <A, B or none>  [ -c <counter> ]                 - " \
                                                        "Switch system\n");
    printf("  pbconfig -v <A or B>                                         - " \
                                                        "Set verified flag\n");
}

int main(int argc, char * const argv[])
{
    extern char *optarg;
    uint32_t err;
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

    while (((c = getopt (argc, argv, "hd:u:s:ic:v:o:b:")) != -1) && (c != 255))
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
            case 'o':
                flag_primary_offset = true;
                offset_primary = strtol(optarg, NULL, 0);
            break;
            case 'c':
                counter = strtol(optarg, NULL, 0);
            break;
            case 'b':
                flag_backup_offset  = true;
                offset_backup = strtol(optarg, NULL, 0);
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

    /* Find config partitions using UUID's */
    if (!(flag_primary_offset && flag_backup_offset))
    {
        blkid_probe pr;
        pr = blkid_new_probe_from_filename(device_path);

        if (!pr)
        {
            printf("Error: could not probe '%s'\n", device_path);
            return -1;
        }

        blkid_do_probe(pr);
        blkid_partlist pl = blkid_probe_get_partitions(pr);
        int no_of_parts = blkid_partlist_numof_partitions(pl);

        for (uint32_t n = 0; n < no_of_parts; n++)
        {
            blkid_partition p = blkid_partlist_get_partition(pl, n);
            uuid_t uuid_raw;
            uuid_parse(blkid_partition_get_uuid(p), uuid_raw);

            if (memcmp(uuid_raw, PB_PARTUUID_CONFIG_PRIMARY, 16) == 0)
            {
                offset_primary = blkid_partition_get_start(p);
                printf("Found primary configuration at lba %lx\n",
                                                        offset_primary);
            }

            if (memcmp(uuid_raw, PB_PARTUUID_CONFIG_BACKUP, 16) == 0)
            {
                offset_backup = blkid_partition_get_start(p);
                printf("Found backup configuration at lba %lx\n",
                                                        offset_backup);
            }
        }

        blkid_free_probe(pr);
    }

    if (flag_device)
    {
        err = pbconfig_load(device_path, offset_primary, offset_backup);

        if (err != PB_OK)
            return -1;
    }
    else
    {
        /* UUID */
    }


    if (flag_show)
    {
        print_configuration();
    }
    else if (flag_switch)
    {
        if (strncasecmp(system, "a", 1) == 0)
            err = pbconfig_switch(SYSTEM_A, counter);
        else if (strncasecmp(system, "b", 1) == 0)
            err = pbconfig_switch(SYSTEM_B, counter);
        else
            err = pbconfig_switch(SYSTEM_NONE, 0);
    }
    else if (flag_verify)
    {
        if (strncasecmp(system_verified, "a", 1) == 0)
            err = pbconfig_set_verified(SYSTEM_A);
        else if (strncasecmp(system_verified, "b", 1) == 0)
            err = pbconfig_set_verified(SYSTEM_B);
        else
            err = PB_ERR;
    }

    if (err != PB_OK)
        printf("Error: operation failed\n");

    return err;
}
