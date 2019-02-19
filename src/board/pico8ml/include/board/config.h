#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

#define  BOARD_BOOT_ARGS "console=ttymxc0,115200 " \
        "earlycon=ec_imx6q,0x30860000,115200 earlyprintk " \
        "cma=768M " \
        "root=PARTUUID=%s " \
        "rw rootfstype=ext4 gpt rootwait"

#endif
