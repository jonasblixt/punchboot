
MEMORY
{
    ocram (rx)  : ORIGIN = 0x908000, LENGTH = 64K
    dram (rx)   : ORIGIN = 0x40000000, LENGTH = 32M
}
 

SECTIONS
{
    /* TODO: Move to QEMU target plat */
    .init_array : 
    {
        PROVIDE(__init_array_start = .);
		KEEP(*(SORT(.init_array.*)))
		KEEP(*(.init_array))
        PROVIDE(__init_array_end = .);
    } > ocram
 
	. = ALIGN(8);
    
    /* TODO: Move to QEMU target plat*/
    .fini_array : 
    {
        PROVIDE(__fini_array_start = .);
		KEEP(*(SORT(.fini_array.*)))
		KEEP(*(.fini_array))
        PROVIDE(__fini_array_end = .);
    } > ocram
}
