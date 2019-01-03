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


#define PB_MAX_FAIL_BOOT_COUNT 16

#define BOARD_UUID_FUSE0 15, 4
#define BOARD_UUID_FUSE1 15, 5
#define BOARD_UUID_FUSE2 15, 6
#define BOARD_UUID_FUSE3 15, 7

#define PB_BOOT_FUNC(pbi, system_index) pb_boot_linux_with_dt(pbi, system_index)

#endif
