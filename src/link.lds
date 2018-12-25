/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


SECTIONS
{

	. = PB_ENTRY;

    PROVIDE (_code_start = .);
	.text :
 	{
		*(.text)
        . = ALIGN(8);
        _etext = .;
    } > dram

    . = ALIGN(8);
    PROVIDE (_code_end = .);

    .data :
    {
        *(.data)
        *(.rodata .rodata.*)
        . = ALIGN(8);
        _edata = .;
    } > dram

    . = ALIGN(8);
    PROVIDE (_zero_region_start = .);

    .bss :
    {
        _bss_start = .;
        *(.bss)
        . = ALIGN(8);
        _bss_end = .;
	} > dram

    PROVIDE (_zero_region_end = .);
   
 	.stack :
    {
        PROVIDE(_stack_start = .);
        . = . + 32*1024 ;
        . = ALIGN(8);
        PROVIDE(_stack_end = .);
    } > dram
                            
    .bigbuffer (NOLOAD): 
    {
        PROVIDE(_big_buffer_start = .);
        *(.bigbuffer .bigbuffer*)
        . = ALIGN(8);
        PROVIDE(_big_buffer_end = .);
    } > dram

    PROVIDE(end = .);
}
