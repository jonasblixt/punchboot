#ifndef __BOOT_H__
#define __BOOT_H__

#include "pb_types.h"

#define PB_BOOT_A 0xAA
#define PB_BOOT_B 0xBB

typedef int bootfunc(void);


void boot_inc_fail_count(u8 sys_no);
u32 boot_fail_count(u8 sys_no);
u32 boot_boot_count(void);
u32 boot_inc_boot_count(void);
u32 boot_load(u8 sys_no);
void boot(void);


#endif
