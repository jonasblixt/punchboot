/**
 * \file plat.h
 *
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * Initialize the platform
 *
 * @return PB_OK on success or a negative number
 */
int plat_init(void);

/**
 * Initialize the board
 *
 * @return PB_OK on success or a negative number
 */
int plat_board_init(void);

/**
 * Platform specific reset function
 */
void plat_reset(void);

/**
 * Read platform/system tick
 *
 * @return current micro second tick
 */
unsigned int plat_get_us_tick(void);

/**
 * Kick watchdog
 */
void plat_wdog_kick(void);

/**
 * Returns a platform specific boot reason as an integer.
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
 * Platform / SoC Unique data. This function is called by the
 * device_uuid module to generate a device unique identifer
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
