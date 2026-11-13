/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
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
#include <string.h>

void pb_main(void);

/* Options without a short form */
enum {
    OPT_DRAM_FILTER_BYPASSED = 0x100,
    OPT_BOOT_SOURCE,
};

/* --boot-source: what the ROM would have loaded pb from */
static int parse_boot_source(const char *name)
{
    static const struct {
        const char *name;
        enum pb_boot_source_kind kind;
    } kinds[] = {
        { "unknown", PB_BOOT_SOURCE_UNKNOWN },   { "storage", PB_BOOT_SOURCE_STORAGE },
        { "usb", PB_BOOT_SOURCE_USB },           { "uart", PB_BOOT_SOURCE_UART },
        { "debugger", PB_BOOT_SOURCE_DEBUGGER },
    };

    for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
        if (strcmp(name, kinds[i].name) == 0) {
            host_opts.boot_source = kinds[i].kind;
            host_opts.boot_source_set = true;
            return 0;
        }
    }

    return -1;
}

struct host_options host_opts = {
    .disk_path = "disk.img",
    .fuse_path = "fuses.img",
    .socket_path = "pb.sock",
    .boot_status_path = "boot_status",
    .force_command_mode = false,
};

static void usage(const char *name)
{
    printf("Usage: %s [options]\n"
           "  -d, --disk PATH          Disk image (default: disk.img)\n"
           "  -n, --nor PATH           Memory mapped flash image (default: none)\n"
           "  -f, --fuses PATH         Fuse file (default: fuses.img)\n"
           "  -s, --socket PATH        Command mode socket (default: pb.sock)\n"
           "  -b, --boot-status PATH   Boot status output (default: boot_status)\n"
           "  -c, --command-mode       Force command mode\n"
           "      --dram-filter-bypassed  The platform reports a bypassed TrustZone\n"
           "                           DRAM filter: no boot\n"
           "      --boot-source KIND   Where pb was loaded from: unknown, storage,\n"
           "                           usb, uart or debugger (default: not supported)\n"
           "  -h, --help               Show this help\n",
           name);
}

int main(int argc, char **argv)
{
    static const struct option long_options[] = {
        { "disk", required_argument, NULL, 'd' },
        { "nor", required_argument, NULL, 'n' },
        { "fuses", required_argument, NULL, 'f' },
        { "socket", required_argument, NULL, 's' },
        { "boot-status", required_argument, NULL, 'b' },
        { "command-mode", no_argument, NULL, 'c' },
        { "dram-filter-bypassed", no_argument, NULL, OPT_DRAM_FILTER_BYPASSED },
        { "boot-source", required_argument, NULL, OPT_BOOT_SOURCE },
        { "help", no_argument, NULL, 'h' },
        { NULL, 0, NULL, 0 },
    };
    int opt;

    while ((opt = getopt_long(argc, argv, "d:n:f:s:b:ch", long_options, NULL)) != -1) {
        switch (opt) {
        case 'd':
            host_opts.disk_path = optarg;
            break;
        case 'f':
            host_opts.fuse_path = optarg;
            break;
        case 'n':
            host_opts.nor_path = optarg;
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
        case OPT_DRAM_FILTER_BYPASSED:
            host_opts.dram_filter_bypassed = true;
            break;
        case OPT_BOOT_SOURCE:
            if (parse_boot_source(optarg) != 0) {
                usage(argv[0]);
                return EXIT_FAILURE;
            }
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
