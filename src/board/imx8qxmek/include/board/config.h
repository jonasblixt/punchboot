#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

#define BOARD_BOOT_ARGS "console=ttyLP0,115200  " \
                        "quiet " \
                        "fec.macaddr=0xe2,0xf4,0x91,0x3a,0x82,0x93 "

#define BOARD_BOOT_ARGS_VERBOSE "console=ttyLP0,115200  " \
                        "earlycon=adma_lpuart32,0x5a060000,115200 earlyprintk " \
                        "fec.macaddr=0xe2,0xf4,0x91,0x3a,0x82,0x93 "
#endif
