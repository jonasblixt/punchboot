#ifndef __PB_BOOT_H__
#define __PB_BOOT_H__

#include <stdint.h>
#include <image.h>
#include <gpt.h>


void pb_boot(struct pb_pbi *pbi, uint32_t system_index);

#endif
