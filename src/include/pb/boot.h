#ifndef __PB_BOOT_H__
#define __PB_BOOT_H__

#include <stdint.h>
#include <image.h>

#define SYSTEM_A 0xAA
#define SYSTEM_B 0xBB

uint32_t pb_boot_load_part(uint8_t boot_part, struct pb_pbi **pbi);
uint32_t pb_boot_image(struct pb_pbi *pbi, uint8_t system_index);
void pb_boot_linux_with_dt(struct pb_pbi *pbi, uint8_t system_index);

#endif
