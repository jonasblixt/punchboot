#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__


            /*"earlycon=lpuart32,0x5a060000,115200 earlyprintk " \*/
#define BOARD_BOOT_ARGS "console=ttyLP0,115200  " \
                        "earlycon=lpuart32,0x5a060000,115200 earlyprintk " \
                        "root=PARTUUID=%s " \
                        "ro rootfstype=ext4 gpt rootwait"

#endif
