#ifndef __PB_CONFIG_H__
#define __PB_CONFIG_H__

#include <stdint.h>

void print_configuration(void);

uint32_t pbconfig_load(const char *device, uint64_t primary_offset,
                        uint64_t backup_offset);
uint32_t pbconfig_switch(uint8_t system, uint8_t counter);

uint32_t pbconfig_set_verified(uint8_t system);

#endif
