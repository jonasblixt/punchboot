#ifndef __PUNCHBOOT_RECOVERY_H__
#define __PUNCHBOOT_RECOVERY_H__


#include <stdint.h>
#include <pb/gpt.h>

int pb_print_version(void);
int pb_execute_image (const char *f_name);
int pb_install_default_gpt(void);
int pb_write_default_fuse(void);
int pb_write_uuid(void);
int pb_reset(void);
int pb_boot_part(uint8_t part_no);
int pb_get_gpt_table(struct gpt_primary_tbl *tbl);
unsigned int pb_get_config_value(uint32_t index);
unsigned int pb_set_config_value(uint32_t index, uint32_t val);
int pb_get_config_tbl (void);
int pb_flash_part (uint8_t part_no, const char *f_name);
int pb_program_bootloader (const char *f_name);
int pb_execute_image (const char *f_name);

#endif
