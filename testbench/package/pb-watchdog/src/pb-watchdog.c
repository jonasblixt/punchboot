/**
 * Punch BOOT test bench
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * BusyBox's watchdog daemon, plus a pretimeout: the kernel's panic
 * pretimeout governor then panics Linux that long before the watchdog
 * resets the board, so a hang leaves a ramoops record. The pretimeout can
 * only be set with WDIOC_SETPRETIMEOUT, BusyBox's daemon doesn't.
 *
 * pb-watchdog [-t interval] [-p pretimeout] [-F] DEVICE
 */

#include <fcntl.h>
#include <linux/watchdog.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int fd = -1;

/* Like BusyBox: stop the watchdog on the way out (shutdown, reboot) */
static void stop(int sig)
{
    int options = WDIOS_DISABLECARD;

    (void)sig;
    (void)write(fd, "V", 1);
    (void)ioctl(fd, WDIOC_SETOPTIONS, &options);
    close(fd);
    _exit(0);
}

int main(int argc, char **argv)
{
    unsigned int interval = 5;
    int pretimeout = 2;
    int foreground = 0;
    int opt;

    while ((opt = getopt(argc, argv, "t:p:F")) != -1) {
        switch (opt) {
        case 't':
            interval = (unsigned int)strtoul(optarg, NULL, 0);
            break;
        case 'p':
            pretimeout = (int)strtol(optarg, NULL, 0);
            break;
        case 'F':
            foreground = 1;
            break;
        default:
            fprintf(stderr, "Usage: %s [-t interval] [-p pretimeout] [-F] DEVICE\n", argv[0]);
            return 1;
        }
    }

    if (optind != argc - 1 || interval == 0) {
        fprintf(stderr, "Usage: %s [-t interval] [-p pretimeout] [-F] DEVICE\n", argv[0]);
        return 1;
    }

    fd = open(argv[optind], O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        perror(argv[optind]);
        return 1;
    }

    if (ioctl(fd, WDIOC_SETPRETIMEOUT, &pretimeout) != 0)
        perror("WDIOC_SETPRETIMEOUT");

    if (!foreground && daemon(0, 0) != 0) {
        perror("daemon");
        return 1;
    }

    signal(SIGTERM, stop);
    signal(SIGINT, stop);

    while (1) {
        (void)ioctl(fd, WDIOC_KEEPALIVE, 0);
        sleep(interval);
    }
}
