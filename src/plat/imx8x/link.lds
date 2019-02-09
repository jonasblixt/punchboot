
MEMORY
{
    reset(rx)   : ORIGIN = 0x80000000, LENGTH = 128K
    pbram (rx)  : ORIGIN = 0x80020000, LENGTH = 256K
    pbram2 (rx) : ORIGIN = 0x80060000, LENGTH = 256K
    dram (rx)   : ORIGIN = 0x800a0000, LENGTH = 24M

}

/**
 * Provide a 128 kByte hole for loading ATF at 0x80000000.
 * The SCFW code seems to have a hardcoded boot vector to that address.
 * The NXP supplied ATF also has a lot of code that assumes that ATF
 * is loaded at that perticular address.
 */



SECTIONS
{
	. = 0x80000000;

	.imx8x_reset :
 	{
        PROVIDE (_imx8x_start = .);
		*(.text.imx8x_reset)
        . = ALIGN(8);
        PROVIDE (_imx8x_end = .);
    } > reset
}
