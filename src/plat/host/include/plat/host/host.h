/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Host platform: runs punchboot as a native Linux process for testing.
 *
 *  - The board RAM is emulated by a fixed mapping at BOARD_RAM_BASE, so bpak
 *    load addresses work unmodified.
 *  - Block devices are backed by files.
 *  - The command mode transport is a unix domain socket.
 *  - A reset terminates the process.
 */

#ifndef PLAT_HOST_HOST_H
#define PLAT_HOST_HOST_H

#include <pb/bio.h>
#include <stdbool.h>
#include <stddef.h>
#include <uuid.h>

struct host_options {
    const char *disk_path; /* Backing file for the main disk */
    const char *socket_path; /* Command mode socket */
    const char *boot_status_path; /* Written by the board on 'boot' */
    bool force_command_mode; /* Abort boot and enter command mode */
};

extern struct host_options host_opts;

/**
 * Create a block device backed by a file. The file is created if it does
 * not exist, and is extended to 'size' bytes if it is smaller.
 */
bio_dev_t host_disk_init(const char *path, size_t size, const uuid_t uu, const char *description);

/* Command mode transport over a unix domain socket */
int host_socket_init(void);
int host_socket_read(void *buf, size_t length);
int host_socket_write(const void *buf, size_t length);
int host_socket_complete(void);

#endif // PLAT_HOST_HOST_H
