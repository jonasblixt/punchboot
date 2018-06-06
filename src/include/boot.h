/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __BOOT_H__
#define __BOOT_H__

#include <pb.h>

#define PB_BOOT_A 0xAA
#define PB_BOOT_B 0xBB

typedef int32_t bootfunc(void);


void boot_inc_fail_count(uint32_t sys_no);
uint32_t boot_fail_count(uint32_t sys_no);
uint32_t boot_boot_count(void);
uint32_t boot_inc_boot_count(void);
uint32_t boot_load(uint32_t sys_no);
void boot(void);


#endif
