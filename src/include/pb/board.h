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

#define BOARD_OTP_WRITE_KEY 0xaabbccdd

uint32_t board_init(void);
uint8_t board_force_recovery(void);

uint32_t board_get_uuid(uint8_t *uuid);
uint32_t board_get_boardinfo(struct board_info *info);

uint32_t board_write_uuid(uint8_t *uuid, uint32_t key);
uint32_t board_write_boardinfo(struct board_info *info, uint32_t key);
uint32_t board_write_standard_fuses(uint32_t key);
uint32_t board_write_mac_addr(uint8_t *mac_addr, uint32_t len, uint32_t index, 
                                                        uint32_t key);
uint32_t board_enable_secure_boot(uint32_t key);

uint32_t board_write_gpt_tbl();
uint32_t board_usb_init(struct usb_device **dev);
void board_boot(struct pb_pbi *pbi);
#endif
