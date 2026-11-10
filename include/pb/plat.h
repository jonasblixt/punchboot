/**
 * \file plat.h
 *
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The platform contract: doc/dev/platform.md (D-030). What each function
 * must do, from when it may be called and what a failure means is there;
 * the comments here are the short form.
 */

#ifndef INCLUDE_PB_PLAT_H_
#define INCLUDE_PB_PLAT_H_

#include <pb-tools/wire.h>
#include <pb/pb.h>
#include <uuid.h>

/** Where the boot ROM (or a debugger) loaded pb from */
enum pb_boot_source_kind {
    PB_BOOT_SOURCE_UNKNOWN = 0, /*!< The platform can't tell */
    PB_BOOT_SOURCE_STORAGE, /*!< A bio partition, see part_uu */
    PB_BOOT_SOURCE_USB, /*!< ROM serial download over USB (uuu, blhost, ...) */
    PB_BOOT_SOURCE_UART, /*!< ROM serial download over UART */
    PB_BOOT_SOURCE_DEBUGGER, /*!< Loaded over JTAG/SWD */
};

struct pb_boot_source {
    enum pb_boot_source_kind kind;
    uuid_t part_uu; /*!< STORAGE: the bio partition (boot slot) pb was loaded from */
};

/**
 * The first platform setup, called once by main() after arch_init(), before
 * board.console_init(). Afterwards the tick counts and log_time_start() has
 * been called, the watchdog is armed (CONFIG_ENABLE_WATCHDOG), timers the
 * SoC arms out of reset are disabled, the ROM's state that later code
 * overwrites is saved and the reset reason is read (not cleared). No DRAM the
 * ROM didn't set up, no MMU. Short: it is on every boot.
 *
 * @return PB_OK, or a negative number for what can't be recovered from:
 *         main() resets
 */
int plat_early_init(void);

/**
 * The rest of the platform, called once by main() after
 * board.console_init(): the core clocks, fuse access, DRAM, the TrustZone
 * DRAM filter, then the MMU, the SoC's crypto engine.
 *
 * @return PB_OK, also when something command mode doesn't need failed: it
 *         is recorded for the boot's preconditions, which refuse a boot
 *         whose security depends on it (no silent boot); a negative number
 *         when command mode's prerequisites (DRAM, MMU, its clocks, fuses)
 *         are missing: main() resets
 */
int plat_init(void);

/**
 * Reset the SoC. Works from the first instruction after arch_init(), also
 * before plat_early_init() and without CONFIG_ENABLE_WATCHDOG. Does not
 * return.
 */
void plat_reset(void);

/**
 * A free-running microsecond counter from an arbitrary start. It wraps
 * modulo 2^32 (71.6 minutes): use differences (now - start) only. Valid from
 * plat_early_init(); the platform header states its accuracy.
 *
 * @return The tick in microseconds
 */
unsigned int plat_get_us_tick(void);

/**
 * Restart the watchdog. Cheap and safe at any time, also before it is armed
 * and without CONFIG_ENABLE_WATCHDOG. The core calls it where work makes
 * progress (a chunk loaded, before the jump, command mode's loop), never
 * from pb_delay_*() or pb_timeout_*().
 */
void plat_wdog_kick(void);

/**
 * The SoC's reset reason as an integer. To be replaced by
 * plat_get_boot_reason() (doc/dev/platform.md, F-236).
 *
 * @return >= 0 for valid boot reasons or a negative number on error
 */
int plat_boot_reason(void);

/**
 * Returns a platform specific boot reason as an string
 *
 * @return Boot reason string or ""
 */
const char *plat_boot_reason_str(void);

/**
 * The SoC's unique id, at most 16 bytes, the same for the life of the
 * device. Valid from plat_init(). device_uuid() derives the device UUID from
 * it and platform_ns_uuid: its bytes, their order and number must never
 * change (doc/dev/platform.md, the device identity).
 *
 * @param[out] output Unique data output
 * @param[inout] length Size of output buffer and result length
 *
 * @return PB_OK on success or a negative number
 */
int plat_get_unique_id(uint8_t *output, size_t *length);

/**
 * Where pb was loaded from
 *
 * Every boot slot is a bio partition, whatever the medium: eMMC boot
 * partitions, regions of a NOR flash or offsets in internal flash. So any
 * number of slots (A/B, primary/backups/factory) is described by the
 * partition's UUID, and its name says what it is. Only the platform knows how
 * its ROM reports this; drivers only report facts about their device.
 *
 * @param[out] src Boot source. kind is PB_BOOT_SOURCE_UNKNOWN when the
 *                 platform can't tell this time.
 *
 * @return PB_OK on success, -PB_ERR_NOT_SUPPORTED if the platform doesn't
 *         implement it
 */
int plat_get_boot_source(struct pb_boot_source *src);

#endif // INCLUDE_PB_PLAT_H_
