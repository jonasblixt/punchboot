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

	.text :
 	{
        PROVIDE (_code_start = .);
		*(.text)
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
	} > pbram2
   
 	.stack (NOLOAD):
    {
        PROVIDE(_stack_start = .);
        . = . + 10*1024 ;
        . = ALIGN(8);
        PROVIDE(_stack_end = .);
    } > pbram2
                            
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

