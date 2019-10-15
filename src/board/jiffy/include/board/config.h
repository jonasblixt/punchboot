/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef BOARD_JIFFY_INCLUDE_BOARD_CONFIG_H_
#define BOARD_JIFFY_INCLUDE_BOARD_CONFIG_H_


#define EHCI_PHY_BASE 0x02184000

#define BOARD_BOOT_ARGS  "console=ttymxc1,115200 " \
                         "quiet " \
                         "root=PARTUUID=%s " \
                         "rw rootfstype=ext4 gpt rootwait"


#define BOARD_BOOT_ARGS_VERBOSE  "console=ttymxc1,115200 " \
                         "earlyprintk " \
                         "root=PARTUUID=%s " \
                         "rw rootfstype=ext4 gpt rootwait"

#endif  // BOARD_JIFFY_INCLUDE_BOARD_CONFIG_H_
