OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH("arm")
ENTRY(_start_vect)


SECTIONS
{

	. = 0x82000000;

	PROVIDE(_code_start = .);
	PROVIDE(_reloc_region_start = .);

	.text :
 	{
		KEEP(*(.expvect))
		*(.text)
		. = ALIGN(16);
		_etext = .;
	}

	.data :
	{
		*(.data)
		. = ALIGN(16);
		_edata = .;
	}

	.rodata :
	{
		*(.rodata .rodata.*)
		. = ALIGN(16);
		_erodata = .;
	}

	. = ALIGN(16);

	PROVIDE(_reloc_region_end = .);

	PROVIDE(_zero_region_start = .);

	.bss :
	{
		PROVIDE(_bss_start = .);
		*(.bss)
		. = ALIGN(16);
		_ebss = .;
		PROVIDE(_bss_end = .);
	}

	.heap :
	{
		PROVIDE(_heap_start = .);
		*(.heap)
                . = . + 8192;
		. = ALIGN(4);
		PROVIDE(_heap_end = .);
	}

	. = ALIGN(16);

	PROVIDE(_zero_region_end = .);

 	.svc_stack :
        {
                PROVIDE(_svc_stack_start = .);
                . = . + 4096;
                . = ALIGN(4);
                PROVIDE(_svc_stack_end = .);
        }

 	.abt_stack :
        {
                PROVIDE(_abt_stack_start = .);
                . = . + 4096;
                . = ALIGN(4);
                PROVIDE(_abt_stack_end = .);
        }

 	.und_stack :
        {
                PROVIDE(_und_stack_start = .);
                . = . + 4096;
                . = ALIGN(4);
                PROVIDE(_und_stack_end = .);
        }

 	.irq_stack :
        {
                PROVIDE(_irq_stack_start = .);
                . = . + 4096;
                . = ALIGN(4);
                PROVIDE(_irq_stack_end = .);
        }

 	.fiq_stack :
        {
                PROVIDE(_fiq_stack_start = .);
                . = . + 4096;
                . = ALIGN(4);
                PROVIDE(_fiq_stack_end = .);
        }

 	.usr_stack :
        {
                PROVIDE(_usr_stack_start = .);
                . = . + 1024*32 ;
                . = ALIGN(4);
                PROVIDE(_usr_stack_end = .);
        }

	PROVIDE(_code_end = .);

    . = ALIGN(1024*100);
    PROVIDE(end = .);

}
