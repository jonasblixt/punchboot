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
 *  - The fuses are a file of their own (--fuses), a block device that the
 *    test fuse driver reads in plat_init(): readable whatever the disk and
 *    its partition table are, as a SoC's fuses are (platform.md 2.2).
 *  - With --nor, a file is also mapped read only at BOARD_NOR_BASE: a memory
 *    mapped flash, for partitions that are booted in place.
 *  - The command mode transport is a unix domain socket (socket.c has the
 *    cm_transport_*() functions of pb/cm.h).
 *  - A reset terminates the process.
 *  - With CONFIG_ENABLE_WATCHDOG (configs/host_speculative_defconfig) the
 *    watchdog is a fake that plat_early_init() arms and that counts the
 *    kicks, for the tests to see (--watchdog). It resets nothing.
 *  - The boot reason (plat_get_boot_reason()) is always a power on,
 *    PB_BOOT_REASON_POR with a raw value of 0: a process has no reset status.
 *  - The platform's status (plat_get_status()) has everything as absent: no
 *    TrustZone, no watchdog, no crypto engine of the platform's, no ROM.
 *    With --dram-filter-bypassed it reports a TrustZone DRAM filter that is
 *    bypassed, for the tests of the boot's preconditions.
 *  - Where pb was loaded from (plat_get_boot_source()) isn't supported: a
 *    process has no ROM. With --boot-source it is what the option names,
 *    for the tests of the serial download stop.
 */

#ifndef PLAT_HOST_HOST_H
#define PLAT_HOST_HOST_H

#include <pb/bio.h>
#include <pb/plat.h>
#include <stdbool.h>
#include <stddef.h>
#include <uuid.h>

struct host_options {
    const char *disk_path; /* Backing file for the main disk */
    const char *nor_path; /* Backing file for the memory mapped flash, or NULL */
    const char *fuse_path; /* Backing file for the fuses */
    const char *socket_path; /* Command mode socket */
    const char *boot_status_path; /* Written by the board on 'boot' */
    const char *dt_path; /* Written by the board on 'boot': the tree it prepared, or NULL */
    const char *watchdog_path; /* The watchdog's timeout and kicks, or NULL */
    bool force_command_mode; /* Abort boot and enter command mode */
    bool dram_filter_bypassed; /* plat_get_status(): PLAT_TZ_DRAM_BYPASSED */
    bool boot_source_set; /* plat_get_boot_source() answers boot_source */
    enum pb_boot_source_kind boot_source;
};

extern struct host_options host_opts;

/**
 * Create a block device backed by a file. The file is created if it does
 * not exist, and is extended to 'size' bytes if it is smaller. Two devices
 * at most, each with its own UUID: a second init with a UUID replaces the
 * first one's file, also when it fails.
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
