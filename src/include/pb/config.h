#ifndef __PB_CONFIG_H__
#define __PB_CONFIG_H__

#include <stdint.h>
#include <stdbool.h>

#define PB_CONFIG_MAGIC 0x026d4a65

#define PB_CONFIG_BOOT_ROLLBACK_ERR (1 << 0)

struct config
{
    uint32_t magic;
    uint8_t a_sys_enable;
    uint8_t b_sys_enable;
    uint8_t a_sys_verified;
    uint8_t b_sys_verified;
    uint8_t a_boot_counter;
    uint8_t b_boot_counter;
    uint32_t a_boot_error;
    uint32_t b_boot_error;
    uint8_t rz[490];
    uint32_t crc;
} __attribute__ ((packed));

#ifdef __PB_BUILD

uint32_t config_init(void);
uint32_t config_commit(void);
bool config_system_enabled(uint32_t system);
void config_system_enable(uint32_t system, bool enable);
bool config_system_verified(uint32_t system);
void config_system_set_verified(uint32_t system, bool verified);
uint32_t config_get_boot_counter(uint32_t system);
void config_set_boot_counter(uint32_t system, uint8_t counter);
void config_set_boot_error_bits(uint32_t system, uint32_t bits);
#endif

#endif
