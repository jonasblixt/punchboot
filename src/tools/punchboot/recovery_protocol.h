#ifndef __PUNCHBOOT_RECOVERY_H__
#define __PUNCHBOOT_RECOVERY_H__


#include <stdint.h>
#include <pb/gpt.h>
#include <pb/config.h>

uint32_t pb_get_version(char **out);
uint32_t pb_install_default_gpt(void);
uint32_t pb_write_default_fuse(void);
uint32_t pb_write_uuid(void);
uint32_t pb_read_uuid(uint8_t *uuid);
uint32_t pb_reset(void);
uint32_t pb_boot_part(uint8_t part_no);
uint32_t pb_get_gpt_table(struct gpt_primary_tbl *tbl);
uint32_t pb_get_config_value(uint32_t index, uint32_t *value);
uint32_t pb_set_config_value(uint32_t index, uint32_t val);
uint32_t pb_get_config_tbl (struct pb_config_item *items);
uint32_t pb_flash_part (uint8_t part_no, const char *f_name);
uint32_t pb_program_bootloader (const char *f_name);
uint32_t pb_execute_image (const char *f_name);

#endif
