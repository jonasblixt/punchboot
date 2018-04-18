OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH("arm")
ENTRY(_reset)

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


    PROVIDE (_irq_vectors_start = .);
    .irqvectors :
    {
       *(.irqvectors)
        . = ALIGN(8);
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
        . = . + 1024*128;
		. = ALIGN(8);
		_heap_end = .;
	}
    PROVIDE (_zero_region_end = .);
   
 	.stack :
    {
        PROVIDE(_stack_start = .);
        . = . + 1024*128 ;
        . = ALIGN(8);
        PROVIDE(_stack_end = .);
    }

    PROVIDE(end = .);
}
