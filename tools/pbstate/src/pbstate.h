/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef TOOLS_pbstate_pbstate_H_
#define TOOLS_pbstate_pbstate_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef int (*pbstate_printfunc_t)(const char *fmt, ...);

typedef enum pbstate_system {
  PBSTATE_SYSTEM_NONE = 0,
  PBSTATE_SYSTEM_A = 1,
  PBSTATE_SYSTEM_B = 2,
} pbstate_system_t;

int pbstate_init(const char *p_device,
                 const char *b_device,
                 pbstate_printfunc_t _printfunc);

/**
 * Check's if system 'system' is active
 *
 * @param[in] system System to check for active status
 *
 * @return 1 System is active,
 *         0 System is inactive,
 *         or a negative number on errors
 */
int pbstate_is_system_active(pbstate_system_t system);

/**
 * Check's if system 'system' is verified
 *
 * @param[in] system System to check for verified status
 *
 * @return 1 System is verified,
 *         0 System is unverified,
 *         or a negative number on errors
 */
int pbstate_is_system_verified(pbstate_system_t system);

/**
 * Reads the current boot attempt counter for unverified systems.
 *
 * @param[out] boot_attempts Number of remaining boot attempts
 *
 * @return 0 On success or a negative number on errors
 */
int pbstate_get_remaining_boot_attempts(unsigned int *boot_attempts);

/**
 * Force a rollback. This function will set the boot attempt counter
 * to zero and when the system reboots the bootloader will trigger a 
 * rollback.
 *
 * If the currently active system has the verify bit set this function
 * will fail since it's not allowed to rollback from a verified system.
 *
 * @return 0, on success
 *     -EPERM, Current system is verified
 *
 */
int pbstate_force_rollback(void);

/**
 * Read the bootloader error state
 *
 * @param[out] error Error register output
 *
 * @return 0 on success or a negative number
 */
int pbstate_get_errors(uint32_t *error);

/**
 * Clear error states
 *
 * @param[in] mask Error bit mask
 *
 * @return 0 on success or a negative number
 */
int pbstate_clear_error(uint32_t mask);

/**
 * Change active system
 *
 * @param[in] system System to switch into
 * @param[in] boot_attempts Number of attempts to boot the target system
 *
 * @return 0 on success or a negative number
 */
int pbstate_switch_system(pbstate_system_t system, uint32_t boot_attempts);

/**
 * Set system verified bit
 *
 * @param[in] system System to set verified bit on
 *
 * @return 0 on success or a negative number
 */
int pbstate_set_system_verified(pbstate_system_t system);

/**
 * Read a board specific register
 *
 * @param[in] index Board register index
 * @param[out] value Register value
 *
 * @return 0 on success
 *    -EINVAL in invalid index
 */
int pbstate_read_board_reg(unsigned int index, uint32_t *value);

/**
 * Write a board specific register
 *
 * @param[in] index Board register index
 * @param[in] value Register value
 *
 * @return 0 on success
 *    -EINVAL on invalid index
 */
int pbstate_write_board_reg(unsigned int index, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif  // TOOLS_pbstate_pbstate_H_
