
/**
 * ERR011115: User should set the boot image start address greater or 
 *   equal to 0x908000 (if the boot image running in OCRAM)
 */

MEMORY
{
    ocram (rx)  : ORIGIN = 0x908000, LENGTH = 64K
    dram (rx)   : ORIGIN = 0x80001000, LENGTH = 32M
}
