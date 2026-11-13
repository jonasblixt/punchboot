/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Where pb, its buffers and the board's DRAM are is the board's, from its
 * defconfig (platform.md 5.1): PB_ENTRY, PB_SIZE, PB_BUFFERS_BASE and _SIZE,
 * BOARD_DRAM_BASE and _SIZE. The addresses here are the SoC's, and the ones
 * that differ per SoC are makefile.mk's: IMX8M_LOAD_BASE, IMX8M_LOAD_END and
 * IMX8M_OCRAM_SIZE.
 */

/* pb runs from on-chip RAM: it has the ROM's load window from PB_ENTRY to
 * the window's end; a board with PB_SIZE gives it less.
 *
 * The length is written out in MEMORY: a symbol that this script makes from
 * the board's values isn't known yet when ld works MEMORY out, and the
 * region then has every address, without a message. make.link checks the
 * regions. */
MEMORY
{
    ram (rx)  : ORIGIN = PB_ENTRY,
                LENGTH = (PB_SIZE != 0 ? PB_ENTRY + PB_SIZE : IMX8M_LOAD_END) - PB_ENTRY
    umctl2 (rx) : ORIGIN = 0x40000000, LENGTH = 128K
    buffers (rx)   : ORIGIN = PB_BUFFERS_BASE, LENGTH = PB_BUFFERS_SIZE
}

IMX8M_OCRAM_BASE = 0x00900000;
IMX8M_OCRAM_END = IMX8M_OCRAM_BASE + IMX8M_OCRAM_SIZE;
BOARD_DRAM_END = BOARD_DRAM_BASE + BOARD_DRAM_SIZE;
PB_REGION_END = ORIGIN(ram) + LENGTH(ram);

ASSERT(PB_ENTRY >= IMX8M_LOAD_BASE && PB_ENTRY < IMX8M_LOAD_END,
       "CONFIG_PB_ENTRY is not in the on-chip RAM the i.MX8M's ROM loads pb to")

ASSERT(PB_REGION_END > PB_ENTRY && PB_REGION_END <= IMX8M_LOAD_END,
       "CONFIG_PB_SIZE ends pb's region after the memory the ROM loads pb to")

ASSERT((PB_BUFFERS_BASE >= BOARD_DRAM_BASE &&
        PB_BUFFERS_BASE + PB_BUFFERS_SIZE <= BOARD_DRAM_END) ||
       (PB_BUFFERS_BASE >= IMX8M_OCRAM_BASE &&
        PB_BUFFERS_BASE + PB_BUFFERS_SIZE <= IMX8M_OCRAM_END),
       "pb's buffers (CONFIG_PB_BUFFERS_BASE, _SIZE) are not in the board's DRAM or the OCRAM")

ASSERT(ORIGIN(buffers) + LENGTH(buffers) <= ORIGIN(ram) ||
       ORIGIN(buffers) >= ORIGIN(ram) + LENGTH(ram),
       "pb's buffers (CONFIG_PB_BUFFERS_BASE, _SIZE) overlap pb")

/* Used to store run-time training data for the dram -controller
 */

SECTIONS
{
    .umctl (NOLOAD):
    {
        PROVIDE(_umctl2_start = .);
        *(.umctl2 .umctl2*)
        . = ALIGN(8);
        PROVIDE(_umctl2_end = .);
    } > umctl2
}

/* TF-A's bl_params are where BL31 takes them from (plat.c, makefile.mk) */
_plat_bl_params_end = DEFINED(plat_bl_params) ? plat_bl_params + PLAT_BL_PARAMS_SIZE : 0;
ASSERT(_plat_bl_params_end <= PLAT_BL_PARAMS_LIMIT,
       "plat_bl_params is above the memory BL31 takes bl_params from")
