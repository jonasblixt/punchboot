/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * pb proper (D-088). Where pb, its buffers and the board's DRAM are is the
 * board's, from its makefile.mk (platform.md 5.1): PB_ENTRY, PB_SIZE,
 * PB_BUFFERS_BASE and _SIZE, BOARD_DRAM_BASE and _SIZE. pb proper is in the
 * board's DRAM, the SPL brought it up. The addresses here are the SoC's, and
 * the ones that differ per SoC are makefile.mk's: the RAM log's and
 * bl_params', at the end of the SPL's window, which pb proper keeps
 * (spl/link.lds).
 */

/* The start of the DRAM, where the DDR controller's state is: the SPL wrote
 * it, TF-A's i.MX8M DRAM driver reads it there (plat/imx8m/dram.h) */
IMX8M_DDR_BASE = 0x40000000;

/* pb proper is in DRAM from PB_ENTRY, PB_SIZE long.
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
    ram (rx)  : ORIGIN = PB_ENTRY, LENGTH = PB_SIZE
    buffers (rx)   : ORIGIN = IMX8M_DDR_BASE,
                     LENGTH = PB_BUFFERS_BASE + PB_BUFFERS_SIZE - IMX8M_DDR_BASE
    spl_ram (rw) : ORIGIN = IMX8M_RAMLOG_BASE,
                   LENGTH = PLAT_BL_PARAMS_BASE + 0x1000 - IMX8M_RAMLOG_BASE
}

IMX8M_OCRAM_BASE = 0x00900000;
IMX8M_OCRAM_END = IMX8M_OCRAM_BASE + IMX8M_OCRAM_SIZE;
BOARD_DRAM_END = BOARD_DRAM_BASE + BOARD_DRAM_SIZE;
PB_REGION_END = ORIGIN(ram) + LENGTH(ram);

ASSERT(PB_SIZE != 0, "PB_SIZE: pb proper's region in the DRAM is the board's (D-088)")

ASSERT(PB_ENTRY >= BOARD_DRAM_BASE && PB_REGION_END <= BOARD_DRAM_END,
       "pb proper (PB_ENTRY, PB_SIZE) is not in the board's DRAM (D-088)")

/* The buffers are in the DRAM here, after the controller's state */
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

SECTIONS
{
    /* The DDR controller's state, which the SPL wrote for TF-A
     * (IMX8M_DRAM_TIMING_SAVE_SIZE, plat/imx8m/dram.h): kept free */
    .umctl IMX8M_DDR_BASE (NOLOAD):
    {
        PROVIDE(_umctl2_start = .);
        *(.umctl2 .umctl2*)
        . = 0x20000; /* From the section's start */
        PROVIDE(_umctl2_end = .);
    } > buffers

    /* The SPL's RAM log, which pb proper goes on with (ramlog.c, D-088) */
    .ramlog_spl IMX8M_RAMLOG_BASE (NOLOAD):
    {
        *(.ramlog)
    } > spl_ram

    /* TF-A's bl_params (platform.md 5.3), where BL31 takes them from */
    .bl_params PLAT_BL_PARAMS_BASE (NOLOAD):
    {
        *(.bl_params)
    } > spl_ram
}

/* TF-A's bl_params are where BL31 takes them from (plat.c, makefile.mk) */
_plat_bl_params_end = DEFINED(plat_bl_params) ? plat_bl_params + PLAT_BL_PARAMS_SIZE : 0;
ASSERT(_plat_bl_params_end <= PLAT_BL_PARAMS_LIMIT,
       "plat_bl_params is above the memory BL31 takes bl_params from")
