/*
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

MEMORY
{
    ram (rwx) : ORIGIN = 0x20210000, LENGTH = 0x20249000 - 0x20210000
}

SECTIONS
{
    /* VTOR needs the table aligned to its size rounded up to a power of
     * two: 176 entries, 1 KiB */
    .text : ALIGN(1024)
    {
        KEEP(*(.vectors))
        *(.text .text.*)
        *(.rodata .rodata.*)
        /* pb's FCB, the configuration for the ROM's NOR API */
        KEEP(*(.fcb))
        . = ALIGN(4);
    } > ram

    .data : ALIGN(4)
    {
        *(.data .data.*)
        . = ALIGN(4);
    } > ram

    .bss (NOLOAD) : ALIGN(4)
    {
        _bss_start = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > ram

    .stack (NOLOAD) : ALIGN(8)
    {
        . += 8K;
        _stack_top = .;
    } > ram

    /DISCARD/ : { *(.ARM.exidx*) *(.comment) }
}
