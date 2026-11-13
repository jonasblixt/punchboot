/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

MEMORY
{
    /* PB_ENTRY from the board's makefile.mk, PB_RAM_LENGTH from the SoC's in makefile.mk */
    ram (rx)  : ORIGIN = PB_ENTRY, LENGTH = PB_RAM_LENGTH
    umctl2 (rx) : ORIGIN = 0x40000000, LENGTH = 128K
    buffers (rx)   : ORIGIN = PB_BUFFERS_BASE, LENGTH = PB_BUFFERS_SIZE
}

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
