/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Process entry point for the host platform. Parses the command line and
 * then runs the regular punchboot main, which is renamed to pb_main for
 * host builds.
 */

#include <getopt.h>
#include <plat/host/host.h>
#include <stdio.h>
#include <stdlib.h>

void pb_main(void);

struct host_options host_opts = {
    .disk_path = "disk.img",
    .socket_path = "pb.sock",
    .boot_status_path = "boot_status",
    .force_command_mode = false,
};

static void usage(const char *name)
{
    printf("Usage: %s [options]\n"
           "  -d, --disk PATH          Disk image (default: disk.img)\n"
           "  -s, --socket PATH        Command mode socket (default: pb.sock)\n"
           "  -b, --boot-status PATH   Boot status output (default: boot_status)\n"
           "  -c, --command-mode       Force command mode\n"
           "  -h, --help               Show this help\n",
           name);
}

int main(int argc, char **argv)
{
    static const struct option long_options[] = {
        { "disk", required_argument, NULL, 'd' },
        { "socket", required_argument, NULL, 's' },
        { "boot-status", required_argument, NULL, 'b' },
        { "command-mode", no_argument, NULL, 'c' },
        { "help", no_argument, NULL, 'h' },
        { NULL, 0, NULL, 0 },
    };
    int opt;

    while ((opt = getopt_long(argc, argv, "d:s:b:ch", long_options, NULL)) != -1) {
        switch (opt) {
        case 'd':
            host_opts.disk_path = optarg;
            break;
        case 's':
            host_opts.socket_path = optarg;
            break;
        case 'b':
            host_opts.boot_status_path = optarg;
            break;
        case 'c':
            host_opts.force_command_mode = true;
            break;
        case 'h':
            usage(argv[0]);
            return EXIT_SUCCESS;
        default:
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    pb_main();

    /* pb_main ends with plat_reset(), which exits */
    return EXIT_FAILURE;
}
