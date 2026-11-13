/**
 * \file plat.h
 *
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The platform contract: doc/interfaces/platform.md (D-030). What each function
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

/** Why the SoC was reset, in a form that is the same on every platform */
enum pb_boot_reason_kind {
    PB_BOOT_REASON_UNKNOWN = 0, /*!< No cause the platform knows, or one without a kind */
    PB_BOOT_REASON_POR, /*!< Power on, brown-out */
    PB_BOOT_REASON_WDOG, /*!< Any watchdog, also a system controller's or the ROM's */
    PB_BOOT_REASON_SW, /*!< Software reset, e.g. the OS's reboot */
    PB_BOOT_REASON_DEBUG, /*!< JTAG/SWD */
    PB_BOOT_REASON_FAULT, /*!< Thermal, lockup, ECC, tamper, ... */
};

struct pb_boot_reason {
    enum pb_boot_reason_kind kind; /*!< Portable: for the core and the boards */
    uint32_t raw; /*!< The SoC's value, documented in the platform header */
    const char *name; /*!< For the banner and the log, never NULL */
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
void plat_reset(void) __attribute__((noreturn));

/**
 * A free-running microsecond counter from an arbitrary start. It wraps
 * modulo 2^32 (71.6 minutes): use differences (now - start) only. Valid from
 * plat_early_init(); the platform header states its accuracy.
 *
 * @return The tick in microseconds
 */
uint32_t plat_get_us_tick(void);

/**
 * Restart the watchdog. Cheap and safe at any time, also before it is armed
 * and without CONFIG_ENABLE_WATCHDOG. The core calls it where work makes
 * progress (a chunk loaded, before the jump, command mode's loop), never
 * from pb_delay_*() or pb_timeout_*().
 */
void plat_wdog_kick(void);

/**
 * Why the SoC was reset, as plat_early_init() read it: valid from there. The
 * platform doesn't clear the SoC's status, the OS reads the same registers.
 * Where the status is sticky the kind is the highest priority cause since it
 * was last cleared, not the cause of this reset; the platform header has the
 * raw values, the priority and which causes survive a reset
 * (doc/interfaces/platform.md 3.4). The raw value is not on the wire.
 *
 * @param[out] reason The kind, the SoC's value and a name. Filled in also
 *                    when the call fails: PB_BOOT_REASON_UNKNOWN and a name
 *
 * @return PB_OK, or a negative number when the platform could not read it
 */
int plat_get_boot_reason(struct pb_boot_reason *reason);

/**
 * The UUID namespace of the platform, 16 bytes: a constant. device_uuid()
 * derives the device UUID from it and the SoC's unique id, the host sees
 * that UUID and command mode's tokens are signed over it. It must never
 * change: it is fixed like the wire (doc/interfaces/platform.md 3.6, 3.7).
 * A new platform generates its own once, at random.
 */
extern const char *platform_ns_uuid;

/**
 * The SoC's unique id, at most 16 bytes, the same for the life of the
 * device. Valid from plat_init(). device_uuid() derives the device UUID from
 * it and platform_ns_uuid: its bytes, their order and number must never
 * change (doc/interfaces/platform.md, the device identity).
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

/**
 * Set up the USB device port command mode uses: its clocks, its power, its
 * PHY and its controller, so that the transport can start. Called once, when
 * command mode starts, never on the boot path (D-023). Which of the SoC's
 * ports is the platform's Kconfig choice, made in the board's defconfig. It
 * relies on what plat_init() set up and on nothing of the board's: command
 * mode also runs after a failed board_setup(). Mandatory with CONFIG_CM
 * (doc/interfaces/platform.md 3.10).
 *
 * @return PB_OK, or a negative number: no command mode, and pb resets
 */
int plat_cm_init(void);

/**
 * Lock, until the next reset, what only pb may change: key revocation, the
 * fuses that reopen a closed device, the controls the SoC can lock for the
 * boot's lifetime. Called by the core right before the jump, after the last
 * moment command mode could use them. A platform with nothing to lock
 * returns PB_OK (doc/interfaces/platform.md 4.4).
 *
 * @return PB_OK, or a negative number: the boot doesn't jump
 */
int plat_lockdown(void);

#endif // INCLUDE_PB_PLAT_H_
