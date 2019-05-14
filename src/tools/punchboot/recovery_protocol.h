#ifndef __PUNCHBOOT_RECOVERY_H__
#define __PUNCHBOOT_RECOVERY_H__


#include <stdint.h>
#include <pb/gpt.h>
#include <pb/recovery.h>
#include <pb/params.h>

uint32_t pb_get_version(char **out);
uint32_t pb_install_default_gpt(void);
uint32_t pb_read_uuid(uint8_t *uuid);
uint32_t pb_reset(void);
uint32_t pb_boot_part(uint8_t part_no, bool verbose);
uint32_t pb_get_gpt_table(struct gpt_primary_tbl *tbl);
uint32_t pb_set_bootpart(uint8_t bootpart);
uint32_t pb_flash_part (uint8_t part_no, int64_t offset, const char *f_name);
uint32_t pb_program_bootloader (const char *f_name);
uint32_t pb_execute_image (const char *f_name, uint32_t active_system, 
                            bool verbose);
uint32_t pb_recovery_setup(struct param *params);
uint32_t pb_recovery_setup_lock(void);
uint32_t pb_read_params(struct param **params);
uint32_t pb_recovery_authenticate(uint32_t key_index, const char *fn,
                                  uint32_t signature_kind, uint32_t hash_kind);
#endif
