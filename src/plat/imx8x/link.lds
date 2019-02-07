
MEMORY
{
    reset(rx)   : ORIGIN = 0x80000000, LENGTH = 128K
    pbram (rx)  : ORIGIN = 0x80020000, LENGTH = 256K
    pbram2 (rx) : ORIGIN = 0xa0060000, LENGTH = 256K
    dram (rx)   : ORIGIN = 0xa0080000, LENGTH = 128M

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
		*(.imx8x_reset)
        . = ALIGN(8);
    } > reset
}
