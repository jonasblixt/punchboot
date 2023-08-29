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

#define PB_STATE_ERROR_A_ROLLBACK (1 << 0)
#define PB_STATE_ERROR_B_ROLLBACK (1 << 1)

int pbstate_load(const char *p_device,
                 const char *b_device,
                 pbstate_printfunc_t _printfunc);

bool pbstate_is_system_active(pbstate_system_t system);

bool pbstate_is_system_verified(pbstate_system_t system);

uint32_t pbstate_get_boot_attempts(void);

int pbstate_force_rollback(void);

uint32_t pbstate_get_errors(void);

int pbstate_clear_error(uint32_t error_flag);

int pbstate_switch_system(pbstate_system_t system, uint32_t boot_attempts);

int pbstate_set_system_verified(pbstate_system_t system);

#ifdef __cplusplus
}
#endif

#endif  // TOOLS_pbstate_pbstate_H_
