/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Where pb, its buffers and the board's DRAM are is the board's, from its
 * defconfig (platform.md 5.1): PB_ENTRY, PB_SIZE, PB_BUFFERS_BASE and _SIZE,
 * BOARD_DRAM_BASE and _SIZE. The addresses here are the SoC's.
 */

/**
 * The OCRAM, and where in it the ROM may load pb:
 *
 * ERR011115: User should set the boot image start address greater or
 *   equal to 0x908000 (if the boot image running in OCRAM)
 */
IMX6UL_OCRAM_BASE = 0x00900000;
IMX6UL_OCRAM_LOAD_BASE = 0x00908000;
IMX6UL_OCRAM_END = 0x00920000;

/* The ROM's load windows: that part of the OCRAM, and the board's DRAM,
 * which the ROM sets up from the board's DCD. pb has the window PB_ENTRY is
 * in, from PB_ENTRY to the buffers, when they follow it there, and else to
 * the window's end; a board with PB_SIZE gives it less.
 *
 * The length is written out in MEMORY: a symbol that this script makes from
 * the board's values isn't known yet when ld works MEMORY out, and the
 * region then has every address, without a message. make.link checks the
 * regions. */
MEMORY
{
    ram (rx)  : ORIGIN = PB_ENTRY,
                LENGTH = (PB_SIZE != 0 ? PB_ENTRY + PB_SIZE :
                          (PB_ENTRY >= IMX6UL_OCRAM_LOAD_BASE && PB_ENTRY < IMX6UL_OCRAM_END ?
                           (PB_BUFFERS_BASE > PB_ENTRY && PB_BUFFERS_BASE < IMX6UL_OCRAM_END ?
                            PB_BUFFERS_BASE : IMX6UL_OCRAM_END) :
                           (PB_BUFFERS_BASE > PB_ENTRY &&
                            PB_BUFFERS_BASE < BOARD_DRAM_BASE + BOARD_DRAM_SIZE ?
                            PB_BUFFERS_BASE : BOARD_DRAM_BASE + BOARD_DRAM_SIZE))) - PB_ENTRY
    buffers (rx)  : ORIGIN = PB_BUFFERS_BASE, LENGTH = PB_BUFFERS_SIZE
}

BOARD_DRAM_END = BOARD_DRAM_BASE + BOARD_DRAM_SIZE;
PB_REGION_END = ORIGIN(ram) + LENGTH(ram);
/* The end of the window PB_ENTRY is in */
PB_LOAD_END = PB_ENTRY >= IMX6UL_OCRAM_LOAD_BASE && PB_ENTRY < IMX6UL_OCRAM_END ?
              IMX6UL_OCRAM_END : BOARD_DRAM_END;

ASSERT((PB_ENTRY >= IMX6UL_OCRAM_LOAD_BASE && PB_ENTRY < IMX6UL_OCRAM_END) ||
       (PB_ENTRY >= BOARD_DRAM_BASE && PB_ENTRY < BOARD_DRAM_END),
       "CONFIG_PB_ENTRY is not where the i.MX6UL's ROM loads pb to: the board's DRAM, or the OCRAM from 0x908000")

ASSERT(PB_REGION_END > PB_ENTRY && PB_REGION_END <= PB_LOAD_END,
       "CONFIG_PB_SIZE ends pb's region after the memory the ROM loads pb to")

ASSERT((PB_BUFFERS_BASE >= BOARD_DRAM_BASE &&
        PB_BUFFERS_BASE + PB_BUFFERS_SIZE <= BOARD_DRAM_END) ||
       (PB_BUFFERS_BASE >= IMX6UL_OCRAM_BASE &&
        PB_BUFFERS_BASE + PB_BUFFERS_SIZE <= IMX6UL_OCRAM_END),
       "pb's buffers (CONFIG_PB_BUFFERS_BASE, _SIZE) are not in the board's DRAM or the OCRAM")

ASSERT(ORIGIN(buffers) + LENGTH(buffers) <= ORIGIN(ram) ||
       ORIGIN(buffers) >= ORIGIN(ram) + LENGTH(ram),
       "pb's buffers (CONFIG_PB_BUFFERS_BASE, _SIZE) overlap pb")

/* The Firmware Handoff region is the board's too (CONFIG_BOOT_FH): in the
 * same memory. src/boot/fh.lds keeps it off pb and the buffers */
BOOT_FH_REGION_END = DEFINED(BOOT_FH_REGION_BASE) ? BOOT_FH_REGION_BASE + BOOT_FH_REGION_SIZE : 0;
ASSERT(DEFINED(BOOT_FH_REGION_BASE) ?
       ((BOOT_FH_REGION_BASE >= BOARD_DRAM_BASE && BOOT_FH_REGION_END <= BOARD_DRAM_END) ||
        (BOOT_FH_REGION_BASE >= IMX6UL_OCRAM_BASE && BOOT_FH_REGION_END <= IMX6UL_OCRAM_END)) : 1,
       "The Firmware Handoff region (CONFIG_BOOT_FH_REGION_BASE, _SIZE) is not in the board's DRAM or the OCRAM")
