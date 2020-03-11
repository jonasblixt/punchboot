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
    const char *transport = NULL;

    struct option long_options[] =
    {
        {"help",      no_argument,       0,  'h' },
        {"version",   no_argument,       0,  'V' },
        {"verbose",   no_argument,       0,  'v' },
        {"transport", required_argument, 0,  't' },
        {0,           0,                 0,   0  }
    };

    if (argc < 2)
    {
        help_main();
        return 0;
    }

    srand(time(NULL));

    while ((opt = getopt_long(2, argv, "hVvt:",
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
            case 't':
                transport = optarg;
            break;
            case ':':
                printf("Missing arg for %c\n", optopt);
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
        else
        {
            printf ("Unknown action '%s'\n", action);
            return -1;
        }
    }
    else
    {
        printf("Unknown action\n");
        return -1;
    }

    return 0;
}
