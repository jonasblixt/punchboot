
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
    umctl2 (rx) : ORIGIN = 0x40000000, LENGTH = 128K
    pbram (rx)  : ORIGIN = 0x7E1000, LENGTH = 128K
    dram (rx)   : ORIGIN = 0x40060000, LENGTH = 24M
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

	. = PB_ENTRY;

	.text :
 	{
        PROVIDE (_code_start = .);
		*(.vectors .vectors.*)
        . = ALIGN(8);
		*(.text .text.*)
        . = ALIGN(8);
        _etext = .;
        PROVIDE (_code_end = .);
    } > pbram

    .data :
    {
        PROVIDE (_data_region_start = .);
        *(.data)
        *(.rodata .rodata.*)
        . = ALIGN(8);
        _edata = .;
        PROVIDE (_data_region_end = .);
    } > pbram

    .bss (NOLOAD):
    {
        PROVIDE (_zero_region_start = .);
        _bss_start = .;
        *(.bss)
        . = ALIGN(8);
        _bss_end = .;
        PROVIDE (_zero_region_end = .);
	} > pbram

 	.stack (NOLOAD):
    {
        PROVIDE(_stack_start = .);
        . = . + 10*1024 ;
        . = ALIGN(8);
        PROVIDE(_stack_end = .);
    } > pbram

    .bigbuffer (NOLOAD):
    {
        PROVIDE(_big_buffer_start = .);
        *(.bigbuffer .bigbuffer*)
        . = ALIGN(8);
        PROVIDE(_big_buffer_end = .);
    } > dram

    PROVIDE(end = .);
}


_bss_size = (_bss_end - _bss_start)>>3;

