
/**
 * ERR011115: User should set the boot image start address greater or 
 *   equal to 0x908000 (if the boot image running in OCRAM)
 */

MEMORY
{
    pbram (rx)  : ORIGIN = 0x80001000, LENGTH = 128K
    dram (rx)   : ORIGIN = 0x80021000, LENGTH = 128M
}
