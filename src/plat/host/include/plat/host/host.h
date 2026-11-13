/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Host platform: runs punchboot as a native Linux process for testing.
 *
 *  - The board RAM is emulated by a fixed mapping at BOARD_RAM_BASE, so bpak
 *    load addresses work unmodified.
 *  - Block devices are backed by files.
 *  - With --nor, a file is also mapped read only at BOARD_NOR_BASE: a memory
 *    mapped flash, for partitions that are booted in place.
 *  - The command mode transport is a unix domain socket (socket.c has the
 *    cm_transport_*() functions of pb/cm.h).
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
    const char *nor_path; /* Backing file for the memory mapped flash, or NULL */
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

/**
 * Map the memory mapped flash: 'size' bytes of the file at 'base', read only.
 * The file is created if it does not exist, and is extended to 'size' bytes
 * with 0xff if it is smaller. Called by plat_early_init() with --nor.
 */
int host_nor_map(const char *path, uintptr_t base, size_t size);

/**
 * Create the block device of the memory mapped flash: it reads the map and
 * writes the file. -PB_ERR_NOT_FOUND without --nor.
 */
bio_dev_t host_nor_init(const uuid_t uu, const char *description);

#endif // PLAT_HOST_HOST_H
