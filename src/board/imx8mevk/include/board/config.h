/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef BOARD_IMX8MEVK_INCLUDE_BOARD_CONFIG_H_
#define BOARD_IMX8MEVK_INCLUDE_BOARD_CONFIG_H_

#define BOARD_BOOT_ARGS       \
    "console=ttymxc0,115200 " \
    "quiet "                  \
    "cma=768M "               \
    "root=PARTUUID=%s "       \
    "rw rootfstype=ext4 gpt rootwait"

#define BOARD_BOOT_ARGS_VERBOSE                        \
    "console=ttymxc0,115200 "                          \
    "earlycon=ec_imx6q,0x30860000,115200 earlyprintk " \
    "cma=768M "                                        \
    "root=PARTUUID=%s "                                \
    "rw rootfstype=ext4 gpt rootwait"
#endif // BOARD_IMX8MEVK_INCLUDE_BOARD_CONFIG_H_
