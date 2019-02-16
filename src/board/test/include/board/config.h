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


#define BOARD_BOOT_ARGS "console=ttyLP0,115200  " \
                        "quiet " \
                        "root=PARTUUID=%s " \
                        "ro rootfstype=ext4 gpt rootwait"

#endif
