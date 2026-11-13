/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * pb runs from the on-chip RAM, where the ROM copies it from the boot
 * device, and its buffers follow it there (platform.md 5.1): PB_ENTRY,
 * PB_BUFFERS_BASE and PB_BUFFERS_SIZE are the board's, from its defconfig.
 */

/* The ROM's load window: OCRAM2 above the ROM's own data, 32 KiB that its
 * API still uses, and the FlexRAM's OCRAM after it, 256 KiB with the
 * FlexRAM's default configuration, which pb doesn't change
 * (plat/imxrt/mm.h) */
IMXRT_LOAD_BASE = 0x20208000;
IMXRT_LOAD_END = 0x202c0000;

/* pb has the window from PB_ENTRY to the buffers, when they follow it
 * there, and else to the window's end */
MEMORY
{
    ram (rw)  : ORIGIN = PB_ENTRY,
                LENGTH = (PB_BUFFERS_BASE > PB_ENTRY && PB_BUFFERS_BASE < IMXRT_LOAD_END ?
                          PB_BUFFERS_BASE : IMXRT_LOAD_END) - PB_ENTRY
    buffers (rw)  : ORIGIN = PB_BUFFERS_BASE, LENGTH = PB_BUFFERS_SIZE
}

ASSERT(PB_ENTRY >= IMXRT_LOAD_BASE && PB_ENTRY < IMXRT_LOAD_END,
       "CONFIG_PB_ENTRY is not in the on-chip RAM the i.MX RT's ROM loads pb to")

ASSERT(ORIGIN(buffers) + LENGTH(buffers) <= ORIGIN(ram) ||
       ORIGIN(buffers) >= ORIGIN(ram) + LENGTH(ram),
       "pb's buffers (CONFIG_PB_BUFFERS_BASE, _SIZE) overlap pb")
