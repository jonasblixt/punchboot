/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/* pb runs from on-chip RAM: it has the ROM's load window, IMX8M_LOAD_BASE to
 * IMX8M_LOAD_END (the SoC's, makefile.mk), from the board's PB_ENTRY to the
 * window's end. Its buffers are in DRAM (platform.md 5.1) */
MEMORY
{
    ram (rx)  : ORIGIN = PB_ENTRY, LENGTH = IMX8M_LOAD_END - PB_ENTRY
    umctl2 (rx) : ORIGIN = 0x40000000, LENGTH = 128K
    buffers (rx)   : ORIGIN = PB_BUFFERS_BASE, LENGTH = PB_BUFFERS_SIZE
}

ASSERT(PB_ENTRY >= IMX8M_LOAD_BASE && PB_ENTRY < IMX8M_LOAD_END,
       "CONFIG_PB_ENTRY is not in the on-chip RAM the i.MX8M's ROM loads pb to")

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
