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

#define PB_STATE_MAGIC 0x026d4a65

#define NO_OF_BOARD_REGS 4

struct pb_boot_state /* 512 bytes */
{
    uint32_t magic;                    /*!< PB boot state magic number */
    uint32_t enable;                   /*!< Boot partition enable bits */
    uint32_t verified;                 /*!< Boot partition verified bits */
    uint32_t remaining_boot_attempts;  /*!< Rollback boot counter */
    uint32_t error;                    /*!< Rollback error bits */
    uint8_t rz[472];                   /*!< Reserved, set to zero */
    uint32_t board_regs[NO_OF_BOARD_REGS];    /*!< Board specific registers */
    uint32_t crc;                      /*!< State checksum */
} __attribute__((packed));

#define PB_STATE_A_ENABLED (1 << 0)
#define PB_STATE_B_ENABLED (1 << 1)
#define PB_STATE_A_VERIFIED (1 << 0)
#define PB_STATE_B_VERIFIED (1 << 1)

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

int pbstate_read_board_reg(unsigned int index, uint32_t *value);

int pbstate_write_board_reg(unsigned int index, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif  // TOOLS_pbstate_pbstate_H_
