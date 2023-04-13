
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


MEMORY
{
    ram (rx)  : ORIGIN = 0x40000000, LENGTH = 32M
}


SECTIONS
{
    .init_array :
    {
        . = ALIGN(4096);
        PROVIDE(__init_array_start = .);
		KEEP(*(SORT(.init_array.*)))
		KEEP(*(.init_array))
        PROVIDE(__init_array_end = .);
        . = ALIGN(4096);
        PROVIDE(__init_array_end2 = .); /* Align to page, for mmu mapping */
    } > ram

	. = ALIGN(8);

    .fini_array :
    {
        . = ALIGN(4096);
        PROVIDE(__fini_array_start = .);
		KEEP(*(SORT(.fini_array.*)))
		KEEP(*(.fini_array))
        PROVIDE(__fini_array_end = .);
        . = ALIGN(4096);
        PROVIDE(__fini_array_end2 = .);
    } > ram
}
