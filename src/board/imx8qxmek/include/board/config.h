#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__


            /*"earlycon=lpuart32,0x5a060000,115200 earlyprintk " \*/
/*"fec.macaddr=0xe2,0xf4,0x91,0x3a,0x82,0x93 " \*/
#define BOARD_BOOT_ARGS "console=ttyLP0,115200  " \
                        "quiet " \
                        "fec.macaddr=0xe2,0xf4,0x91,0x3a,0x82,0x93 " \
                        "root=PARTUUID=%s " \
                        "ro rootfstype=squashfs gpt rootwait"

#endif
