/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __BOARD_H_
#define __BOARD_H_

#include <pb.h>
#include <usb.h>
#include <image.h>

struct board_info {
    uint16_t type;
    uint8_t rev;
    uint8_t var;
} __attribute__ ((packed)) ;

uint32_t board_early_init(void);
uint32_t board_late_init(void);
uint8_t  board_force_recovery(void);
uint32_t board_configure_gpt_tbl(void);
uint32_t board_usb_init(struct usb_device **dev);
uint32_t board_get_debug_uart(void);
uint32_t board_configure_bootargs(char *buf, char *boot_part_uuid);

#endif
