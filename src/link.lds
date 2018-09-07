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
    }

	. = ALIGN(8);

    PROVIDE (_irq_vectors_start = .);
    .irqvectors :
    {
       *(.irqvectors)
        . = ALIGN(8);
    }

    .init_array : 
    {
        PROVIDE(__init_array_start = .);
		KEEP(*(SORT(.init_array.*)))
		KEEP(*(.init_array))
        PROVIDE(__init_array_end = .);
    }
 
	. = ALIGN(8);

    .fini_array : 
    {
        PROVIDE(__fini_array_start = .);
		KEEP(*(SORT(.fini_array.*)))
		KEEP(*(.fini_array))
        PROVIDE(__fini_array_end = .);
    }

    .data :
    {
        *(.data)
        *(.rodata .rodata.*)
        . = ALIGN(8);
        _edata = .;
    }
    PROVIDE (_code_end = .);

 
    .bigbuffer (NOLOAD): 
    {
        PROVIDE(_big_buffer_start = .);
        *(.bigbuffer .bigbuffer*)
        . = ALIGN(8);
        PROVIDE(_big_buffer_end = .);
    }


    PROVIDE (_zero_region_start = .);
    .bss :
    {
        _bss_start = .;
        *(.bss)
        . = ALIGN(8);
        _bss_end = .;
	}

	.heap :
	{
	    _heap_start = .;
		*(.heap)
        . = . + 1024*1024;
		. = ALIGN(8);
		_heap_end = .;
	}
    PROVIDE (_zero_region_end = .);
   
 	.stack :
    {
        PROVIDE(_stack_start = .);
        . = . + 1024*1024 ;
        . = ALIGN(8);
        PROVIDE(_stack_end = .);
    }
                            
    PROVIDE(end = .);
}
