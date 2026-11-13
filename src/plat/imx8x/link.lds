/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/**
 * The SoC's region: 128 KiB at the start of the DRAM. The SCFW code seems to
 * have a hardcoded boot vector to that address, and the NXP supplied ATF
 * also has a lot of code that assumes that ATF is loaded there. pb's reset
 * stub is linked there and pb follows the region, so the board's PB_ENTRY is
 * the region's start.
 */
MEMORY
{
    reset(rx) : ORIGIN = 0x80000000, LENGTH = 128K
    ram (rx)  : ORIGIN = ORIGIN(reset) + LENGTH(reset), LENGTH = 16M
    buffers (rx)  : ORIGIN = PB_BUFFERS_BASE, LENGTH = PB_BUFFERS_SIZE
}

ASSERT(PB_ENTRY == ORIGIN(reset),
       "CONFIG_PB_ENTRY is not 0x80000000, where the i.MX8X starts pb")

/* pb here is the reset region too: BL31 is loaded there once pb's stub ran */
ASSERT(ORIGIN(buffers) + LENGTH(buffers) <= ORIGIN(reset) ||
       ORIGIN(buffers) >= ORIGIN(ram) + LENGTH(ram),
       "pb's buffers (CONFIG_PB_BUFFERS_BASE, _SIZE) overlap pb")

SECTIONS
{
    . = ORIGIN(reset);

    .imx8x_reset :
    {
        PROVIDE (_imx8x_start = .);
        KEEP(*(.text.imx8x_reset))
        . = ALIGN(8);
        PROVIDE (_imx8x_end = .);
    } > reset

}
