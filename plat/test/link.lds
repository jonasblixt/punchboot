
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
    ocram (rx)  : ORIGIN = 0x908000, LENGTH = 64K
    pbram (rx)  : ORIGIN = 0x40000000, LENGTH = 1M
    pbram2 (rx) : ORIGIN = 0x40100000, LENGTH = 16M
    dram (rx)   : ORIGIN = 0x41100000, LENGTH = 200M
}


SECTIONS
{
    .init_array :
    {
        PROVIDE(__init_array_start = .);
		KEEP(*(SORT(.init_array.*)))
		KEEP(*(.init_array))
        PROVIDE(__init_array_end = .);
    } > dram

	. = ALIGN(8);

    .fini_array :
    {
        PROVIDE(__fini_array_start = .);
		KEEP(*(SORT(.fini_array.*)))
		KEEP(*(.fini_array))
        PROVIDE(__fini_array_end = .);
    } > dram
}
