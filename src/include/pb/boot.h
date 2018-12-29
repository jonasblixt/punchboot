#ifndef __PB_BOOT_H__
#define __PB_BOOT_H__

#include <stdint.h>
#include <image.h>

uint32_t pb_boot_load_part(uint8_t boot_part, struct pb_pbi **pbi);
uint32_t pb_boot_image(struct pb_pbi *pbi);

#endif
