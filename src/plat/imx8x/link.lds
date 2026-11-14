/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Where pb, its buffers and the board's DRAM are is the board's, from its
 * makefile.mk (platform.md 5.1): PB_ENTRY, PB_SIZE, PB_BUFFERS_BASE and _SIZE,
 * BOARD_DRAM_BASE and _SIZE. The addresses here are the SoC's.
 */

/**
 * The SoC's region: 128 KiB at the start of the DRAM. The SCFW code seems to
 * have a hardcoded boot vector to that address, and the NXP supplied ATF
 * also has a lot of code that assumes that ATF is loaded there. pb's reset
 * stub is linked there and pb follows the region, so the board's PB_ENTRY is
 * the region's start.
 */
IMX8X_RESET_BASE = 0x80000000;
IMX8X_RESET_SIZE = 0x20000;

/* pb is loaded into the DRAM, which the SCFW set up. It has the DRAM from
 * PB_ENTRY to the buffers, when they follow it, and else to the DRAM's end;
 * a board with PB_SIZE gives it less.
 *
 * The length is written out in MEMORY: a symbol that this script makes from
 * the board's values isn't known yet when ld works MEMORY out, and the
 * region then has every address, without a message. make.link checks the
 * regions. */
MEMORY
{
    reset(rx) : ORIGIN = IMX8X_RESET_BASE, LENGTH = IMX8X_RESET_SIZE
    ram (rx)  : ORIGIN = IMX8X_RESET_BASE + IMX8X_RESET_SIZE,
                LENGTH = (PB_SIZE != 0 ? PB_ENTRY + PB_SIZE :
                          (PB_BUFFERS_BASE > PB_ENTRY &&
                           PB_BUFFERS_BASE < BOARD_DRAM_BASE + BOARD_DRAM_SIZE ?
                           PB_BUFFERS_BASE : BOARD_DRAM_BASE + BOARD_DRAM_SIZE)) -
                         (IMX8X_RESET_BASE + IMX8X_RESET_SIZE)
    buffers (rx)  : ORIGIN = PB_BUFFERS_BASE, LENGTH = PB_BUFFERS_SIZE
}

BOARD_DRAM_END = BOARD_DRAM_BASE + BOARD_DRAM_SIZE;
PB_REGION_END = ORIGIN(ram) + LENGTH(ram);

ASSERT(PB_ENTRY == ORIGIN(reset),
       "PB_ENTRY is not 0x80000000, where the i.MX8X starts pb")

ASSERT(PB_ENTRY >= BOARD_DRAM_BASE && PB_REGION_END > ORIGIN(ram) &&
       PB_REGION_END <= BOARD_DRAM_END,
       "pb (PB_ENTRY, PB_SIZE) is not in the board's DRAM")

ASSERT(PB_BUFFERS_BASE >= BOARD_DRAM_BASE &&
       PB_BUFFERS_BASE + PB_BUFFERS_SIZE <= BOARD_DRAM_END,
       "pb's buffers (PB_BUFFERS_BASE, _SIZE) are not in the board's DRAM")

/* pb here is the reset region too: BL31 is loaded there once pb's stub ran */
ASSERT(ORIGIN(buffers) + LENGTH(buffers) <= ORIGIN(reset) ||
       ORIGIN(buffers) >= ORIGIN(ram) + LENGTH(ram),
       "pb's buffers (PB_BUFFERS_BASE, _SIZE) overlap pb")

/* The Firmware Handoff region is the board's too (CONFIG_BOOT_FH): in the
 * same memory. src/boot/fh.lds keeps it off pb and the buffers */
BOOT_FH_REGION_END = DEFINED(BOOT_FH_REGION_BASE) ? BOOT_FH_REGION_BASE + BOOT_FH_REGION_SIZE : 0;
ASSERT(DEFINED(BOOT_FH_REGION_BASE) ?
       (BOOT_FH_REGION_BASE >= BOARD_DRAM_BASE && BOOT_FH_REGION_END <= BOARD_DRAM_END) : 1,
       "The Firmware Handoff region (CONFIG_BOOT_FH_REGION_BASE, _SIZE) is not in the board's DRAM")

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
