#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "pb_types.h"

#define PB_CONFIG_BOOT 0x01

u32 config_init(void);
u8  config_get_byte(u32 param);

#endif
