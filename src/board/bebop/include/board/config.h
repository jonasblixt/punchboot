/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */



#ifndef __BOARD_CONFIG__
#define __BOARD_CONFIG__


#define EHCI_PHY_BASE 0x02184000

#define BOARD_BOOT_ARGS  "console=ttymxc1,115200 " \
                         "earlyprintk " \
                         "root=PARTUUID=%s " \
                         "rw rootfstype=ext4 gpt rootwait"
#endif
