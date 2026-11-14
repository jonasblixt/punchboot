/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Where pb, its buffers and the board's DRAM are is the board's, from its
 * makefile.mk (platform.md 5.1): PB_ENTRY, PB_SIZE, PB_BUFFERS_BASE and _SIZE,
 * BOARD_DRAM_BASE and _SIZE. The addresses here are the SoC's, and the ones
 * that differ per SoC are makefile.mk's: IMX8M_LOAD_BASE, IMX8M_LOAD_END and
 * IMX8M_OCRAM_SIZE.
 */

/* The start of the DRAM, where the DDR controller's state is: TF-A's i.MX8M
 * DRAM driver reads it there (plat/imx8m/dram.h) */
IMX8M_DDR_BASE = 0x40000000;

/* pb runs from on-chip RAM: it has the ROM's load window from PB_ENTRY to
 * the window's end; a board with PB_SIZE gives it less.
 *
 * The buffers region is the DRAM from its start to the end of the board's
 * buffers: the controller's state is linked there too, so that everything pb
 * links in the DRAM is in the region, and no image data region of a board
 * may hold it (platform.md 5.3). pb's own buffers start at PB_BUFFERS_BASE,
 * as everywhere (src/link.lds).
 *
 * The length is written out in MEMORY: a symbol that this script makes from
 * the board's values isn't known yet when ld works MEMORY out, and the
 * region then has every address, without a message. make.link checks the
 * regions. */
MEMORY
{
    ram (rx)  : ORIGIN = PB_ENTRY,
                LENGTH = (PB_SIZE != 0 ? PB_ENTRY + PB_SIZE : IMX8M_LOAD_END) - PB_ENTRY
    buffers (rx)   : ORIGIN = IMX8M_DDR_BASE,
                     LENGTH = PB_BUFFERS_BASE + PB_BUFFERS_SIZE - IMX8M_DDR_BASE
}

IMX8M_OCRAM_BASE = 0x00900000;
IMX8M_OCRAM_END = IMX8M_OCRAM_BASE + IMX8M_OCRAM_SIZE;
BOARD_DRAM_END = BOARD_DRAM_BASE + BOARD_DRAM_SIZE;
PB_REGION_END = ORIGIN(ram) + LENGTH(ram);

ASSERT(PB_ENTRY >= IMX8M_LOAD_BASE && PB_ENTRY < IMX8M_LOAD_END,
       "PB_ENTRY is not in the on-chip RAM the i.MX8M's ROM loads pb to")

ASSERT(PB_REGION_END > PB_ENTRY && PB_REGION_END <= IMX8M_LOAD_END,
       "PB_SIZE ends pb's region after the memory the ROM loads pb to")

/* The buffers are in the DRAM here, after the controller's state: the on-chip
 * RAM has no room for them, and the region is one range */
ASSERT(PB_BUFFERS_BASE >= BOARD_DRAM_BASE && PB_BUFFERS_BASE >= ADDR(.umctl) + SIZEOF(.umctl) &&
       PB_BUFFERS_BASE + PB_BUFFERS_SIZE <= BOARD_DRAM_END,
       "pb's buffers (PB_BUFFERS_BASE, _SIZE) are not in the board's DRAM, after the DDR controller's state")

ASSERT(ORIGIN(buffers) + LENGTH(buffers) <= ORIGIN(ram) ||
       ORIGIN(buffers) >= ORIGIN(ram) + LENGTH(ram),
       "pb's buffers (PB_BUFFERS_BASE, _SIZE) overlap pb")

/* The Firmware Handoff region is the board's too (CONFIG_BOOT_FH): in the
 * same memory. src/boot/fh.lds keeps it off pb and the buffers */
BOOT_FH_REGION_END = DEFINED(BOOT_FH_REGION_BASE) ? BOOT_FH_REGION_BASE + BOOT_FH_REGION_SIZE : 0;
ASSERT(DEFINED(BOOT_FH_REGION_BASE) ?
       ((BOOT_FH_REGION_BASE >= BOARD_DRAM_BASE && BOOT_FH_REGION_END <= BOARD_DRAM_END) ||
        (BOOT_FH_REGION_BASE >= IMX8M_OCRAM_BASE && BOOT_FH_REGION_END <= IMX8M_OCRAM_END)) : 1,
       "The Firmware Handoff region (CONFIG_BOOT_FH_REGION_BASE, _SIZE) is not in the board's DRAM or the OCRAM")

/* Used to store run-time training data for the dram -controller
 */

SECTIONS
{
    .umctl IMX8M_DDR_BASE (NOLOAD):
    {
        PROVIDE(_umctl2_start = .);
        *(.umctl2 .umctl2*)
        . = ALIGN(8);
        PROVIDE(_umctl2_end = .);
    } > buffers
}

/* TF-A's bl_params are where BL31 takes them from (plat.c, makefile.mk) */
_plat_bl_params_end = DEFINED(plat_bl_params) ? plat_bl_params + PLAT_BL_PARAMS_SIZE : 0;
ASSERT(_plat_bl_params_end <= PLAT_BL_PARAMS_LIMIT,
       "plat_bl_params is above the memory BL31 takes bl_params from")
