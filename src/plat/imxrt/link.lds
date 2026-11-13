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

MEMORY
{
    ram (rw)  : ORIGIN = PB_ENTRY, LENGTH = 256k
    buffers (rw)  : ORIGIN = PB_BUFFERS_BASE, LENGTH = PB_BUFFERS_SIZE
}
