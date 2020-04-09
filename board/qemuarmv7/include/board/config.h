/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef BOARD_TEST_INCLUDE_BOARD_CONFIG_H_
#define BOARD_TEST_INCLUDE_BOARD_CONFIG_H_


#define BOARD_BOOT_ARGS "console=ttyLP0,115200  " \
                        "quiet " \
                        "root=PARTUUID=%s " \
                        "ro rootfstype=ext4 gpt rootwait"

#endif  // BOARD_TEST_INCLUDE_BOARD_CONFIG_H_
