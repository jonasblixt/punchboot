
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

MEMORY
{

    pbram (rx)  : ORIGIN = 0x7E1000, LENGTH = 128K
    pbram2 (rx) : ORIGIN = 0x180000, LENGTH = 32K
    umctl2 (rx) : ORIGIN = 0x40000000, LENGTH = 128K
    dram (rx)   : ORIGIN = 0x48000000, LENGTH = 128M


}

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
