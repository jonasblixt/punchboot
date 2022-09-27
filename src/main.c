/**
 * Punch BOOT bootloader cli
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>

#include "tool.h"

int main(int argc, char **argv)
{
    int opt;
    int long_index = 0;
    const char *action = NULL;

    struct option long_options[] =
    {
        {"help",      no_argument,       0,  'h' },
        {"version",   no_argument,       0,  'V' },
        {"verbose",   no_argument,       0,  'v' },
        {0,           0,                 0,   0  }
    };

    if (argc < 2)
    {
        help_main();
        return 0;
    }

    srand(time(NULL));

    while ((opt = getopt_long(2, argv, "hVv",
                   long_options, &long_index )) != -1)
    {
        switch (opt)
        {
            case 'h':
                help_main();
                return 0;
            break;
            case 'V':
                print_version();
                return 0;
            break;
            case 'v':
                pb_inc_verbosity();
            break;
            case ':':
                fprintf(stderr, "Missing arg for %c\n", optopt);
                return -1;
            break;
             default:
                help_main();
                exit(EXIT_FAILURE);
        }
    }


    if (optind < argc)
    {
        action = (const char *) argv[optind++];
        optind = 1;
        argv++;
        argc--;

        /* Check for valid action */
        if (strcmp(action, "dev") == 0)
        {
            return action_dev(argc, argv);
        }
        if (strcmp(action, "part") == 0)
        {
            return action_part(argc, argv);
        }
        if (strcmp(action, "boot") == 0)
        {
            return action_boot(argc, argv);
        }
        if (strcmp(action, "slc") == 0)
        {
            return action_slc(argc, argv);
        }
        if (strcmp(action, "auth") == 0)
        {
            return action_auth(argc, argv);
        }
        if (strcmp(action, "board") == 0)
        {
            return action_board(argc, argv);
        }
        else
        {
            fprintf (stderr, "Unknown action '%s'\n", action);
            return -1;
        }
    }
    else
    {
        fprintf(stderr, "Unknown action\n");
        return -1;
    }

    return 0;
}
