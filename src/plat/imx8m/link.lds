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
    ram (rx)  : ORIGIN = 0x7E1000, LENGTH = 224k
    umctl2 (rx) : ORIGIN = 0x40000000, LENGTH = 128K
    buffers (rx)   : ORIGIN = 0x40060000, LENGTH = 16M
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
