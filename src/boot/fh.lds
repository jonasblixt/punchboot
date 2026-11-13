/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The Firmware Handoff region (boot.md 5.4, platform.md 5.1): the board's,
 * BOOT_FH_REGION_BASE and BOOT_FH_REGION_SIZE from its defconfig. pb writes
 * the transfer list there before the jump, so neither pb's region nor its
 * buffers' may be in it: the regions as the platform's MEMORY has them,
 * with the room pb grows in. That the region is in memory that works after
 * plat_init() is the platform's to check, in its link.lds.
 */

ASSERT(BOOT_FH_REGION_BASE + BOOT_FH_REGION_SIZE <= ORIGIN(ram) ||
       BOOT_FH_REGION_BASE >= ORIGIN(ram) + LENGTH(ram),
       "The Firmware Handoff region (CONFIG_BOOT_FH_REGION_BASE, _SIZE) overlaps pb")

ASSERT(BOOT_FH_REGION_BASE + BOOT_FH_REGION_SIZE <= ORIGIN(buffers) ||
       BOOT_FH_REGION_BASE >= ORIGIN(buffers) + LENGTH(buffers),
       "The Firmware Handoff region (CONFIG_BOOT_FH_REGION_BASE, _SIZE) overlaps pb's buffers")
