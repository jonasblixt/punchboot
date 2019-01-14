
MEMORY
{
    pbram (rx)  : ORIGIN = 0x7E1000, LENGTH = 124K
    pbram2 (rx) : ORIGIN = 0x180000, LENGTH = 32K

/*
    pbram (rx)  : ORIGIN = 0x44000000, LENGTH = 124K
    pbram2 (rx) : ORIGIN = 0x45000000, LENGTH = 32K
*/
    dram (rx)   : ORIGIN = 0x40000000, LENGTH = 128M
}
