/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>

#define PB_CONFIG_BOOT       0
#define PB_CONFIG_BOOT_COUNT 1
#define PB_CONFIG_FORCE_RECOVERY 2

struct pb_config_item {
    const int8_t index;
    const char description[16];
#define PB_CONFIG_ITEM_RW 1
#define PB_CONFIG_ITEM_RO 2
#define PB_CONFIG_ITEM_OTP 3
    const uint8_t access;
    const uint32_t default_value;
} __attribute__ ((packed));

#define PB_CONFIG_MAGIC 0xd276ec0c
struct pb_config_data {
    uint32_t _magic;
    uint32_t crc;
    uint32_t data[127];
};

uint32_t config_init(void);
uint32_t config_get_uint32_t(uint8_t index, uint32_t *value);
uint32_t config_set_uint32_t(uint8_t index, uint32_t value);
uint32_t config_commit(void);
struct pb_config_item * config_get_tbl(void);
uint32_t config_get_tbl_sz(void);

#endif
