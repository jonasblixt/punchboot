/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <board.h>
#include <plat.h>
#include <tinyprintf.h>
#include <io.h>
#include <gpt.h>
#include <usb.h>

#include <plat/test/uart.h>
#include <plat/test/pl061.h>

#include "board_config.h"

const uint8_t part_type_config[] = {0xF7, 0xDD, 0x45, 0x34, 0xCC, 0xA5, 0xC6, 
        0x45, 0xAA, 0x17, 0xE4, 0x10, 0xA5, 0x42, 0xBD, 0xB8};

const uint8_t part_type_system_a[] = {0x59, 0x04, 0x49, 0x1E, 0x6D, 0xE8, 0x4B, 
        0x44, 0x82, 0x93, 0xD8, 0xAF, 0x0B, 0xB4, 0x38, 0xD1};

const uint8_t part_type_system_b[] = { 0x3C, 0x29, 0x85, 0x3F, 0xFB, 0xC6, 0xD0, 
        0x42, 0x9E, 0x1A, 0xAC, 0x6B, 0x35, 0x60, 0xC3, 0x04,};

uint32_t board_usb_init(struct usb_device **dev)
{
   return PB_OK;
}

/* TODO: MOVE TO Platform */
__inline uint32_t plat_get_ms_tick(void) {
    return 1;
}


uint32_t board_init(void)
{
    test_uart_init();
    init_printf(NULL, &plat_uart_putc);
 
    pl061_init(0x09030000);
    return PB_OK;
}

uint8_t board_force_recovery(void) {
    return true;
}


uint32_t board_get_uuid(uint8_t *uuid) {
    uint32_t *uuid_ptr = (uint32_t *) uuid;

    return PB_OK;
}

uint32_t board_get_boardinfo(struct board_info *info) {
    UNUSED(info);
    return PB_OK;
}

uint32_t board_write_uuid(uint8_t *uuid, uint32_t key) {
    return PB_OK;
}

uint32_t board_write_boardinfo(struct board_info *info, uint32_t key) {
    UNUSED(info);
    UNUSED(key);
    return PB_OK;
}

uint32_t board_write_gpt_tbl() {
    return PB_ERR;
}

uint32_t board_write_standard_fuses(uint32_t key) {
   return PB_OK;
}

uint32_t board_write_mac_addr(uint8_t *mac_addr, uint32_t len, uint32_t index, uint32_t key) {
    UNUSED(mac_addr);
    UNUSED(len);
    UNUSED(index);
    UNUSED(key);

    return PB_OK;
}

uint32_t board_enable_secure_boot(uint32_t key) {
    UNUSED(key);
    return PB_OK;
}

